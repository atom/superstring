#include "encoding-conversion.h"
#include <iconv.h>

using std::function;
using std::vector;
using String = Text::String;

static const uint32_t bytes_per_character = (sizeof(uint16_t) / sizeof(char));
static const uint16_t replacement_character = 0xFFFD;
static const size_t conversion_failure = static_cast<size_t>(-1);
static const float buffer_growth_factor = 2;

optional<EncodingConversion> transcoding_to(const char *name) {
  iconv_t conversion = iconv_open(name, "UTF-16LE");
  return conversion == reinterpret_cast<iconv_t>(-1) ?
    optional<EncodingConversion>{} :
    optional<EncodingConversion>{conversion};
}

optional<EncodingConversion> transcoding_from(const char *name) {
  iconv_t conversion = iconv_open("UTF-16LE", name);
  return conversion == reinterpret_cast<iconv_t>(-1) ?
    optional<EncodingConversion>{} :
    EncodingConversion(conversion);
}

EncodingConversion::EncodingConversion(EncodingConversion &&other) :
  data{other.data} {other.data = nullptr;}

EncodingConversion::EncodingConversion() :
  data{nullptr} {}

EncodingConversion::EncodingConversion(void *data) :
  data{data} {}

EncodingConversion::~EncodingConversion() {
  if (data) iconv_close(data);
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

size_t EncodingConversion::decode(String &string, const char *input_bytes,
                                  size_t input_length, bool is_last_chunk) {
  size_t previous_size = string.size();
  string.resize(previous_size + input_length);

  size_t new_size = previous_size;
  size_t input_bytes_remaining = input_length;
  size_t output_bytes_remaining = input_length * bytes_per_character;
  char *input_pointer = const_cast<char *>(input_bytes);
  char *output_pointer = reinterpret_cast<char *>(string.data() + previous_size);
  bool incomplete_sequence_at_chunk_end = false;

  while (input_bytes_remaining > 0 && !incomplete_sequence_at_chunk_end) {
    size_t conversion_result = iconv(
      data,
      &input_pointer,
      &input_bytes_remaining,
      &output_pointer,
      &output_bytes_remaining
    );

    new_size = string.size() - output_bytes_remaining / bytes_per_character;

    if (conversion_result == conversion_failure) {
      switch (errno) {
        // Encountered an incomplete multibyte sequence at end of the chunk. If
        // this is not the last chunk, then stop here, because maybe the
        // remainder of the character will occur at the beginning of the next
        // chunk. If this *is* the last chunk, then we must consume these bytes,
        // so we fall through to the next case and append the unicode
        // replacement character.
        case EINVAL:
          if (!is_last_chunk) {
            incomplete_sequence_at_chunk_end = true;
            break;
          }

        // Encountered an invalid multibyte sequence. Append the unicode
        // replacement character and resume transcoding.
        case EILSEQ:
          input_bytes_remaining--;
          input_pointer++;
          string[new_size] = replacement_character;
          output_pointer += bytes_per_character;
          output_bytes_remaining -= bytes_per_character;
          new_size++;
          break;

        // Insufficient room in the output buffer to write all characters in the
        // input buffer. Grow the content vector and resume transcoding.
        case E2BIG:
          size_t old_size = string.size();
          size_t new_size = old_size * buffer_growth_factor;
          string.resize(new_size);
          output_bytes_remaining += ((new_size - old_size) * bytes_per_character);
          output_pointer = reinterpret_cast<char *>(string.data() + new_size);
          break;
      }
    }
  }

  string.resize(new_size);
  return input_pointer - input_bytes;
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
  const char *input_buffer = reinterpret_cast<const char *>(string.data() + *start_offset);
  char *input_pointer = const_cast<char *>(input_buffer);
  size_t input_bytes_remaining = (end_offset - *start_offset) * bytes_per_character;
  char *output_pointer = output_buffer;
  size_t output_bytes_remaining = output_length;

  bool done = false;
  while (!done) {
    size_t conversion_result = iconv(
      data,
      &input_pointer,
      &input_bytes_remaining,
      &output_pointer,
      &output_bytes_remaining
    );

    if (conversion_result == conversion_failure) {
      switch (errno) {
        // Encountered an incomplete multibyte sequence at end of input.
        case EINVAL: {
          done = true;
          if (!is_at_end) break;
        }

        // Encountered an invalid character in the input Text. Write the unicode
        // 'replacement character' to the output buffer in the given encoding.
        // If there's enough space to hold the replacement character, then skip
        // the invalid input character. Otherwise, stop here; we'll try again
        // on the next iteration with an empty output buffer.
        case EILSEQ: {
          uint16_t replacement_characters[] = {replacement_character, 0};
          char *replacement_text = reinterpret_cast<char *>(replacement_characters);
          size_t replacement_text_size = bytes_per_character;
          if (iconv(
            data,
            &replacement_text,
            &replacement_text_size,
            &output_pointer,
            &output_bytes_remaining
          ) == conversion_failure) {
            done = true;
          } else {
            input_bytes_remaining -= bytes_per_character;
            input_pointer += bytes_per_character;
          }
          break;
        }

        // Insufficient room in the output buffer to write all characters in the input buffer
        case E2BIG: {
          done = true;
          break;
        }
      }
    } else {
      done = true;
    }
  }

  *start_offset += (input_pointer - input_buffer) / bytes_per_character;
  return output_pointer - output_buffer;
}
