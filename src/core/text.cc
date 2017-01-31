#include "text.h"
#include "text-slice.h"

using std::vector;
using std::ostream;
using std::function;

static const uint32_t bytes_per_character = (sizeof(uint16_t) / sizeof(char));
static const uint16_t replacement_character = 0xFFFD;
static const float buffer_growth_factor = 2;

Text::Text() : line_offsets {0} {}

Text::Text(const Text &other) :
  content {other.content}, line_offsets {other.line_offsets} {}

Text::Text(vector<uint16_t> &&content) : content {content}, line_offsets {0} {
  uint32_t offset = 0;
  while (offset < content.size()) {
    switch (content[offset]) {
      case '\n':
        line_offsets.push_back(offset + 1);
        offset += 1;
        break;
      case '\r':
        if (offset < content.size() - 1 && content[offset + 1] == '\n') {
          line_offsets.push_back(offset + 2);
          offset += 2;
        } else {
          line_offsets.push_back(offset + 1);
          offset += 1;
        }
        break;
      default:
        offset += 1;
        break;
    }
  }
}

Text::Text(const std::u16string &string) :
  Text(vector<uint16_t> {string.begin(), string.end()}) {}

Text::Text(TextSlice slice) :
  content {
    slice.text->content.begin() + slice.start_offset(),
    slice.text->content.begin() + slice.end_offset()
  }, line_offsets {} {

  line_offsets.push_back(slice.start_offset());
  line_offsets.insert(
    line_offsets.end(),
    slice.text->line_offsets.begin() + slice.start_position.row + 1,
    slice.text->line_offsets.begin() + slice.end_position.row + 1
  );

  for (uint32_t &line_offset : line_offsets) {
    line_offset -= slice.start_offset();
  }
}

Text::Text(const vector<uint16_t> &content, const vector<uint32_t> &line_offsets) :
  content {content}, line_offsets {line_offsets} {}

Text::Text(Serializer &serializer) : line_offsets {0} {
  uint32_t size = serializer.read<uint32_t>();
  content.reserve(size);
  uint32_t offset = 0;
  while (offset < size) {
    uint16_t character = serializer.read<uint16_t>();
    content.push_back(character);
    switch (character) {
      case '\n':
        line_offsets.push_back(offset + 1);
        offset += 1;
        break;
      case '\r':
        if (offset < size - 1 && serializer.peek<uint16_t>() == '\n') {
          serializer.read<uint16_t>();
          line_offsets.push_back(offset + 2);
          offset += 2;
        } else {
          line_offsets.push_back(offset + 1);
          offset += 1;
        }
        break;
      default:
        offset += 1;
        break;
    }
  }
}

void Text::serialize(Serializer &serializer) const {
  serializer.append<uint32_t>(size());
  for (uint16_t character : content) {
    serializer.append<uint16_t>(character);
  }
}

Text Text::build(std::istream &stream, size_t input_size, const char *encoding_name,
                      size_t chunk_size, function<void(size_t)> progress_callback) {
  iconv_t conversion = iconv_open("UTF-16LE", encoding_name);
  if (conversion == reinterpret_cast<iconv_t>(-1)) {
    return Text {{}, {}};
  }

  vector<char> input_vector(chunk_size);
  vector<uint16_t> output_vector(input_size);
  vector<uint32_t> line_offsets({ 0 });

  size_t total_bytes_read = 0;
  size_t indexed_character_count = 0;
  bool pending_carriage_return = false;

  char *input_buffer = input_vector.data();
  size_t input_bytes_remaining = 0;

  char *output_buffer = reinterpret_cast<char *>(output_vector.data());
  char *output_pointer = output_buffer;
  size_t output_bytes_remaining = output_vector.size() * bytes_per_character;

  for (;;) {
    stream.read(input_buffer + input_bytes_remaining, chunk_size - input_bytes_remaining);
    size_t bytes_read = stream.gcount();
    input_bytes_remaining += bytes_read;
    if (input_bytes_remaining == 0) break;

    if (bytes_read > 0) {
      total_bytes_read += bytes_read;
      progress_callback(total_bytes_read);
    }

    char *input_pointer = input_buffer;
    size_t conversion_result = iconv(
      conversion,
      &input_pointer,
      &input_bytes_remaining,
      &output_pointer,
      &output_bytes_remaining
    );

    size_t output_characters_remaining = (output_bytes_remaining / bytes_per_character);
    size_t output_characters_written = output_vector.size() - output_characters_remaining;

    if (conversion_result == static_cast<size_t>(-1)) {
      switch (errno) {
        // Encountered an incomplete multibyte sequence at end of input
        case EINVAL:
          if (bytes_read > 0) break;

        // Encountered an invalid multibyte sequence
        case EILSEQ:
          input_bytes_remaining--;
          input_pointer++;
          output_vector[output_characters_written] = replacement_character;
          output_pointer += bytes_per_character;
          output_bytes_remaining -= bytes_per_character;
          break;

        // Insufficient room in the output buffer to write all characters in the input buffer
        case E2BIG:
          size_t old_size = output_vector.size();
          size_t new_size = old_size * buffer_growth_factor;
          output_vector.resize(new_size);
          output_bytes_remaining += ((new_size - old_size) * bytes_per_character);
          output_buffer = reinterpret_cast<char *>(output_vector.data());
          output_pointer = output_buffer + (output_characters_written * bytes_per_character);
          break;
      }

      memcpy(input_buffer, input_pointer, input_bytes_remaining);
    }

    while (indexed_character_count < output_characters_written) {
      switch (output_vector[indexed_character_count]) {
        case '\n':
          pending_carriage_return = false;
          line_offsets.push_back(indexed_character_count + 1);
          break;
        case '\r':
          if (pending_carriage_return) {
            line_offsets.push_back(indexed_character_count);
          }
          pending_carriage_return = true;
          break;
        default:
          if (pending_carriage_return) {
            line_offsets.push_back(indexed_character_count);
            pending_carriage_return = false;
          }
          break;
      }

      indexed_character_count++;
    }
  }

  if (pending_carriage_return) {
    line_offsets.push_back(indexed_character_count);
  }

  size_t output_characters_remaining = (output_bytes_remaining / bytes_per_character);
  size_t output_characters_written = output_vector.size() - output_characters_remaining;
  output_vector.resize(output_characters_written);
  return Text {output_vector, line_offsets};
}

Text Text::concat(TextSlice a, TextSlice b) {
  Text result;
  result.append(a);
  result.append(b);
  return result;
}

Text Text::concat(TextSlice a, TextSlice b, TextSlice c) {
  Text result;
  result.append(a);
  result.append(b);
  result.append(c);
  return result;
}

std::pair<Text::const_iterator, Text::const_iterator> Text::line_iterators(uint32_t row) const {
  const_iterator begin = content.cbegin() + line_offsets[row];

  const_iterator end;
  if (row < line_offsets.size() - 1) {
    end = content.cbegin() + line_offsets[row + 1] - 1;
    if (*(end - 1) == '\r') {
      --end;
    }
  } else {
    end = content.end();
  }

  return std::pair<const_iterator, const_iterator>(begin, end);
}

Text::const_iterator Text::cbegin() const {
  return content.cbegin();
}

Text::const_iterator Text::cend() const {
  return content.cend();
}

size_t Text::size() const {
  return content.size();
}

const uint16_t *Text::data() const {
  return content.data();
}

Point Text::extent() const {
  return Point(line_offsets.size() - 1, content.size() - line_offsets.back());
}

void Text::append(TextSlice slice) {
  int64_t line_offset_delta = static_cast<int64_t>(content.size()) - static_cast<int64_t>(slice.start_offset());

  content.insert(
    content.end(),
    slice.cbegin(),
    slice.cend()
  );

  size_t original_size = line_offsets.size();
  line_offsets.insert(
    line_offsets.end(),
    slice.text->line_offsets.begin() + slice.start_position.row + 1,
    slice.text->line_offsets.begin() + slice.end_position.row + 1
  );

  for (size_t i = original_size; i < line_offsets.size(); i++) {
    line_offsets[i] += line_offset_delta;
  }
}

bool Text::operator==(const Text &other) const {
  return content == other.content && line_offsets == other.line_offsets;
}

ostream &operator<<(ostream &stream, const Text &text) {
  for (uint16_t character : text.content) {
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
