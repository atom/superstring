#include "patch.h"
#include "point.h"
#include "range.h"
#include "text.h"

class TextBuffer {
  Text base_text;
  Patch patch;
  Point extent_;
  TextBuffer(Text &&base_text);

public:
  class iterator {
    friend class TextBuffer;

    TextBuffer &buffer;
    uint32_t offset;
    optional<Patch::Hunk> next_hunk;
    optional<uint32_t> next_hunk_offset;
    iterator(TextBuffer &buffer);

  public:
    using value_type = uint16_t;
    using difference_type = uint32_t;
    using iterator_category = std::random_access_iterator_tag;
    using pointer = uint16_t *;
    using reference = uint16_t &;

    uint16_t operator*() const;
    iterator &operator++();
    bool operator!=(const iterator &other) const;
    difference_type operator-(const iterator &other) const;
    iterator traverse(Point point) const;
  };

  static TextBuffer build(std::istream &stream, size_t input_size, const char *encoding_name,
                          size_t chunk_size, std::function<void(size_t)> progress_callback);

  TextBuffer(std::u16string text);

  Point extent() const;
  Text text();
  Text text_in_range(Range range);
  void set_text_in_range(Range old_range, Text &&new_text);
  iterator begin();
  iterator end();
};
