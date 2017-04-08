#include <string>
#include <vector>
#include "patch.h"
#include "point.h"
#include "range.h"
#include "text.h"

class TextBuffer {
  struct Layer;
  Text base_text;
  Layer *top_layer;

  TextBuffer(Text &&base_text);

public:
  TextBuffer();
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

  bool reset_base_text(Text &&);
  bool flush_outstanding_changes();
  bool serialize_outstanding_changes(Serializer &);
  bool deserialize_outstanding_changes(Deserializer &);
  size_t base_text_digest();

  enum {
    NO_RESULTS = -1,
    INVALID_PATTERN = -2
  };

  struct SearchResult {
    optional<Range> range;
    std::u16string error_message;
  };

  SearchResult search(const uint16_t *, uint32_t);
  SearchResult search(const std::u16string &);

  std::string get_dot_graph() const;

  class Snapshot {
    friend class TextBuffer;
    TextBuffer &buffer;
    Layer &layer;

    Snapshot(TextBuffer &, Layer &);

  public:
    ~Snapshot();
    uint32_t size() const;
    Point extent() const;
    uint32_t line_length_for_row(uint32_t) const;
    std::vector<TextSlice> chunks() const;
    std::vector<TextSlice> chunks_in_range(Range) const;
    Text text() const;
    Text text_in_range(Range) const;
    SearchResult search(const uint16_t *, uint32_t) const;
  };

  friend class Snapshot;
  const Snapshot *create_snapshot();
};
