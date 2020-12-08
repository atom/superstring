#include "encoding-conversion.h"
#include "utf8-conversions.h"
#include <iconv.h>
#include <cstring>

using std::function;
using std::u16string;
using std::vector;

static const uint32_t bytes_per_character = (sizeof(uint16_t) / sizeof(char));
static const uint16_t replacement_character = 0xFFFD;
static const size_t conversion_failure = static_cast<size_t>(-1);
static const float buffer_growth_factor = 2;

enum Mode {
  GENERAL,
  UTF16_TO_UTF8,
  UTF8_TO_UTF16,
};

enum ConversionResult {
  Ok,
  Partial,
  Invalid,
  InvalidTrailing,
  Error,
};

optional<EncodingConversion> transcoding_to(const char *name) {
  if (strcmp(name, "UTF-8") == 0) {
    return EncodingConversion{UTF16_TO_UTF8, nullptr};
  } else {
    iconv_t conversion = iconv_open(name, "UTF-16LE");
    return conversion == reinterpret_cast<iconv_t>(-1) ?
      optional<EncodingConversion>{} :
      EncodingConversion(GENERAL, conversion);
  }
}

optional<EncodingConversion> transcoding_from(const char *name) {
  if (strcmp(name, "UTF-8") == 0) {
    return EncodingConversion{UTF8_TO_UTF16, nullptr};
  } else {
    iconv_t conversion = iconv_open("UTF-16LE", name);
    return conversion == reinterpret_cast<iconv_t>(-1) ?
      optional<EncodingConversion>{} :
      EncodingConversion(GENERAL, conversion);
  }
}

EncodingConversion::EncodingConversion(EncodingConversion &&other) :
  data{other.data}, mode{other.mode} {
  other.mode = GENERAL;
  other.data = nullptr;
}

EncodingConversion::EncodingConversion() :
  data{nullptr}, mode{GENERAL} {}

EncodingConversion::EncodingConversion(int mode, void *data) :
  data{data}, mode{mode} {}

EncodingConversion::~EncodingConversion() {
  if (data) iconv_close(data);
}

int EncodingConversion::convert(
  const char **input, const char *input_end, char **output, char *output_end) const {
  switch (mode) {
    case UTF8_TO_UTF16: {
      const uint8_t *next_input;
      uint16_t *next_output;
      int result = utf8_to_utf16(
        reinterpret_cast<const uint8_t *>(*input),
        reinterpret_cast<const uint8_t *>(input_end),
        next_input,
        reinterpret_cast<uint16_t *>(*output),
        reinterpret_cast<uint16_t *>(output_end),
        next_output
      );
      *input = reinterpret_cast<const char *>(next_input);
      *output = reinterpret_cast<char *>(next_output);
      switch (result) {
        case transcode_result::ok:
          return Ok;
        case transcode_result::partial:
          return (*input == input_end) ? Partial : InvalidTrailing;
        case transcode_result::error:
          return Invalid;
      }
    }

    case UTF16_TO_UTF8: {
      const uint16_t *next_input;
      uint8_t *next_output;
      int result = utf16_to_utf8(
        reinterpret_cast<const uint16_t *>(*input),
        reinterpret_cast<const uint16_t *>(input_end),
        next_input,
        reinterpret_cast<uint8_t *>(*output),
        reinterpret_cast<uint8_t *>(output_end),
        next_output
      );
      *input = reinterpret_cast<const char *>(next_input);
      *output = reinterpret_cast<char *>(next_output);
      switch (result) {
        case transcode_result::ok:
          return Ok;
        case transcode_result::partial:
          return (*input == input_end) ? Partial : InvalidTrailing;
        case transcode_result::error:
          return Invalid;
      }
    }

    default: {
      auto converter = static_cast<iconv_t *>(data);
      size_t input_length = input_end - *input;
      size_t output_length = output_end - *output;
      auto conversion_result = iconv(
        converter,
        const_cast<char **>(input),
        &input_length,
        output,
        &output_length
      );
      if (conversion_result == conversion_failure) {
        switch (errno) {
          case EINVAL: return InvalidTrailing;
          case EILSEQ: return Invalid;
          case E2BIG: return Partial;
        }
      } else {
        return Ok;
      }
    }
  }

  return Error;
}

bool EncodingConversion::decode(u16string &string, FILE *stream,
                                vector<char> &input_vector,
                                function<void(size_t)> progress_callback) {
  char *input_buffer = input_vector.data();
  size_t bytes_left_over = 0;
  size_t total_bytes_read = 0;

  for (;;) {
    size_t bytes_to_read = input_vector.size() - bytes_left_over;
    size_t bytes_read = fread(input_buffer + bytes_left_over, 1, bytes_to_read, stream);
    if (bytes_read < bytes_to_read && ferror(stream)) return false;
    size_t bytes_to_append = bytes_left_over + bytes_read;
    if (bytes_to_append == 0) break;

    size_t bytes_appended = decode(
      string,
      input_buffer,
      bytes_to_append,
      bytes_read == 0
    );

    total_bytes_read += bytes_appended;
    progress_callback(total_bytes_read);

    if (bytes_appended < bytes_to_append) {
      std::copy(input_buffer + bytes_appended, input_buffer + bytes_to_append, input_buffer);
    }

    bytes_left_over = bytes_to_append - bytes_appended;
  }

  return true;
}

size_t EncodingConversion::decode(u16string &string, const char *input_start,
                                  size_t input_length, bool is_last_chunk) {
  size_t previous_size = string.size();
  string.resize(previous_size + input_length);

  size_t new_size = previous_size;
  const char *input_pointer = input_start;
  const char *input_end = input_start + input_length;
  char *output_start = reinterpret_cast<char *>(&string[0]);
  char *output_pointer = output_start + previous_size * bytes_per_character;
  char *output_end = output_pointer + input_length * bytes_per_character;
  bool incomplete_sequence_at_chunk_end = false;

  while (input_pointer < input_end && !incomplete_sequence_at_chunk_end) {
    int conversion_result = convert(
      &input_pointer,
      input_end,
      &output_pointer,
      output_end
    );

    new_size = (output_pointer - output_start) / bytes_per_character;

    switch (conversion_result) {
      case Ok: break;

      // Encountered an incomplete multibyte sequence at end of the chunk. If
      // this is not the last chunk, then stop here, because maybe the
      // remainder of the character will occur at the beginning of the next
      // chunk. If this *is* the last chunk, then we must consume these bytes,
      // so we fall through to the next case and append the unicode
      // replacement character.
      case InvalidTrailing:
        if (!is_last_chunk) {
          incomplete_sequence_at_chunk_end = true;
          break;
        }

      // Encountered an invalid multibyte sequence. Append the unicode
      // replacement character and resume transcoding.
      case Invalid:
        input_pointer++;
        string[new_size] = replacement_character;
        output_pointer += bytes_per_character;
        new_size++;
        break;

      // Insufficient room in the output buffer to write all characters in the
      // input buffer. Grow the content vector and resume transcoding.
      case Partial:
        size_t old_size = string.size();
        size_t new_size = old_size * buffer_growth_factor;
        string.resize(new_size);
        output_start = reinterpret_cast<char *>(&string[0]);
        output_pointer = output_start + old_size;
        output_end = output_start + new_size;
        break;
    }
  }

  string.resize(new_size);
  return input_pointer - input_start;
}

bool EncodingConversion::encode(const u16string &string, size_t start_offset,
                                size_t end_offset, FILE *stream,
                                vector<char> &output_vector) {
  char *output_buffer = output_vector.data();
  bool end = false;
  while (start_offset < end_offset) {
    size_t bytes_encoded = encode(
      string,
      &start_offset,
      end_offset,
      output_vector.data(),
      output_vector.size(),
      end
    );
    if (bytes_encoded == 0) {
      if (start_offset == end_offset) {
        end = true;
      } else {
        return false;
      }
    }
    size_t bytes_written = fwrite(output_buffer, 1, bytes_encoded, stream);
    if (bytes_written < bytes_encoded && ferror(stream)) return false;
  }

  return true;
}

size_t EncodingConversion::encode(const u16string &string, size_t *start_offset,
                                  size_t end_offset, char *output_buffer,
                                  size_t output_length, bool is_at_end) {
  const char *input_start = reinterpret_cast<const char *>(string.data() + *start_offset);
  const char *input_end = reinterpret_cast<const char *>(string.data() + end_offset);
  const char *input_pointer = input_start;
  char *output_pointer = output_buffer;
  char *output_end = output_buffer + output_length;

  bool done = false;
  while (!done) {
    int conversion_result = convert(
      &input_pointer,
      input_end,
      &output_pointer,
      output_end
    );

    switch (conversion_result) {
      case Ok:
      case Partial:
        done = true;
        break;

      case InvalidTrailing:
        done = true;
        if (!is_at_end) break;

      case Invalid:
        uint16_t replacement_characters[] = {replacement_character, 0};
        const char *replacement_text = reinterpret_cast<const char *>(replacement_characters);
        if (convert(
          &replacement_text,
          replacement_text + bytes_per_character,
          &output_pointer,
          output_end
        ) != Ok) {
          done = true;
        } else {
          input_pointer += bytes_per_character;
        }
        break;
    }
  }

  *start_offset += (input_pointer - input_start) / bytes_per_character;
  return output_pointer - output_buffer;
}
