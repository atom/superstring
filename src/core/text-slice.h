#ifndef FLAT_TEXT_SLICE_H_
#define FLAT_TEXT_SLICE_H_

#include <vector>

#include "point.h"
#include "text.h"

class TextSlice {
  friend class Text;

  const Text *text;
  Point start_position;
  Point end_position;

  TextSlice(const Text *text, Point start_position, Point end_position);
  size_t start_offset() const;
  size_t end_offset() const;

 public:
  TextSlice(const Text &text);
  std::pair<TextSlice, TextSlice> split(Point) const;
  TextSlice prefix(Point) const;
  TextSlice suffix(Point) const;

  Point extent() const;
  uint32_t size() const;

  Text::const_iterator cbegin() const;
  Text::const_iterator cend() const;
};

#endif // FLAT_TEXT_SLICE_H_