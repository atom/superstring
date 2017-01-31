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

  std::vector<uint16_t> content;
  std::vector<uint32_t> line_offsets;
  Text(const std::vector<uint16_t> &, const std::vector<uint32_t> &);

 public:
  using const_iterator = std::vector<uint16_t>::const_iterator;

  Text();
  Text(const std::u16string &string);
  Text(std::vector<uint16_t> &&content);
  Text(TextSlice slice);
  Text(Serializer &serializer);

  static Text build(std::istream &stream, size_t input_size, const char *encoding_name,
                        size_t chunk_size, std::function<void(size_t)> progress_callback);
  static Text concat(TextSlice a, TextSlice b);
  static Text concat(TextSlice a, TextSlice b, TextSlice c);

  std::pair<const_iterator, const_iterator> line_iterators(uint32_t row) const;
  const_iterator cbegin() const;
  const_iterator cend() const;
  Point extent() const;
  void append(TextSlice);
  void serialize(Serializer &) const;
  size_t size() const;
  const uint16_t *data() const;

  bool operator==(const Text &) const;

  friend std::ostream &operator<<(std::ostream &, const Text &);
};

#endif // FLAT_TEXT_H_
