#ifndef FLAT_TEXT_SLICE_H_
#define FLAT_TEXT_SLICE_H_

#include <vector>

#include "point.h"
#include "flat-text.h"

class FlatTextSlice {
  friend class FlatText;

  const FlatText &text;
  const Point start_position;
  const Point end_position;

  FlatTextSlice(const FlatText &text, Point start_position, Point end_position);
  size_t start_offset() const;
  size_t end_offset() const;

 public:
  FlatTextSlice(const FlatText &text);
  std::pair<FlatTextSlice, FlatTextSlice> split(Point) const;
  FlatTextSlice prefix(Point) const;
  FlatTextSlice suffix(Point) const;

  FlatText::const_iterator cbegin() const;
  FlatText::const_iterator cend() const;
};

#endif // FLAT_TEXT_SLICE_H_
