#include <vector>
#include "patch.h"
#include "point.h"
#include "range.h"
#include "text.h"

class TextBuffer {
  struct TextChunkCallback {
    virtual void operator()(TextSlice chunk) = 0;
  };

  struct Layer {
    virtual uint32_t size() const = 0;
    virtual uint16_t character_at(Point) = 0;
    virtual ClipResult clip_position(Point position) = 0;
    virtual void add_chunks_in_range(TextChunkCallback *, Point start, Point end) = 0;
    virtual Point extent() const = 0;
  };

  struct BaseLayer : public Layer {
    Text text;
    BaseLayer(Text &&text);
    BaseLayer() = default;
    virtual uint32_t size() const;
    virtual uint16_t character_at(Point);
    virtual ClipResult clip_position(Point position);
    virtual void add_chunks_in_range(TextChunkCallback *callback, Point start, Point end);
    virtual Point extent() const;
  };

  struct DerivedLayer : public Layer {
    Layer *previous_layer;
    Patch patch;
    Point extent_;
    uint32_t size_;

    DerivedLayer(Layer *);
    virtual uint32_t size() const;
    virtual uint16_t character_at(Point);
    virtual ClipResult clip_position(Point position);
    virtual void add_chunks_in_range(TextChunkCallback *callback, Point start, Point end);
    virtual Point extent() const;

    void set_text_in_range(Range old_range, Text &&new_text);
  };

  BaseLayer base_layer;
  std::vector<DerivedLayer> derived_layers;
  TextBuffer(Text &&base_text);

public:
  TextBuffer();
  TextBuffer(std::u16string text);

  bool load(std::istream &stream, size_t input_size, const char *encoding_name,
            size_t chunk_size, std::function<void(size_t)> progress_callback);
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
};
