#include "flat-text-slice.h"
#include "flat-text.h"
#include <assert.h>

FlatTextSlice::FlatTextSlice(const FlatText *text, Point start_position, Point end_position) :
  text {text}, start_position {start_position}, end_position {end_position} {}

FlatTextSlice::FlatTextSlice(const FlatText &text) :
  text {&text}, start_position {Point()}, end_position {text.extent()} {}

size_t FlatTextSlice::start_offset() const {
  assert(start_position.row < text->line_offsets.size());
  return text->line_offsets[start_position.row] + start_position.column;
}

size_t FlatTextSlice::end_offset() const {
  return text->line_offsets[end_position.row] + end_position.column;
}

std::pair<FlatTextSlice, FlatTextSlice> FlatTextSlice::split(Point split_point) const {
  Point absolute_split_point = start_position.traverse(split_point);
  assert(absolute_split_point <= text->extent());
  return std::pair<FlatTextSlice, FlatTextSlice> {
    FlatTextSlice {text, start_position, absolute_split_point},
    FlatTextSlice {text, absolute_split_point, end_position}
  };
}

FlatTextSlice FlatTextSlice::prefix(Point prefix_end) const {
  return split(prefix_end).first;
}

FlatTextSlice FlatTextSlice::suffix(Point suffix_start) const {
  return split(suffix_start).second;
}

Point FlatTextSlice::extent() const {
  return end_position.traversal(start_position);
}

FlatText::const_iterator FlatTextSlice::cbegin() const {
  return text->cbegin() + start_offset();
}

FlatText::const_iterator FlatTextSlice::cend() const {
  return text->cbegin() + end_offset();
}
