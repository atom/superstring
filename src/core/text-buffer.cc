#include "text-slice.h"
#include "text-buffer.h"
#include <cassert>
#include <vector>
#include <sstream>

using std::move;
using std::string;
using std::vector;

struct TextChunkCallback {
  virtual void operator()(TextSlice chunk) = 0;
};

class BaseLayer {
  const Text &text;

public:
  BaseLayer(Text &text) : text{text} {}
  uint32_t size() const { return text.size(); }
  Point extent() const { return text.extent(); }
  uint16_t character_at(Point position) { return text.at(position); }
  ClipResult clip_position(Point position) { return text.clip_position(position); }
  void add_chunks_in_range(TextChunkCallback *callback, Point start, Point end) {
    (*callback)(TextSlice(text).slice({start, end}));
  }
};

struct TextBuffer::Layer {
  union {
    Text *base_text;
    Layer *previous_layer;
  };

  Patch patch;
  Point extent_;
  uint32_t size_;
  uint32_t snapshot_count;
  bool is_first;
  bool is_last;

  Layer(Text *base_text) :
    base_text{base_text},
    extent_{base_text->extent()},
    size_{base_text->size()},
    snapshot_count{0},
    is_first{true},
    is_last{true} {}

  Layer(Layer *previous_layer) :
    previous_layer{previous_layer},
    extent_{previous_layer->extent()},
    size_{previous_layer->size()},
    snapshot_count{0},
    is_first{false},
    is_last{true} {}

  static inline Point previous_column(Point position) {
    return Point(position.row, position.column - 1);
  }

  template <typename T>
  inline uint16_t character_at_(T &previous_layer, Point position) {
    auto change = patch.find_change_for_new_position(position);
    if (!change) return previous_layer.character_at(position);
    if (position < change->new_end) {
      return change->new_text->at(position.traversal(change->new_start));
    } else {
      return previous_layer.character_at(
        change->old_end.traverse(position.traversal(change->new_end))
      );
    }
  }

  template <typename T>
  inline ClipResult clip_position_(T &previous_layer, Point position) {
    auto preceding_change = is_last ?
      patch.change_for_new_position(position) :
      patch.find_change_for_new_position(position);
    if (!preceding_change) return previous_layer.clip_position(position);

    uint32_t preceding_change_base_offset =
      previous_layer.clip_position(preceding_change->old_start).offset;
    uint32_t preceding_change_current_offset =
      preceding_change_base_offset +
      preceding_change->preceding_new_text_size -
      preceding_change->preceding_old_text_size;

    if (position < preceding_change->new_end) {
      ClipResult position_within_preceding_change =
        preceding_change->new_text->clip_position(
          position.traversal(preceding_change->new_start)
        );

      if (position_within_preceding_change.offset == 0 && preceding_change->old_start.column > 0) {
        if (preceding_change->new_text->content.front() == '\n' &&
            previous_layer.character_at(previous_column(preceding_change->old_start)) == '\r') {
          return {
            previous_column(preceding_change->new_start),
            preceding_change_current_offset - 1
          };
        }
      }

      return {
        preceding_change->new_start.traverse(position_within_preceding_change.position),
        preceding_change_current_offset + position_within_preceding_change.offset
      };
    } else {
      ClipResult base_location = previous_layer.clip_position(
        preceding_change->old_end.traverse(position.traversal(preceding_change->new_end))
      );

      ClipResult distance_past_preceding_change = {
        base_location.position.traversal(preceding_change->old_end),
        base_location.offset - (preceding_change_base_offset + preceding_change->old_text_size)
      };

      if (distance_past_preceding_change.offset == 0 && base_location.offset < previous_layer.size()) {
        uint16_t previous_character = 0;
        if (preceding_change->new_text->size() > 0) {
          previous_character = preceding_change->new_text->content.back();
        } else if (preceding_change->old_start.column > 0) {
          previous_character = previous_layer.character_at(previous_column(preceding_change->old_start));
        }

        if (previous_character == '\r' && previous_layer.character_at(base_location.position) == '\n') {
          return {
            previous_column(preceding_change->new_end),
            preceding_change_current_offset + preceding_change->new_text->size() - 1
          };
        }
      }

      return {
        preceding_change->new_end.traverse(distance_past_preceding_change.position),
        preceding_change_current_offset + preceding_change->new_text->size() + distance_past_preceding_change.offset
      };
    }
  }

  template <typename T>
  inline void add_chunks_in_range_(T &previous_layer, TextChunkCallback *callback, Point start, Point end) {
    Point goal_position = clip_position(end).position;
    Point current_position = clip_position(start).position;
    Point base_position = current_position;
    auto change = is_last ?
      patch.change_for_new_position(current_position) :
      patch.find_change_for_new_position(current_position);

    while (current_position < goal_position) {
      if (change) {
        if (current_position < change->new_end) {
          TextSlice slice = TextSlice(*change->new_text)
            .prefix(Point::min(
              goal_position.traversal(change->new_start),
              change->new_end.traversal(change->new_start)
            ))
            .suffix(current_position.traversal(change->new_start));
          (*callback)(slice);
          base_position = change->old_end;
          current_position = change->new_end;
          if (current_position > goal_position) break;
        }

        base_position = change->old_end.traverse(current_position.traversal(change->new_end));
      }

      change = is_last ?
        patch.change_ending_after_new_position(current_position, true) :
        patch.find_change_ending_after_new_position(current_position);

      Point next_base_position, next_position;
      if (change) {
        next_position = Point::min(goal_position, change->new_start);
        next_base_position = Point::min(base_position.traverse(goal_position.traversal(current_position)), change->old_start);
      } else {
        next_position = goal_position;
        next_base_position = base_position.traverse(goal_position.traversal(current_position));
      }

      previous_layer.add_chunks_in_range(callback, base_position, next_base_position);
      base_position = next_base_position;
      current_position = next_position;
    }
  }

  uint16_t character_at(Point position) {
    if (is_first) {
      BaseLayer base_layer(*base_text);
      return character_at_(base_layer, position);
    } else {
      return character_at_(*previous_layer, position);
    }
  }

  ClipResult clip_position(Point position) {
    if (is_first) {
      BaseLayer base_layer(*base_text);
      return clip_position_(base_layer, position);
    } else {
      return clip_position_(*previous_layer, position);
    }
  }

  void add_chunks_in_range(TextChunkCallback *callback, Point start, Point end) {
    if (is_first) {
      BaseLayer base_layer(*base_text);
      return add_chunks_in_range_(base_layer, callback, start, end);
    } else {
      return add_chunks_in_range_(*previous_layer, callback, start, end);
    }
  }

  Point extent() const { return extent_; }

  uint32_t size() const { return size_; }

  void set_text_in_range(Range old_range, Text &&new_text) {
    auto start = clip_position(old_range.start);
    auto end = clip_position(old_range.end);
    Point new_range_end = start.position.traverse(new_text.extent());
    uint32_t deleted_text_size = end.offset - start.offset;
    extent_ = new_range_end.traverse(extent_.traversal(old_range.end));
    size_ += new_text.size() - deleted_text_size;
    patch.splice(
      old_range.start,
      old_range.extent(),
      new_text.extent(),
      optional<Text> {},
      move(new_text),
      deleted_text_size
    );
  }

  Text text_in_range(Range range) {
    struct Callback : public TextChunkCallback {
      Text text;
      void operator()(TextSlice chunk) { text.append(chunk); }
    };

    Callback callback;
    add_chunks_in_range(&callback, range.start, range.end);
    return callback.text;
  }

  vector<TextSlice> chunks_in_range(Range range) {
    struct Callback : public TextChunkCallback {
      vector<TextSlice> slices;
      void operator()(TextSlice chunk) { slices.push_back(chunk); }
    };

    Callback callback;
    add_chunks_in_range(&callback, range.start, range.end);
    return callback.slices;
  }
};

TextBuffer::TextBuffer(Text &&text) :
  base_text{std::move(text)},
  top_layer{new TextBuffer::Layer(&this->base_text)} {}

TextBuffer::TextBuffer() :
  top_layer{new TextBuffer::Layer(&this->base_text)} {}

TextBuffer::~TextBuffer() { delete top_layer; }

TextBuffer::TextBuffer(std::u16string text) : TextBuffer {Text {text}} {}

bool TextBuffer::reset(Text &&new_base_text) {
  if (!top_layer->is_first || top_layer->patch.get_change_count() > 0) {
    return false;
  }

  top_layer->extent_ = new_base_text.extent();
  top_layer->size_ = new_base_text.size();
  base_text = move(new_base_text);
  return true;
}

Point TextBuffer::extent() const {
  return top_layer->extent();
}

uint32_t TextBuffer::size() const {
  return top_layer->size();
}

uint32_t TextBuffer::line_length_for_row(uint32_t row) {
  return top_layer->clip_position(Point{row, UINT32_MAX}).position.column;
}

Point TextBuffer::clip_position(Point position) {
  return top_layer->clip_position(position).position;
}

Range TextBuffer::clip_range(Range range) {
  return Range {clip_position(range.start), clip_position(range.end)};
}

Text TextBuffer::text() {
  return text_in_range(Range{Point(), extent()});
}

Text TextBuffer::text_in_range(Range range) {
  return top_layer->text_in_range(range);
}

void TextBuffer::set_text(Text &&new_text) {
  set_text_in_range(Range {Point(0, 0), extent()}, move(new_text));
}

void TextBuffer::set_text_in_range(Range old_range, Text &&new_text) {
  top_layer->set_text_in_range(old_range, move(new_text));
}

string TextBuffer::get_dot_graph() const {
  Layer *layer = top_layer;
  vector<TextBuffer::Layer *> layers;
  for (;;) {
    layers.push_back(layer);
    if (layer->is_first) break;
    layer = layer->previous_layer;
  }

  std::stringstream result;
  result << "graph { label=\"--- buffer ---\" }\n";
  result << "graph { label=\"base:\n" << base_text << "\" }\n";
  for (auto begin = layers.rbegin(), iter = begin, end = layers.rend();
       iter != end; ++iter) {
    result << "graph { label=\"layer " << std::to_string(iter - begin) <<
      " (snapshot count " << std::to_string((*iter)->snapshot_count) <<
      "):\" }\n" << (*iter)->patch.get_dot_graph();
  }
  return result.str();
}

const TextBuffer::Snapshot *TextBuffer::create_snapshot() {
  Layer *layer;
  if (!top_layer->is_first && top_layer->patch.get_change_count() == 0) {
    layer = top_layer->previous_layer;
  } else {
    layer = top_layer;
    layer->is_last = false;
    top_layer = new Layer(top_layer);
  }
  layer->snapshot_count++;
  return new Snapshot(*this, *layer);
}

uint32_t TextBuffer::Snapshot::size() const {
  return layer.size();
}

Point TextBuffer::Snapshot::extent() const {
  return layer.extent();
}

uint32_t TextBuffer::Snapshot::line_length_for_row(uint32_t row) const {
  return layer.clip_position(Point{row, UINT32_MAX}).position.column;
}

Text TextBuffer::Snapshot::text_in_range(Range range) const {
  return layer.text_in_range(range);
}

Text TextBuffer::Snapshot::text() const {
  return layer.text_in_range({{0, 0}, extent()});
}

vector<TextSlice> TextBuffer::Snapshot::chunks_in_range(Range range) const {
  return layer.chunks_in_range(range);
}

vector<TextSlice> TextBuffer::Snapshot::chunks() const {
  return layer.chunks_in_range({{0, 0}, extent()});
}

TextBuffer::Snapshot::Snapshot(TextBuffer &buffer, TextBuffer::Layer &layer)
  : buffer{buffer}, layer{layer} {}

TextBuffer::Snapshot::~Snapshot() {
  assert(layer.snapshot_count > 0);
  layer.snapshot_count--;
  if (layer.snapshot_count > 0) return;

  // Find the topmost layer that has no snapshots pointing to it.
  vector<TextBuffer::Layer *> layers_to_remove;
  TextBuffer::Layer *top_layer = buffer.top_layer;
  if (top_layer->snapshot_count > 0) return;
  while (!top_layer->is_first && top_layer->previous_layer->snapshot_count == 0) {
    layers_to_remove.push_back(top_layer);
    top_layer = top_layer->previous_layer;
  }

  top_layer->size_ = buffer.top_layer->size_;
  top_layer->extent_ = buffer.top_layer->extent_;

  // Incorporate all the changes from upper layers into this layer.
  bool left_to_right = true;
  for (auto iter = layers_to_remove.rbegin(),
       end = layers_to_remove.rend();
       iter != end; ++iter) {
    top_layer->patch.combine((*iter)->patch, left_to_right);
    delete *iter;
    left_to_right = !left_to_right;
  }

  buffer.top_layer = top_layer;
  buffer.top_layer->is_last = true;
}
