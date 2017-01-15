#include "text.h"
#include <limits.h>
#include <vector>
#include <memory>

using std::move;
using std::vector;
using std::unique_ptr;
using std::ostream;

TextSlice::TextSlice(Text &text) : text{&text}, start_index{0}, end_index{text.size()} {}

TextSlice::TextSlice(Text *text, size_t start_index, size_t end_index)
    : text{text}, start_index{start_index}, end_index{end_index} {}

Text::iterator TextSlice::begin() {
  return text->begin() + start_index;
}

size_t TextSlice::size() {
  return end_index - start_index;
}

Text::iterator TextSlice::end() {
  return text->begin() + end_index;
}

Text TextSlice::concat(TextSlice a, TextSlice b) {
  Text result;
  result.reserve(a.size() + b.size());
  result.insert(
    result.end(),
    a.begin(),
    a.end()
  );
  result.insert(
    result.end(),
    b.begin(),
    b.end()
  );
  return result;
}

Text TextSlice::concat(TextSlice a, TextSlice b, TextSlice c) {
  Text result;
  result.reserve(a.size() + b.size() + c.size());
  result.insert(
    result.end(),
    a.begin(),
    a.end()
  );
  result.insert(
    result.end(),
    b.begin(),
    b.end()
  );
  result.insert(
    result.end(),
    c.begin(),
    c.end()
  );
  return result;
}

TextSlice::operator Text() const {
  return Text(text->begin() + start_index, text->begin() + end_index);
}

std::pair<TextSlice, TextSlice> TextSlice::split(Point position) {
  size_t index = character_index_for_position(position);
  return {
    TextSlice{text, start_index, start_index + index},
    TextSlice{text, start_index + index, end_index}
  };
}

TextSlice TextSlice::suffix(Point suffix_start) {
  return split(suffix_start).second;
}

TextSlice TextSlice::prefix(Point prefix_end) {
  return split(prefix_end).first;
}

size_t TextSlice::character_index_for_position(Point target) {
  Point position;
  auto begin = text->begin() + start_index;
  auto end = text->begin() + end_index;
  auto iter = begin;
  while (iter != end && position < target) {
    if (*iter == '\n') {
      position.row++;
      position.column = 0;
    } else {
      position.column++;
    }
    ++iter;
  }

  return iter - begin;
}

ostream &operator<<(ostream &stream, const Text *text) {
  if (text) {
    stream << "'";
    for (uint16_t character : *text) {
      if (character < CHAR_MAX) {
        stream << (char)character;
      } else {
        stream << "\\u" << character;
      }
    }
    stream << "'";
    return stream;
  } else {
    return stream << "null";
  }
}
