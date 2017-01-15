#ifndef TEXT_H_
#define TEXT_H_

#include <memory>
#include <vector>
#include <ostream>
#include "point.h"

using Text = std::vector<uint16_t>;

struct TextSlice {
  Text *text;
  size_t start_index;
  size_t end_index;

  static Text concat(TextSlice a, TextSlice b);
  static Text concat(TextSlice a, TextSlice b, TextSlice c);

  TextSlice(Text &text);
  TextSlice(Text *text, size_t start_index, size_t end_index);
  operator Text() const;

  size_t size();
  Text::iterator begin();
  Text::iterator end();

  std::pair<TextSlice, TextSlice> split(Point);
  TextSlice prefix(Point);
  TextSlice suffix(Point);
  size_t character_index_for_position(Point);
};

std::ostream &operator<<(std::ostream &stream, const Text *text);

#endif  // TEXT_H_
