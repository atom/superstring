#ifndef FLAT_TEXT_H_
#define FLAT_TEXT_H_

#include <istream>
#include <iconv.h>
#include <functional>
#include <vector>
#include <ostream>

class FlatTextSlice;

struct Point;

class FlatText {
  friend class FlatTextSlice;

  std::vector<char16_t> content;
  std::vector<uint32_t> line_offsets;
  FlatText(const std::vector<char16_t> &, const std::vector<uint32_t> &);

 public:
  using const_iterator = std::vector<char16_t>::const_iterator;

  FlatText();
  FlatText(const std::u16string &string);
  FlatText(FlatTextSlice slice);

  static FlatText build(std::istream &stream, size_t input_size, const char *encoding_name,
                        size_t chunk_size, std::function<void(size_t)> progress_callback);
  static FlatText concat(FlatTextSlice a, FlatTextSlice b);
  static FlatText concat(FlatTextSlice a, FlatTextSlice b, FlatTextSlice c);

  std::pair<const_iterator, const_iterator> line_iterators(uint32_t row) const;
  const_iterator cbegin() const;
  const_iterator cend() const;
  Point extent() const;
  void append(FlatTextSlice);

  bool operator==(const FlatText &) const;

  friend std::ostream &operator<<(std::ostream &, const FlatText &);
};

#endif // FLAT_TEXT_H_
