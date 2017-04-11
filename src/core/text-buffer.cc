#include "text-slice.h"
#include "text-buffer.h"
#include "regex.h"
#include <cassert>
#include <vector>
#include <sstream>

using std::move;
using std::string;
using std::vector;
using std::u16string;
using SearchResult = TextBuffer::SearchResult;

struct BaseLayer {
  const Text &text;

  BaseLayer(Text &text) : text{text} {}
  uint32_t size() const { return text.size(); }
  Point extent() const { return text.extent(); }
  uint16_t character_at(Point position) { return text.at(position); }
  ClipResult clip_position(Point position) { return text.clip_position(position); }

  template <typename Callback>
  bool for_each_chunk_in_range(Point start, Point end, const Callback &callback) {
    return callback(TextSlice(text).slice({start, end}));
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
  uint16_t character_at_(T &previous_layer, Point position) {
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
  ClipResult clip_position_(T &previous_layer, Point position) {
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

  template <typename T, typename Callback>
  bool for_each_chunk_in_range_(T &previous_layer, Point start, Point end, const Callback &callback) {
    Point goal_position = clip_position(end).position;
    Point current_position = clip_position(start).position;
    Point base_position = current_position;
    auto change = patch.find_change_for_new_position(current_position);

    while (current_position < goal_position) {
      if (change) {
        if (current_position < change->new_end) {
          TextSlice slice = TextSlice(*change->new_text)
            .prefix(Point::min(
              goal_position.traversal(change->new_start),
              change->new_end.traversal(change->new_start)
            ))
            .suffix(current_position.traversal(change->new_start));
          if (callback(slice)) return true;
          base_position = change->old_end;
          current_position = change->new_end;
          if (current_position > goal_position) break;
        }

        base_position = change->old_end.traverse(current_position.traversal(change->new_end));
      }

      change = patch.find_change_ending_after_new_position(current_position);

      Point next_base_position, next_position;
      if (change) {
        next_position = Point::min(goal_position, change->new_start);
        next_base_position = Point::min(base_position.traverse(goal_position.traversal(current_position)), change->old_start);
      } else {
        next_position = goal_position;
        next_base_position = base_position.traverse(goal_position.traversal(current_position));
      }

      if (previous_layer.for_each_chunk_in_range(base_position, next_base_position, callback)) {
        return true;
      }
      base_position = next_base_position;
      current_position = next_position;
    }

    return false;
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

  Point position_for_offset(uint32_t goal_offset) {
    Point position;
    uint32_t offset = 0;

    for_each_chunk_in_range(Point(0, 0), extent(), [&position, &offset, goal_offset](TextSlice slice) {
      uint32_t size = slice.size();
      if (offset + size >= goal_offset) {
        position = position.traverse(slice.position_for_offset(goal_offset - offset));
        return true;
      }
      position = position.traverse(slice.extent());
      offset += size;
      return false;
    });

    return position;
  }

  template <typename Callback>
  bool for_each_chunk_in_range(Point start, Point end, const Callback &callback) {
    if (is_first) {
      BaseLayer base_layer(*base_text);
      return for_each_chunk_in_range_(base_layer, start, end, callback);
    } else {
      return for_each_chunk_in_range_(*previous_layer, start, end, callback);
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
    Text result;
    for_each_chunk_in_range(range.start, range.end, [&result](TextSlice slice) {
      result.append(slice);
      return false;
    });
    return result;
  }

  vector<TextSlice> chunks_in_range(Range range) {
    vector<TextSlice> result;
    for_each_chunk_in_range(range.start, range.end, [&result](TextSlice slice) {
      result.push_back(slice);
      return false;
    });
    return result;
  }

  SearchResult search(const uint16_t *pattern, uint32_t pattern_length) {
    Regex regex(pattern, pattern_length);

    if (!regex.error_message.empty()) {
      return {optional<Range>{}, regex.error_message};
    }

    bool found_match = false;
    size_t match_start_offset;
    size_t match_end_offset;
    size_t current_offset = 0;
    vector<uint16_t> chunk_continuation;

    static const size_t MAX_CHUNK_SIZE_TO_COPY = 1024;

    this->for_each_chunk_in_range(Point(), extent(), [&](TextSlice chunk) {
      size_t chunk_offset = 0;

      for (;;) {
        auto chunk_data = chunk.data() + chunk_offset;
        size_t chunk_size = chunk.size() - chunk_offset;
        if (chunk_size == 0) break;

        if (!chunk_continuation.empty()) {
          if (chunk_size > MAX_CHUNK_SIZE_TO_COPY) {
            chunk_continuation.insert(chunk_continuation.end(), chunk_data, chunk_data + MAX_CHUNK_SIZE_TO_COPY);
            chunk_offset += MAX_CHUNK_SIZE_TO_COPY;
          } else {
            chunk_continuation.insert(chunk_continuation.end(), chunk_data, chunk_data + chunk_size);
            chunk_offset += chunk_size;
          }
          chunk_data = chunk_continuation.data();
          chunk_size = chunk_continuation.size();
        } else {
          chunk_offset = chunk_size;
        }

        int status = regex.match(chunk_data, chunk_size);

        if (status < 0) {
          switch (status) {
            case PCRE2_ERROR_NOMATCH:
              current_offset += chunk_size;
              chunk_continuation.clear();
              chunk_offset = chunk.size();
              break;

            case PCRE2_ERROR_PARTIAL: {
              size_t partial_match_start = regex.get_match_offset(0);
              current_offset += partial_match_start;
              chunk_continuation.assign(chunk_data + partial_match_start, chunk_data + chunk_size);
              break;
            }

            default:
              return true;
          }
        } else {
          match_start_offset = current_offset + regex.get_match_offset(0);
          match_end_offset = current_offset + regex.get_match_offset(1);
          found_match = true;
          return true;
        }
      }

      return false;
    });

    if (found_match) {
      return {
        Range{position_for_offset(match_start_offset), position_for_offset(match_end_offset)},
        u""
      };
    } else {
      return {
        optional<Range>{},
        u""
      };
    }
  }
};

TextBuffer::TextBuffer(Text &&text) :
  base_text{move(text)},
  top_layer{new TextBuffer::Layer(&this->base_text)} {}

TextBuffer::TextBuffer() :
  top_layer{new TextBuffer::Layer(&this->base_text)} {}

TextBuffer::~TextBuffer() { delete top_layer; }

TextBuffer::TextBuffer(std::u16string text) : TextBuffer {Text {text}} {}

bool TextBuffer::reset_base_text(Text &&new_base_text) {
  if (!top_layer->is_first) {
    return false;
  }

  top_layer->patch.clear();
  top_layer->extent_ = new_base_text.extent();
  top_layer->size_ = new_base_text.size();
  base_text = move(new_base_text);
  return true;
}

bool TextBuffer::flush_outstanding_changes() {
  if (!top_layer->is_first) return false;

  for (auto change : top_layer->patch.get_changes()) {
    base_text.splice(
      change.new_start,
      change.old_end.traversal(change.old_start),
      *change.new_text
    );
  }

  top_layer->patch.clear();
  return true;
}

bool TextBuffer::serialize_outstanding_changes(Serializer &serializer) {
  if (!top_layer->is_first) return false;
  top_layer->patch.serialize(serializer);
  serializer.append(top_layer->size_);
  top_layer->extent_.serialize(serializer);
  return true;
}

bool TextBuffer::deserialize_outstanding_changes(Deserializer &deserializer) {
  if (!top_layer->is_first || top_layer->patch.get_change_count() > 0) return false;
  top_layer->patch = Patch(deserializer);
  top_layer->size_ = deserializer.read<uint32_t>();
  top_layer->extent_ = Point(deserializer);
  return true;
}

template <typename T>
inline void hash_combine(std::size_t &seed, const T &value) {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

size_t TextBuffer::base_text_digest() {
  size_t result = 0;
  for (uint16_t character : base_text) {
    hash_combine(result, character);
  }
  return result;
}

const Text &TextBuffer::get_base_text() const {
  return base_text;
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

const uint16_t *TextBuffer::line_ending_for_row(uint32_t row) {
  static uint16_t LF[] = {'\n', 0};
  static uint16_t CRLF[] = {'\r', '\n', 0};
  static uint16_t NONE[] = {0};

  const uint16_t *result = NONE;
  top_layer->for_each_chunk_in_range(
    Point(row, UINT32_MAX),
    Point(row + 1, 0),
    [&result](TextSlice slice) {
      auto begin = slice.begin();
      if (begin == slice.end()) return false;
      result = (*begin == '\r') ? CRLF : LF;
      return true;
    });
  return result;
}

ClipResult TextBuffer::clip_position(Point position) {
  return top_layer->clip_position(position);
}

Point TextBuffer::position_for_offset(uint32_t offset) {
  return top_layer->position_for_offset(offset);
}

Text TextBuffer::text() {
  return text_in_range(Range{Point(), extent()});
}

Text TextBuffer::text_in_range(Range range) {
  return top_layer->text_in_range(range);
}

vector<TextSlice> TextBuffer::chunks() const {
  return top_layer->chunks_in_range({{0, 0}, extent()});
}

void TextBuffer::set_text(Text &&new_text) {
  set_text_in_range(Range {Point(0, 0), extent()}, move(new_text));
}

void TextBuffer::set_text_in_range(Range old_range, Text &&new_text) {
  top_layer->set_text_in_range(old_range, move(new_text));
}

SearchResult TextBuffer::search(const std::u16string &pattern) {
  return search(reinterpret_cast<const uint16_t *>(pattern.c_str()), pattern.size());
}

SearchResult TextBuffer::search(const uint16_t *pattern, uint32_t pattern_length) {
  return top_layer->search(pattern, pattern_length);
}

bool TextBuffer::is_modified() const {
  Layer *layer = top_layer;
  for (;;) {
    if (layer->patch.get_change_count() > 0) return true;
    if (layer->is_first) break;
    layer = layer->previous_layer;
  }
  return false;
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

SearchResult TextBuffer::Snapshot::search(const uint16_t *pattern, uint32_t pattern_length) const {
  return layer.search(pattern, pattern_length);
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
