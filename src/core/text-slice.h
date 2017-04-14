#ifndef FLAT_TEXT_SLICE_H_
#define FLAT_TEXT_SLICE_H_

#include <vector>
#include "point.h"
#include "range.h"
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
  TextSlice();
  TextSlice(const Text &text);
  std::pair<TextSlice, TextSlice> split(Point) const;
  std::pair<TextSlice, TextSlice> split(uint32_t) const;
  TextSlice prefix(Point) const;
  TextSlice suffix(Point) const;
  TextSlice slice(Range range) const;
  Point position_for_offset(uint32_t offset) const;
  Point extent() const;
  uint16_t front() const;
  uint16_t back() const;

  const uint16_t *data() const;
  uint32_t size() const;
  bool empty() const;

  Text::const_iterator begin() const;
  Text::const_iterator end() const;
};

#endif // FLAT_TEXT_SLICE_H_
