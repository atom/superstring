#ifndef TEXT_H_
#define TEXT_H_

#include <memory>
#include "point.h"

struct TextSlice {
  uint16_t *content;
  uint32_t length;
};

struct Text {
  uint16_t *content;
  uint32_t length;

  static std::unique_ptr<Text> Concat(TextSlice a, TextSlice b);
  static std::unique_ptr<Text> Concat(TextSlice a, TextSlice b, TextSlice c);

  Text(uint32_t length);
  Text(TextSlice slice);
  ~Text();
  TextSlice Prefix(Point prefix_end) const;
  TextSlice Suffix(Point suffix_start) const;
  TextSlice AsSlice() const;
  void TrimLeft(Point position);
  void TrimRight(Point position);

private:
  uint32_t CharacterIndexForPosition(Point) const;
};

#endif // TEXT_H_
