#include "flat-text-slice.h"
#include "flat-text.h"

FlatTextSlice::FlatTextSlice(const FlatText &text, Point start, Point end) :
  text{text}, start {start}, end {end} {}

size_t FlatTextSlice::start_offset() const {
  return text.line_offsets[start.row] + start.column;
}

size_t FlatTextSlice::end_offset() const {
  return text.line_offsets[end.row] + end.column;
}

FlatTextSlice::FlatTextSlice(const FlatText &text) :
  text{text}, start{Point(0, 0)}, end{text.extent()} {}

std::pair<FlatTextSlice, FlatTextSlice> FlatTextSlice::split(Point split_point) const {
  Point absolute_split_point = start.traverse(split_point);
  return std::pair<FlatTextSlice, FlatTextSlice> {
    FlatTextSlice {text, start, absolute_split_point},
    FlatTextSlice {text, absolute_split_point, end}
  };
}

FlatTextSlice FlatTextSlice::prefix(Point prefix_end) const {
  return split(prefix_end).first;
}

FlatTextSlice FlatTextSlice::suffix(Point suffix_start) const {
  return split(suffix_start).second;
}
