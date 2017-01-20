#ifndef FLAT_TEXT_SLICE_H_
#define FLAT_TEXT_SLICE_H_

#include "point.h"

class FlatText;

class FlatTextSlice {
  friend class FlatText;

  const FlatText &text;
  const Point start;
  const Point end;

  FlatTextSlice(const FlatText &text, Point start, Point end);
  size_t start_offset() const;
  size_t end_offset() const;

 public:
  std::pair<FlatTextSlice, FlatTextSlice> split(Point) const;
  FlatTextSlice prefix(Point) const;
  FlatTextSlice suffix(Point) const;
};

#endif // FLAT_TEXT_SLICE_H_
