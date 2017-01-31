#ifndef FLAT_TEXT_H_
#define FLAT_TEXT_H_

#include <istream>
#include <iconv.h>
#include <functional>
#include <vector>
#include <ostream>
#include "serializer.h"

class FlatTextSlice;

struct Point;

class FlatText {
  friend class FlatTextSlice;

  std::vector<uint16_t> content;
  std::vector<uint32_t> line_offsets;
  FlatText(const std::vector<uint16_t> &, const std::vector<uint32_t> &);

 public:
  using const_iterator = std::vector<uint16_t>::const_iterator;

  FlatText();
  FlatText(const std::u16string &string);
  FlatText(std::vector<uint16_t> &&content);
  FlatText(FlatTextSlice slice);
  FlatText(const FlatText &);
  FlatText(Serializer &serializer);

  static FlatText build(std::istream &stream, size_t input_size, const char *encoding_name,
                        size_t chunk_size, std::function<void(size_t)> progress_callback);
  static FlatText concat(FlatTextSlice a, FlatTextSlice b);
  static FlatText concat(FlatTextSlice a, FlatTextSlice b, FlatTextSlice c);

  std::pair<const_iterator, const_iterator> line_iterators(uint32_t row) const;
  const_iterator cbegin() const;
  const_iterator cend() const;
  Point extent() const;
  void append(FlatTextSlice);
  void serialize(Serializer &) const;
  size_t size() const;
  const uint16_t *data() const;

  bool operator==(const FlatText &) const;

  friend std::ostream &operator<<(std::ostream &, const FlatText &);
};

#endif // FLAT_TEXT_H_
