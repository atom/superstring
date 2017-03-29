#include "text-slice.h"
#include "text.h"
#include <assert.h>

TextSlice::TextSlice(const Text *text, Point start_position, Point end_position) :
  text {text}, start_position {start_position}, end_position {end_position} {}

TextSlice::TextSlice(const Text &text) :
  text {&text}, start_position {Point()}, end_position {text.extent()} {}

size_t TextSlice::start_offset() const {
  assert(start_position.row < text->line_offsets.size());
  return text->line_offsets[start_position.row] + start_position.column;
}

size_t TextSlice::end_offset() const {
  return text->line_offsets[end_position.row] + end_position.column;
}

std::pair<TextSlice, TextSlice> TextSlice::split(Point split_point) const {
  Point absolute_split_point = Point::min(
    text->extent(),
    start_position.traverse(split_point)
  );

  return std::pair<TextSlice, TextSlice> {
    TextSlice {text, start_position, absolute_split_point},
    TextSlice {text, absolute_split_point, end_position}
  };
}

Point TextSlice::position_for_offset(uint32_t offset) const {
  return text->position_for_offset(offset + start_offset()).traversal(start_position);
}

TextSlice TextSlice::prefix(Point prefix_end) const {
  return split(prefix_end).first;
}

TextSlice TextSlice::suffix(Point suffix_start) const {
  return split(suffix_start).second;
}

TextSlice TextSlice::slice(Range range) const {
  return suffix(range.start).prefix(range.extent());
}

Point TextSlice::extent() const {
  return end_position.traversal(start_position);
}

uint32_t TextSlice::size() const {
  return end_offset() - start_offset();
}

Text::const_iterator TextSlice::cbegin() const {
  return text->cbegin() + start_offset();
}

Text::const_iterator TextSlice::cend() const {
  return text->cbegin() + end_offset();
}
