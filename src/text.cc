#include "text.h"
#include <cstring>

Text *Text::Concat(TextSlice a, TextSlice b) {
  Text *result = new Text{a.length + b.length};
  memcpy(result->content, a.content, a.length * sizeof(uint16_t));
  memcpy(result->content + a.length, b.content, b.length * sizeof(uint16_t));
  return result;
}

Text *Text::Concat(TextSlice a, TextSlice b, TextSlice c) {
  Text *result = new Text{a.length + b.length + c.length};
  memcpy(result->content, a.content, a.length * sizeof(uint16_t));
  memcpy(result->content + a.length, b.content, b.length * sizeof(uint16_t));
  memcpy(result->content + a.length + b.length, c.content, c.length * sizeof(uint16_t));
  return result;
}

Text::Text(uint32_t length) : length{length} {
  content = new uint16_t[length];
}

Text::Text(TextSlice slice) : length{slice.length} {
  content = new uint16_t[length];
  memcpy(content, slice.content, length * sizeof(uint16_t));
}

Text::~Text() {
  delete[] content;
}

TextSlice Text::Prefix(Point prefix_end) const {
  uint32_t end_index = CharacterIndexForPosition(prefix_end);
  return TextSlice{content, end_index};
}

TextSlice Text::Suffix(Point suffix_start) const {
  uint32_t start_index = CharacterIndexForPosition(suffix_start);
  return TextSlice{content + start_index, length - start_index};
}

TextSlice Text::AsSlice() const {
  return TextSlice{content, length};
}

void Text::TrimLeft(Point position) {
  uint32_t new_start_index = CharacterIndexForPosition(position);
  memmove(content, content + new_start_index, (length - new_start_index) * sizeof(uint16_t));
  length -= new_start_index;
}

void Text::TrimRight(Point position) {
  length = CharacterIndexForPosition(position);
}

uint32_t Text::CharacterIndexForPosition(Point target) const {
  uint32_t index = 0;
  Point position;

  while (index < length && position < target) {
    if (content[index] == '\n') {
      position.row++;
      position.column = 0;
    } else {
      position.column++;
    }
    index++;
  }

  return index;
}
