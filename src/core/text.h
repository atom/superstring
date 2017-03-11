#ifndef FLAT_TEXT_H_
#define FLAT_TEXT_H_

#include <istream>
#include <iconv.h>
#include <functional>
#include <vector>
#include <ostream>
#include "serializer.h"

class TextSlice;
struct Point;

class Text {
  friend class TextSlice;

 public:
  std::vector<uint16_t> content;
  std::vector<uint32_t> line_offsets;
  Text(const std::vector<uint16_t> &&, const std::vector<uint32_t> &&);

  using const_iterator = std::vector<uint16_t>::const_iterator;

  Text();
  Text(const std::u16string &string);
  Text(std::vector<uint16_t> &&content);
  Text(TextSlice slice);
  Text(Deserializer &deserializer);
  template<typename Iter>
  Text(Iter begin, Iter end) : Text(std::vector<uint16_t>{begin, end}) {}

  static Text build(std::istream &stream, size_t input_size, const char *encoding_name,
                        size_t cchange_size, std::function<void(size_t)> progress_callback);
  static Text concat(TextSlice a, TextSlice b);
  static Text concat(TextSlice a, TextSlice b, TextSlice c);
  void splice(Point start, Point deletion_extent, TextSlice inserted_slice);

  uint16_t at(uint32_t offset) const;
  std::pair<const_iterator, const_iterator> line_iterators(uint32_t row) const;
  const_iterator begin() const;
  const_iterator end() const;
  inline const_iterator cbegin() const { return begin(); }
  inline const_iterator cend() const { return end(); }
  Point extent() const;
  uint32_t offset_for_position(Point) const;
  uint32_t line_length_for_row(uint32_t row) const;
  void append(TextSlice);
  void serialize(Serializer &) const;
  uint32_t size() const;
  const uint16_t *data() const;

  bool operator==(const Text &) const;

  friend std::ostream &operator<<(std::ostream &, const Text &);
};

#endif // FLAT_TEXT_H_
