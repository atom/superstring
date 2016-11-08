#ifndef TEXT_H_
#define TEXT_H_

#include "point.h"

struct TextSlice {
  uint16_t *content;
  uint32_t length;
};

struct Text {
  uint16_t *content;
  uint32_t length;

  static Text *Concat(TextSlice a, TextSlice b);
  static Text *Concat(TextSlice a, TextSlice b, TextSlice c);

  Text(uint32_t length);
  ~Text();
  TextSlice Prefix(Point prefix_end) const;
  TextSlice Suffix(Point suffix_start) const;
  TextSlice AsSlice() const;

private:
  uint32_t CharacterIndexForPosition (Point target) const;
};

#endif // TEXT_H_
