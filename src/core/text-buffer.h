#include "patch.h"
#include "point.h"
#include "range.h"
#include "text.h"

class TextBuffer {
  Text base_text;
  Patch patch;
  Point extent_;
  uint32_t size_;
  TextBuffer(Text &&base_text);

public:
  class Iterator {
    friend class TextBuffer;

    TextBuffer &buffer;
    optional<Patch::Change> next_change;
    int64_t offset_from_next_change_start;
    uint32_t next_change_base_offset;
    uint32_t current_offset;
    Iterator(TextBuffer &buffer, bool end);

    void fetch_next_change();

  public:
    using value_type = uint16_t;
    using difference_type = uint32_t;
    using iterator_category = std::random_access_iterator_tag;
    using pointer = uint16_t *;
    using reference = uint16_t &;

    uint16_t operator*() const;
    Iterator &operator++();
    bool operator!=(const Iterator &other) const;
    difference_type operator-(const Iterator &other) const;
    Iterator traverse(Point point) const;
  };

  TextBuffer() = default;
  TextBuffer(std::u16string text);

  bool load(std::istream &stream, size_t input_size, const char *encoding_name,
            size_t cchange_size, std::function<void(size_t)> progress_callback);

  uint32_t size() const;
  Point extent() const;
  uint32_t line_length_for_row(uint32_t row);
  Point clip_position(Point);
  Range clip_range(Range);
  Text text();
  Text text_in_range(Range range);
  void set_text_in_range(Range old_range, Text &&new_text);
  Iterator begin();
  Iterator end();
};
