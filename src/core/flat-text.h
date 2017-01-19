#pragma once

#include <istream>
#include <iconv.h>
#include <functional>
#include <vector>
#include <ostream>

class FlatText {
  std::vector<char16_t> content;
  std::vector<uint32_t> line_offsets;
  FlatText(const std::vector<char16_t> &, const std::vector<uint32_t> &);

 public:
  using iterator = std::vector<char16_t>::iterator;

  FlatText(const std::u16string &string);

  static FlatText build(std::istream &stream, size_t input_size, const char *encoding_name,
                        size_t chunk_size, std::function<void(size_t)> progress_callback);

  std::pair<iterator, iterator> line_iterators(uint32_t row);

  bool operator==(const FlatText &) const;

  friend std::ostream &operator<<(std::ostream &, const FlatText &);
};
