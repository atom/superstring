#include <string>
#include <vector>
#include "patch.h"
#include "point.h"
#include "range.h"
#include "text.h"

class TextBuffer {
  struct TextChunkCallback {
    virtual void operator()(TextSlice chunk) = 0;
  };

  struct BaseLayer {
    Text text;

    BaseLayer(Text &&text);
    BaseLayer() = default;
    Point extent() const;
    uint32_t size() const;
    uint16_t character_at(Point);
    ClipResult clip_position(Point position);
    void add_chunks_in_range(TextChunkCallback *callback, Point start, Point end);
  };

  struct DerivedLayer {
    TextBuffer &buffer;
    Patch patch;
    uint32_t index;
    uint32_t size_;
    Point extent_;
    uint32_t reference_count;

    DerivedLayer(TextBuffer &, uint32_t);

    template <typename T>
    inline uint16_t character_at_(T &, Point);
    template <typename T>
    inline ClipResult clip_position_(T &, Point position);
    template <typename T>
    inline void add_chunks_in_range_(T &, TextChunkCallback *callback, Point start, Point end);

    Point extent() const;
    uint32_t size() const;
    uint16_t character_at(Point);
    ClipResult clip_position(Point position);
    void add_chunks_in_range(TextChunkCallback *callback, Point start, Point end);

    void set_text_in_range(Range old_range, Text &&new_text);
    Text text_in_range(Range);
  };

  BaseLayer base_layer;
  std::vector<DerivedLayer> derived_layers;
  TextBuffer(Text &&base_text);

public:
  TextBuffer();
  TextBuffer(std::u16string text);

  bool reset(Text &&);
  bool save(std::ostream &stream, const char *encoding_name, size_t chunk_size);

  uint32_t size() const;
  Point extent() const;
  uint32_t line_length_for_row(uint32_t row);
  Point clip_position(Point);
  Range clip_range(Range);
  Text text();
  Text text_in_range(Range range);
  void set_text(Text &&new_text);
  void set_text_in_range(Range old_range, Text &&new_text);

  std::string get_dot_graph() const;

  class Snapshot {
    friend class TextBuffer;
    TextBuffer &buffer;
    uint32_t index;

    Snapshot(TextBuffer &, uint32_t);

  public:
    ~Snapshot();
    uint32_t size() const;
    Point extent() const;
    uint32_t line_length_for_row(uint32_t) const;
    std::vector<TextSlice> chunks() const;
    std::vector<TextSlice> chunks_in_range(Range) const;
    Text text() const;
    Text text_in_range(Range) const;
  };

  friend class Snapshot;
  const Snapshot *create_snapshot();
};
