#ifndef FLAT_TEXT_H_
#define FLAT_TEXT_H_

#include <istream>
#include <functional>
#include <vector>
#include <ostream>
#include "serializer.h"
#include "point.h"
#include "optional.h"

class TextSlice;

struct ClipResult {
  Point position;
  uint32_t offset;
};

class Text {
  friend class TextSlice;

 public:
  using EncodingConversion = void *;
  static optional<EncodingConversion> transcoding_to(const char *);
  static optional<EncodingConversion> transcoding_from(const char *);

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

  bool encode(EncodingConversion, size_t start_offset, size_t end_offset,
              std::ostream &stream, size_t chunk_size) const;
  size_t encode(EncodingConversion, size_t *start_offset, size_t end_offset,
                char *buffer, size_t buffer_size, bool is_last = false) const;

  bool decode(EncodingConversion, std::istream &stream,
              size_t chunk_size, std::function<void(size_t)> progress_callback);
  size_t decode(EncodingConversion, const char *buffer, size_t buffer_size,
                bool is_last = false);

  static Text concat(TextSlice a, TextSlice b);
  static Text concat(TextSlice a, TextSlice b, TextSlice c);
  void splice(Point start, Point deletion_extent, TextSlice inserted_slice);

  uint16_t at(Point position) const;
  uint16_t at(uint32_t offset) const;
  const_iterator begin() const;
  const_iterator end() const;
  inline const_iterator cbegin() const { return begin(); }
  inline const_iterator cend() const { return end(); }
  ClipResult clip_position(Point) const;
  Point extent() const;
  bool empty() const;
  uint32_t offset_for_position(Point) const;
  Point position_for_offset(uint32_t, bool clip_crlf = true) const;
  uint32_t line_length_for_row(uint32_t row) const;
  void append(TextSlice);
  void assign(TextSlice);
  void serialize(Serializer &) const;
  uint32_t size() const;
  const uint16_t *data() const;
  size_t digest() const;
  void clear();
  void reserve(size_t capacity);

  bool operator!=(const Text &) const;
  bool operator==(const Text &) const;

  friend std::ostream &operator<<(std::ostream &, const Text &);
};

#endif // FLAT_TEXT_H_
