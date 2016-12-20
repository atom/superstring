#include "text.h"
#include <vector>
#include <memory>

using std::move;
using std::vector;
using std::unique_ptr;

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

Text TextSlice::Concat(TextSlice a, TextSlice b) {
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

Text TextSlice::Concat(TextSlice a, TextSlice b, TextSlice c) {
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

std::pair<TextSlice, TextSlice> TextSlice::Split(Point position) {
  size_t index = CharacterIndexForPosition(position);
  return {
    TextSlice{text, start_index, start_index + index},
    TextSlice{text, start_index + index, end_index}
  };
}

TextSlice TextSlice::Suffix(Point suffix_start) {
  return Split(suffix_start).second;
}

TextSlice TextSlice::Prefix(Point prefix_end) {
  return Split(prefix_end).first;
}

size_t TextSlice::CharacterIndexForPosition(Point target) {
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
