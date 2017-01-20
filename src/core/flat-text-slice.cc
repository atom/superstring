#include "flat-text-slice.h"
#include "flat-text.h"

FlatTextSlice::FlatTextSlice(const FlatText *text, Point start_position, Point end_position) :
  text{text}, start_position {start_position}, end_position {end_position} {}

size_t FlatTextSlice::start_offset() const {
  return text->line_offsets[start_position.row] + start_position.column;
}

size_t FlatTextSlice::end_offset() const {
  return text->line_offsets[end_position.row] + end_position.column;
}

FlatTextSlice::FlatTextSlice(const FlatText &text) :
  text{&text}, start_position{Point(0, 0)}, end_position{text.extent()} {}

std::pair<FlatTextSlice, FlatTextSlice> FlatTextSlice::split(Point split_point) const {
  Point absolute_split_point = start_position.traverse(split_point);
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

FlatText::const_iterator FlatTextSlice::cbegin() const {
  return text->cbegin() + start_offset();
}

FlatText::const_iterator FlatTextSlice::cend() const {
  return text->cbegin() + end_offset();
}
