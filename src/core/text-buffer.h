#include <string>
#include <vector>
#include "patch.h"
#include "point.h"
#include "range.h"
#include "text.h"

class TextBuffer {
  struct Layer;
  Layer *base_layer;
  Layer *top_layer;
  void squash_layers(const std::vector<Layer *> &);
  void consolidate_layers();

public:
  static uint32_t MAX_CHUNK_SIZE_TO_COPY;

  TextBuffer();
  TextBuffer(Text &&base_text);
  TextBuffer(std::u16string text);
  ~TextBuffer();

  uint32_t size() const;
  Point extent() const;
  uint32_t line_length_for_row(uint32_t row);
  const uint16_t *line_ending_for_row(uint32_t row);
  ClipResult clip_position(Point);
  Point position_for_offset(uint32_t offset);
  Text text();
  Text text_in_range(Range range);
  void set_text(Text &&new_text);
  void set_text_in_range(Range old_range, Text &&new_text);
  bool is_modified() const;
  std::vector<TextSlice> chunks() const;

  void reset(Text &&);
  void flush_changes();
  void serialize_changes(Serializer &);
  bool deserialize_changes(Deserializer &);
  const Text &base_text() const;

  struct SearchResult {
    optional<Range> range;
    std::u16string error_message;
  };

  SearchResult search(const uint16_t *, uint32_t);
  SearchResult search(const std::u16string &);

  class Snapshot {
    friend class TextBuffer;
    TextBuffer &buffer;
    Layer &layer;
    Layer &base_layer;

    Snapshot(TextBuffer &, Layer &, Layer &);

  public:
    ~Snapshot();
    void flush_preceding_changes();

    uint32_t size() const;
    Point extent() const;
    uint32_t line_length_for_row(uint32_t) const;
    std::vector<TextSlice> chunks() const;
    std::vector<TextSlice> chunks_in_range(Range) const;
    Text text() const;
    Text text_in_range(Range) const;
    const Text &base_text() const;
    SearchResult search(const uint16_t *, uint32_t) const;
  };

  friend class Snapshot;
  Snapshot *create_snapshot();

  size_t layer_count()  const;
  std::string get_dot_graph() const;
};
