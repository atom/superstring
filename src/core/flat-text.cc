#include "flat-text.h"

using std::vector;
using std::ostream;
using std::function;

static const uint32_t bytes_per_character = (sizeof(char16_t) / sizeof(char));
static const char16_t replacement_character = 0xFFFD;

FlatText::FlatText(const std::u16string &string) :
  content {string.begin(), string.end()}, line_offsets({ 0 }) {
  uint32_t offset = 0;
  while (offset < content.size()) {
    switch (content[offset]) {
      case '\n':
        line_offsets.push_back(offset + 1);
        break;
    }
    offset++;
  }
}

FlatText::FlatText(const vector<char16_t> &content, const vector<uint32_t> &line_offsets) :
  content {content}, line_offsets {line_offsets} {}

FlatText FlatText::build(std::istream &stream, size_t input_size, const char *encoding_name,
                      size_t chunk_size, function<void(size_t)> progress_callback) {
  iconv_t conversion = iconv_open("UTF-16LE", encoding_name);
  if (conversion == reinterpret_cast<iconv_t>(-1)) {
    return FlatText {{}, {}};
  }

  vector<char> input_vector(chunk_size);
  vector<char16_t> output_vector(input_size);
  vector<uint32_t> line_offsets({ 0 });

  char *input_buffer = input_vector.data();
  char16_t *output_buffer = output_vector.data();
  char *output_pointer = reinterpret_cast<char *>(output_buffer);
  size_t output_bytes_remaining = output_vector.size() * bytes_per_character;
  size_t character_offset = 0;

  size_t total_bytes_read = 0;
  size_t unconverted_byte_count = 0;

  for (;;) {
    stream.read(input_buffer + unconverted_byte_count, chunk_size - unconverted_byte_count);
    size_t bytes_read = stream.gcount();
    size_t bytes_to_convert = unconverted_byte_count + bytes_read;
    total_bytes_read += bytes_read;
    if (bytes_to_convert == 0) break;

    if (bytes_read > 0) progress_callback(total_bytes_read);

    char *input_pointer = input_buffer;
    size_t conversion_result = iconv(
      conversion,
      &input_pointer,
      &bytes_to_convert,
      &output_pointer,
      &output_bytes_remaining
    );

    unconverted_byte_count = 0;
    size_t characters_written =
      output_vector.size() - (output_bytes_remaining / bytes_per_character);

    if (conversion_result == static_cast<size_t>(-1)) {
      switch (errno) {
        case EINVAL:
          unconverted_byte_count = input_buffer + chunk_size - input_pointer;
          memcpy(input_buffer, input_pointer, unconverted_byte_count);
          break;

        case EILSEQ:
          output_vector[characters_written++] = replacement_character;
          output_pointer += bytes_per_character;
          output_bytes_remaining -= bytes_per_character;
          input_pointer++;
          unconverted_byte_count = input_buffer + chunk_size - input_pointer;
          memcpy(input_buffer, input_pointer, unconverted_byte_count);
          break;
      }
    }

    while (character_offset < characters_written) {
      switch (output_vector[character_offset]) {
        case '\n':
          line_offsets.push_back(character_offset + 1);
          break;

        default:
          break;
      }

      character_offset++;
    }
  }

  output_vector.resize(character_offset);
  return FlatText {output_vector, line_offsets};
}

bool FlatText::operator==(const FlatText &other) const {
  return content == other.content && line_offsets == other.line_offsets;
}

ostream &operator<<(ostream &stream, const FlatText &text) {
  for (char16_t character : text.content) {
    if (character < 255) {
      stream << static_cast<char>(character);
    } else {
      stream << "\\u";
      stream << character;
    }
  }

  stream << "\n";

  for (uint32_t line_offset : text.line_offsets) {
    stream << line_offset << " ";
  }

  return stream;
}
