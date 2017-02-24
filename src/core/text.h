#ifndef TEXT_H_
#define TEXT_H_

#include <memory>
#include <vector>
#include <ostream>
#include "point.h"

struct Text : public std::vector<uint16_t> {
  using std::vector<uint16_t>::vector;
  Text(void) : vector() {}
  Text(std::vector<uint16_t> const & vec) : vector(vec) {}
  Text(std::vector<uint16_t> && vec) : vector(std::move(vec)) {}
};

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
