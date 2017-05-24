#include "text.h"
#include <algorithm>
#include "text-slice.h"

using std::function;
using std::move;
using std::ostream;
using std::vector;
using String = Text::String;

Text::Text() : line_offsets{0} {}

Text::Text(vector<uint16_t> &&content) : content{content}, line_offsets{0} {
  for (uint32_t offset = 0, size = content.size(); offset < size; offset++) {
    if (content[offset] == '\n') {
      line_offsets.push_back(offset + 1);
    }
  }
}

Text::Text(const std::u16string &string) :
  Text(vector<uint16_t>{string.begin(), string.end()}) {}

Text::Text(TextSlice slice) :
  content{
    slice.text->content.begin() + slice.start_offset(),
    slice.text->content.begin() + slice.end_offset()
  },
  line_offsets{} {
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

Text::Text(const vector<uint16_t> &&content, const vector<uint32_t> &&line_offsets) :
  content{move(content)}, line_offsets{move(line_offsets)} {}

Text::Text(Deserializer &deserializer) : line_offsets{0} {
  uint32_t size = deserializer.read<uint32_t>();
  content.reserve(size);
  for (uint32_t offset = 0; offset < size; offset++) {
    uint16_t character = deserializer.read<uint16_t>();
    content.push_back(character);
    if (character == '\n') line_offsets.push_back(offset + 1);
  }
}

void Text::serialize(Serializer &serializer) const {
  serializer.append<uint32_t>(size());
  for (uint16_t character : content) {
    serializer.append<uint16_t>(character);
  }
}

Point Text::extent(const std::u16string &string) {
  Point result;
  for (auto c : string) {
    if (c == '\n') {
      result.row++;
      result.column = 0;
    } else {
      result.column++;
    }
  }
  return result;
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

void Text::clear() {
  content.clear();
  line_offsets.assign({0});
}

template<typename T>
void splice_vector(
  std::vector<T> &vector, uint32_t splice_start, uint32_t deletion_size,
  typename std::vector<T>::const_iterator inserted_begin,
  typename std::vector<T>::const_iterator inserted_end
) {
  uint32_t original_size = vector.size();
  uint32_t insertion_size = inserted_end - inserted_begin;
  uint32_t insertion_end = splice_start + insertion_size;
  uint32_t deletion_end = splice_start + deletion_size;
  int64_t size_delta = static_cast<int64_t>(insertion_size) - deletion_size;

  if (size_delta > 0) {
    vector.resize(vector.size() + size_delta);
  }

  if (insertion_end >= deletion_end && insertion_end < original_size) {
    std::copy_backward(
      vector.cbegin() + deletion_end,
      vector.cbegin() + original_size,
      vector.end()
    );
  } else {
    std::copy(
      vector.cbegin() + deletion_end,
      vector.cbegin() + original_size,
      vector.begin() + insertion_end
    );
  }

  std::copy(
    inserted_begin,
    inserted_end,
    vector.begin() + splice_start
  );

  if (size_delta < 0) {
    vector.resize(vector.size() + size_delta);
  }
}

void Text::splice(Point start, Point deletion_extent, TextSlice inserted_slice) {
  uint32_t content_splice_start = offset_for_position(start);
  uint32_t content_splice_end = offset_for_position(start.traverse(deletion_extent));
  uint32_t original_content_size = content.size();
  splice_vector(
    content,
    content_splice_start,
    content_splice_end - content_splice_start,
    inserted_slice.begin(),
    inserted_slice.end()
  );

  splice_vector(
    line_offsets,
    start.row + 1,
    deletion_extent.row,
    inserted_slice.text->line_offsets.begin() + inserted_slice.start_position.row + 1,
    inserted_slice.text->line_offsets.begin() + inserted_slice.end_position.row + 1
  );

  uint32_t inserted_newlines_start = start.row + 1;
  uint32_t inserted_newlines_end = start.row + inserted_slice.extent().row + 1;
  int64_t inserted_line_offsets_delta = content_splice_start - static_cast<int64_t>(inserted_slice.start_offset());
  for (size_t i = inserted_newlines_start; i < inserted_newlines_end; i++) {
    line_offsets[i] += inserted_line_offsets_delta;
  }

  uint32_t content_size = content.size();
  int64_t trailing_line_offsets_delta = static_cast<int64_t>(content_size) - original_content_size;
  for (auto iter = line_offsets.begin() + inserted_newlines_end; iter != line_offsets.end(); ++iter) {
    *iter += trailing_line_offsets_delta;
  }
}

uint16_t Text::at(uint32_t offset) const {
  return content[offset];
}

uint16_t Text::at(Point position) const {
  return at(offset_for_position(position));
}

ClipResult Text::clip_position(Point position) const {
  uint32_t row = position.row;
  if (row >= line_offsets.size()) {
    return {extent(), size()};
  } else {
    uint32_t start = line_offsets[row];
    uint32_t end;
    if (row == line_offsets.size() - 1) {
      end = content.size();
    } else {
      end = line_offsets[row + 1] - 1;
      if (end > 0 && content[end - 1] == '\r') {
        end--;
      }
    }
    if (position.column > end - start) {
      return {Point(row, end - start), end};
    } else {
      return {position, start + position.column};
    }
  }
}

uint32_t Text::offset_for_position(Point position) const {
  return clip_position(position).offset;
}

Point Text::position_for_offset(uint32_t offset, bool clip_crlf) const {
  if (offset > size()) offset = size();
  auto line_offsets_begin = line_offsets.begin();
  auto line_offset = std::upper_bound(line_offsets_begin, line_offsets.end(), offset);
  if (line_offset != line_offsets_begin) line_offset--;
  uint32_t row = line_offset - line_offsets_begin;
  uint32_t column = offset - *line_offset;
  if (clip_crlf && offset > 0 && offset < size() && at(offset) == '\n' && at(offset - 1) == '\r') {
    column--;
  }
  return Point(row, column);
}

uint32_t Text::line_length_for_row(uint32_t row) const {
  return clip_position(Point{row, UINT32_MAX}).position.column;
}

Text::const_iterator Text::begin() const {
  return content.cbegin();
}

Text::const_iterator Text::end() const {
  return content.cend();
}

uint32_t Text::size() const {
  return content.size();
}

const uint16_t *Text::data() const {
  return content.data();
}

Point Text::extent() const {
  return Point(line_offsets.size() - 1, content.size() - line_offsets.back());
}

bool Text::empty() const {
  return content.empty();
}

template <typename T>
inline void hash_combine(std::size_t &seed, const T &value) {
  std::hash<T> hasher;
  seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

size_t Text::digest() const {
  size_t result = 0;
  for (uint16_t character : content) {
    hash_combine(result, character);
  }
  return result;
}

void Text::append(TextSlice slice) {
  int64_t line_offset_delta = static_cast<int64_t>(content.size()) - static_cast<int64_t>(slice.start_offset());

  content.insert(
    content.end(),
    slice.begin(),
    slice.end()
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

void Text::assign(TextSlice slice) {
  uint32_t slice_start_offset = slice.start_offset();

  content.assign(
    slice.begin(),
    slice.end()
  );

  line_offsets.assign({0});
  line_offsets.insert(
    line_offsets.end(),
    slice.text->line_offsets.begin() + slice.start_position.row + 1,
    slice.text->line_offsets.begin() + slice.end_position.row + 1
  );

  for (size_t i = 1; i < line_offsets.size(); i++) {
    line_offsets[i] -= slice_start_offset;
  }
}

bool Text::operator!=(const Text &other) const {
  return content != other.content;
}

bool Text::operator==(const Text &other) const {
  return content == other.content;
}

ostream &operator<<(ostream &stream, const Text &text) {
  for (uint16_t character : text.content) {
    if (character == '\r') {
      stream << "\\r";
    } else if (character == '\n') {
      stream << "\\n";
    } else if (character < 255) {
      stream << static_cast<char>(character);
    } else {
      stream << "\\u";
      stream << character;
    }
  }

  return stream;
}
