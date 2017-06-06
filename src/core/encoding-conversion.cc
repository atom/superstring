#include "encoding-conversion.h"
#include <iconv.h>
#include <codecvt>
#include <string.h>

using std::codecvt_base;
using std::codecvt_utf8_utf16;
using std::function;
using std::vector;
using String = Text::String;

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

// In the common case of transcoding to/from UTF8, we can use the standard
// library's transcoding routines, which are significantly faster than iconv.
struct UTF8Converter {
  codecvt_utf8_utf16<char16_t> facet;
  std::mbstate_t state;
};

optional<EncodingConversion> transcoding_to(const char *name) {
  if (strcmp(name, "UTF-8") == 0) {
    return EncodingConversion{UTF16_TO_UTF8, new UTF8Converter()};
  } else {
    iconv_t conversion = iconv_open(name, "UTF-16LE");
    return conversion == reinterpret_cast<iconv_t>(-1) ?
      optional<EncodingConversion>{} :
      EncodingConversion(GENERAL, conversion);
  }
}

optional<EncodingConversion> transcoding_from(const char *name) {
  if (strcmp(name, "UTF-8") == 0) {
    return EncodingConversion{UTF8_TO_UTF16, new UTF8Converter()};
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
  if (data) {
    switch (mode) {
      case GENERAL:
        iconv_close(data);
        break;

      case UTF16_TO_UTF8:
      case UTF8_TO_UTF16:
        delete static_cast<UTF8Converter *>(data);
        break;
    }
  }
}

int EncodingConversion::convert(
  const char **input, const char *input_end, char **output, char *output_end) const {
  switch (mode) {
    case UTF8_TO_UTF16: {
      auto converter = static_cast<UTF8Converter *>(data);
      const char *next_input;
      char16_t *next_output;
      int result = converter->facet.in(
        converter->state,
        *input,
        input_end,
        next_input,
        reinterpret_cast<char16_t *>(*output),
        reinterpret_cast<char16_t *>(output_end),
        next_output
      );
      *input = next_input;
      *output = reinterpret_cast<char *>(next_output);
      switch (result) {
        case codecvt_base::ok:
          return Ok;
        case codecvt_base::partial:
          return (*input == input_end) ? Partial : InvalidTrailing;
        case codecvt_base::error:
          return Invalid;
      }
    }

    case UTF16_TO_UTF8: {
      auto converter = static_cast<UTF8Converter *>(data);
      const char *input_start = *input;
      const char16_t *next_input;
      char *next_output;
      int result = converter->facet.out(
        converter->state,
        reinterpret_cast<const char16_t *>(*input),
        reinterpret_cast<const char16_t *>(input_end),
        next_input,
        *output,
        output_end,
        next_output
      );
      *input = reinterpret_cast<const char *>(next_input);
      *output = next_output;
      switch (result) {
        case codecvt_base::ok:
          // When using GCC and libstdc++, `codecvt_utf8_utf16::out` seems to
          // incorrectly return `ok` when there is an incomplete multi-byte
          // sequence at the end of the input chunk. But it correctly does
          // not advance the input pointer, so we can distinguish this
          // situation from an actual successful result.
          if (*input == input_start && input_start < input_end) return InvalidTrailing;

          return Ok;
        case codecvt_base::partial:
          return (*input == input_end) ? Partial : InvalidTrailing;
        case codecvt_base::error:
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

bool EncodingConversion::decode(String &string, std::istream &stream,
                                vector<char> &input_vector,
                                function<void(size_t)> progress_callback) {
  char *input_buffer = input_vector.data();
  size_t bytes_left_over = 0;
  size_t total_bytes_read = 0;

  for (;;) {
    errno = 0;
    stream.read(input_buffer + bytes_left_over, input_vector.size() - bytes_left_over);
    if (!stream && errno != 0) return false;
    size_t bytes_read = stream.gcount();
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

size_t EncodingConversion::decode(String &string, const char *input_start,
                                  size_t input_length, bool is_last_chunk) {
  size_t previous_size = string.size();
  string.resize(previous_size + input_length);

  size_t new_size = previous_size;
  const char *input_pointer = input_start;
  const char *input_end = input_start + input_length;
  char *output_start = reinterpret_cast<char *>(string.data());
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
        output_start = reinterpret_cast<char *>(string.data());
        output_pointer = output_start + old_size;
        output_end = output_start + new_size;
        break;
    }
  }

  string.resize(new_size);
  return input_pointer - input_start;
}

bool EncodingConversion::encode(const String &string, size_t start_offset,
                                size_t end_offset, std::ostream &stream,
                                vector<char> &output_vector) {
  char *output_buffer = output_vector.data();

  bool end = false;
  for (;;) {
    size_t output_bytes_written = encode(
      string,
      &start_offset,
      end_offset,
      output_vector.data(),
      output_vector.size(),
      end
    );
    errno = 0;
    stream.write(output_buffer, output_bytes_written);
    if (end) break;
    if (output_bytes_written == 0) end = true;
    if (!stream && errno != 0) return false;
  }

  return true;
}

size_t EncodingConversion::encode(const String &string, size_t *start_offset,
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
