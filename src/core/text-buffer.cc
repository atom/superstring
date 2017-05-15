#include "text-slice.h"
#include "text-buffer.h"
#include "regex.h"
#include <cassert>
#include <vector>
#include <sstream>

using std::equal;
using std::move;
using std::string;
using std::vector;
using std::u16string;
using SearchResult = TextBuffer::SearchResult;
using MatchResult = Regex::MatchResult;

uint32_t TextBuffer::MAX_CHUNK_SIZE_TO_COPY = 1024;

struct TextBuffer::Layer {
  Layer *previous_layer;
  Patch patch;
  optional<Text> text;
  bool is_derived;

  Point extent_;
  uint32_t size_;
  uint32_t snapshot_count;

  Layer(Text &&text) :
    previous_layer{nullptr},
    text{move(text)},
    is_derived{false},
    extent_{this->text->extent()},
    size_{this->text->size()},
    snapshot_count{0} {}

  Layer(Layer *previous_layer) :
    previous_layer{previous_layer},
    patch{Patch()},
    is_derived{true},
    extent_{previous_layer->extent()},
    size_{previous_layer->size()},
    snapshot_count{0} {}

  static inline Point previous_column(Point position) {
    return Point(position.row, position.column - 1);
  }

  bool is_above_layer(const Layer *layer) {
    Layer *predecessor = previous_layer;
    while (predecessor) {
      if (predecessor == layer) return true;
      predecessor = predecessor->previous_layer;
    }
    return false;
  }

  uint16_t character_at(Point position) {
    if (!is_derived) return text->at(position);

    auto change = patch.find_change_for_new_position(position);
    if (!change) return previous_layer->character_at(position);
    if (position < change->new_end) {
      return change->new_text->at(position.traversal(change->new_start));
    } else {
      return previous_layer->character_at(
        change->old_end.traverse(position.traversal(change->new_end))
      );
    }
  }

  ClipResult clip_position(Point position, bool splay = false) {
    if (!is_derived) return text->clip_position(position);

    auto preceding_change = splay ?
      patch.change_for_new_position(position) :
      patch.find_change_for_new_position(position);
    if (!preceding_change) return previous_layer->clip_position(position);

    uint32_t preceding_change_base_offset =
      previous_layer->clip_position(preceding_change->old_start).offset;
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
            previous_layer->character_at(previous_column(preceding_change->old_start)) == '\r') {
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
      ClipResult base_location = previous_layer->clip_position(
        preceding_change->old_end.traverse(position.traversal(preceding_change->new_end))
      );

      ClipResult distance_past_preceding_change = {
        base_location.position.traversal(preceding_change->old_end),
        base_location.offset - (preceding_change_base_offset + preceding_change->old_text_size)
      };

      if (distance_past_preceding_change.offset == 0 && base_location.offset < previous_layer->size()) {
        uint16_t previous_character = 0;
        if (preceding_change->new_text->size() > 0) {
          previous_character = preceding_change->new_text->content.back();
        } else if (preceding_change->old_start.column > 0) {
          previous_character = previous_layer->character_at(previous_column(preceding_change->old_start));
        }

        if (previous_character == '\r' && previous_layer->character_at(base_location.position) == '\n') {
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

  template <typename Callback>
  bool for_each_chunk_in_range(Point start, Point end, const Callback &callback, bool splay = false) {
    Point goal_position = clip_position(end, splay).position;
    Point current_position = clip_position(start, splay).position;

    if (!is_derived) return callback(TextSlice(*text).slice({current_position, goal_position}));

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

      if (previous_layer->for_each_chunk_in_range(base_position, next_base_position, callback)) {
        return true;
      }
      base_position = next_base_position;
      current_position = next_position;
    }

    return false;
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

  Point extent() const { return extent_; }

  uint32_t size() const { return size_; }

  Text text_in_range(Range range, bool splay = false) {
    Text result;
    for_each_chunk_in_range(range.start, range.end, [&result](TextSlice slice) {
      result.append(slice);
      return false;
    }, splay);
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
    if (pattern_length == 0) return {Range{Point(), Point()}, u""};

    Regex regex(pattern, pattern_length);

    if (!regex.error_message.empty()) {
      return {optional<Range>{}, regex.error_message};
    }

    optional<Range> result;
    Text chunk_continuation;
    TextSlice slice_to_search;
    Point slice_to_search_start_position;
    uint32_t slice_to_search_start_offset = 0;

    for_each_chunk_in_range(Point(), extent(), [&](TextSlice chunk) {
      while (!chunk.empty()) {

        // Once a result is found, we only continue if the match ends with a CR
        // at a chunk boundary. If this chunk starts with an LF, we decrement
        // the column because Points within CRLF line endings are not valid.
        if (result) {
          if (!chunk.empty() && chunk.front() == '\n') result->end.column--;
          return true;
        }

        if (!chunk_continuation.empty()) {
          auto split = chunk.split(MAX_CHUNK_SIZE_TO_COPY);
          chunk_continuation.append(split.first);
          chunk = split.second;
          slice_to_search = TextSlice(chunk_continuation);
        } else {
          slice_to_search = chunk;
          chunk = TextSlice();
        }

        MatchResult match_result = regex.match(slice_to_search.data(), slice_to_search.size());
        switch (match_result.type) {
          case MatchResult::Error:
            chunk_continuation.clear();
            return true;

          case MatchResult::None:
            slice_to_search_start_offset += slice_to_search.size();
            slice_to_search_start_position = slice_to_search_start_position.traverse(
              slice_to_search.extent()
            );
            chunk_continuation.clear();
            break;

          case MatchResult::Partial:
            if (chunk_continuation.empty() || match_result.start_offset > 0) {
              Point partial_match_position = slice_to_search.position_for_offset(match_result.start_offset);
              slice_to_search_start_offset += match_result.start_offset;
              slice_to_search_start_position = slice_to_search_start_position.traverse(partial_match_position);
              chunk_continuation.assign(slice_to_search.suffix(partial_match_position));
            }
            break;

          case MatchResult::Full:
            result = Range{
              slice_to_search_start_position.traverse(
                slice_to_search.position_for_offset(match_result.start_offset)
              ),
              slice_to_search_start_position.traverse(
                slice_to_search.position_for_offset(match_result.end_offset)
              )
            };

            // If the match ends with a CR at the end of a chunk, continue looking
            // at the next chunk, in case that chunk starts with an LF. Points
            // within CRLF line endings are not valid.
            if (match_result.end_offset == slice_to_search.size() && slice_to_search.back() == '\r') continue;

            return true;
        }
      }

      return false;
    });

    if (!result && !chunk_continuation.empty()) {
      result = Range{slice_to_search_start_position, extent()};
    }

    return {result, u""};
  }
};

TextBuffer::TextBuffer(Text &&text) :
  base_layer{new Layer(move(text))},
  top_layer{base_layer} {}

TextBuffer::TextBuffer() :
  base_layer{new Layer(Text{})},
  top_layer{base_layer} {}

TextBuffer::~TextBuffer() {
  Layer *layer = top_layer;
  while (layer) {
    Layer *previous_layer = layer->previous_layer;
    delete layer;
    layer = previous_layer;
  }
}

TextBuffer::TextBuffer(std::u16string text) : TextBuffer{Text{text}} {}

void TextBuffer::reset(Text &&new_base_text) {
  top_layer = new Layer(top_layer);
  top_layer->extent_ = new_base_text.extent();
  top_layer->size_ = new_base_text.size();
  top_layer->text = move(new_base_text);
  base_layer = top_layer;
  consolidate_layers();
}

Patch TextBuffer::get_inverted_changes(const Snapshot *snapshot) const {
  vector<const Patch *> patches;
  Layer *layer = top_layer;
  while (layer != &snapshot->base_layer) {
    patches.insert(patches.begin(), &layer->patch);
    layer = layer->previous_layer;
  }
  Patch combination(patches);
  TextSlice base{snapshot->base_text()};
  Patch result;
  for (auto change : combination.get_changes()) {
    result.splice(
      change.old_start,
      change.new_end.traversal(change.new_start),
      change.old_end.traversal(change.old_start),
      *change.new_text,
      Text(base.slice({change.old_start, change.old_end})),
      change.new_text->size()
    );
  }
  return result;
}

void TextBuffer::serialize_changes(Serializer &serializer) {
  serializer.append(top_layer->size_);
  top_layer->extent_.serialize(serializer);
  if (top_layer == base_layer) {
    Patch().serialize(serializer);
    return;
  }

  if (top_layer->previous_layer == base_layer) {
    top_layer->patch.serialize(serializer);
    return;
  }

  vector<const Patch *> patches;
  Layer *layer = top_layer;
  while (layer != base_layer) {
    patches.insert(patches.begin(), &layer->patch);
    layer = layer->previous_layer;
  }
  Patch(patches).serialize(serializer);
}

bool TextBuffer::deserialize_changes(Deserializer &deserializer) {
  if (top_layer != base_layer || base_layer->previous_layer) return false;
  top_layer = new Layer(base_layer);
  top_layer->size_ = deserializer.read<uint32_t>();
  top_layer->extent_ = Point(deserializer);
  top_layer->patch = Patch(deserializer);
  return true;
}

const Text &TextBuffer::base_text() const {
  return *base_layer->text;
}

Point TextBuffer::extent() const {
  return top_layer->extent();
}

uint32_t TextBuffer::size() const {
  return top_layer->size();
}

optional<uint32_t> TextBuffer::line_length_for_row(uint32_t row) {
  if (row > extent().row) return optional<uint32_t>{};
  return top_layer->clip_position(Point{row, UINT32_MAX}, true).position.column;
}

const uint16_t *TextBuffer::line_ending_for_row(uint32_t row) {
  if (row > extent().row) return nullptr;

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
    }, true);
  return result;
}

optional<Text> TextBuffer::line_for_row(uint32_t row) {
  if (row > extent().row) return optional<Text>{};
  return text_in_range({{row, 0}, {row, UINT32_MAX}});
}

ClipResult TextBuffer::clip_position(Point position) {
  return top_layer->clip_position(position, true);
}

Point TextBuffer::position_for_offset(uint32_t offset) {
  return top_layer->position_for_offset(offset);
}

Text TextBuffer::text() {
  return top_layer->text_in_range(Range{Point(), extent()});
}

Text TextBuffer::text_in_range(Range range) {
  return top_layer->text_in_range(range, true);
}

vector<TextSlice> TextBuffer::chunks() const {
  return top_layer->chunks_in_range({{0, 0}, extent()});
}

void TextBuffer::set_text(Text &&new_text) {
  set_text_in_range(Range{Point(0, 0), extent()}, move(new_text));
}

class ChunkIterator {
  const vector<TextSlice> *slices;
  uint32_t slice_index;
  Text::const_iterator slice_iterator;
  Text::const_iterator slice_end;
  uint32_t offset;

 public:
  ChunkIterator(vector<TextSlice> &slices) :
    slices{&slices},
    slice_index{0},
    slice_iterator{slices.front().begin()},
    slice_end{slices.front().end()},
    offset{0} {}

  ChunkIterator &operator++() {
    ++offset;
    ++slice_iterator;
    while (slice_iterator == slice_end) {
      slice_index++;
      if (slice_index == slices->size()) {
        slices = nullptr;
        slice_index = 0;
        slice_iterator = Text::const_iterator();
        slice_end = Text::const_iterator();
        break;
      } else {
        slice_iterator = slices->at(slice_index).begin();
        slice_end = slices->at(slice_index).end();
      }
    }
    return *this;
  }

  uint16_t operator*() const {
    return *slice_iterator;
  }

  bool operator!=(const ChunkIterator &other) {
    return slice_index != other.slice_index || slice_iterator != other.slice_iterator;
  }
};

namespace std {

template <>
struct iterator_traits<ChunkIterator> {
  using value_type = uint16_t;
  using pointer = uint16_t *;
  using reference = uint16_t &;
  using const_pointer = const uint16_t *;
  using const_reference = const uint16_t &;
  using difference_type = int64_t;
  using iterator_category = std::forward_iterator_tag;
};

}  // namespace std

void TextBuffer::set_text_in_range(Range old_range, Text &&new_text) {
  if (top_layer == base_layer || top_layer->snapshot_count > 0) {
    top_layer = new Layer(top_layer);
  }

  auto start = clip_position(old_range.start);
  auto end = clip_position(old_range.end);
  Point deleted_extent = end.position.traversal(start.position);
  Point inserted_extent = new_text.extent();
  Point new_range_end = start.position.traverse(new_text.extent());
  uint32_t deleted_text_size = end.offset - start.offset;
  top_layer->extent_ = new_range_end.traverse(top_layer->extent_.traversal(end.position));
  top_layer->size_ += new_text.size() - deleted_text_size;
  top_layer->patch.splice(
    start.position,
    deleted_extent,
    inserted_extent,
    optional<Text>{},
    move(new_text),
    deleted_text_size
  );

  auto change = top_layer->patch.change_for_old_position(start.position);
  if (change && change->old_text_size == change->new_text->size()) {
    auto chunks = top_layer->previous_layer->chunks_in_range({
      change->old_start,
      change->old_end
    });

    ChunkIterator existing_text_iter{chunks};
    if (equal(change->new_text->begin(), change->new_text->end(), existing_text_iter)) {
      top_layer->patch.splice_old(start.position, Point(), Point());
    }
  }
}

SearchResult TextBuffer::search(const std::u16string &pattern) {
  return search(reinterpret_cast<const uint16_t *>(pattern.c_str()), pattern.size());
}

SearchResult TextBuffer::search(const uint16_t *pattern, uint32_t pattern_length) {
  return top_layer->search(pattern, pattern_length);
}

bool TextBuffer::is_modified() const {
  if (size() != base_layer->size()) return true;

  bool result = false;
  uint32_t start_offset = 0;
  top_layer->for_each_chunk_in_range(Point(), extent(), [&](TextSlice chunk) {
    if (chunk.text == &(*base_layer->text) ||
        equal(chunk.begin(), chunk.end(), base_layer->text->begin() + start_offset)) {
      start_offset += chunk.size();
      return false;
    }
    result = true;
    return true;
  });

  return result;
}

string TextBuffer::get_dot_graph() const {
  Layer *layer = top_layer;
  vector<Layer *> layers;
  while (layer) {
    layers.push_back(layer);
    layer = layer->previous_layer;
  }

  std::stringstream result;
  result << "graph { label=\"--- buffer ---\" }\n";
  for (auto begin = layers.rbegin(), iter = begin, end = layers.rend();
       iter != end; ++iter) {
    auto layer = *iter;
    auto index = iter - begin;
    result << "graph { label=\"layer " << index << " (snapshot count " << layer->snapshot_count;
    if (layer == base_layer) result << ", base";
    if (layer->is_derived) result << ", derived";
    result << "):\" }\n";
    if (layer->text) result << "graph { label=\"text:\n" << *layer->text << "\" }\n";
    if (index > 0) result << layer->patch.get_dot_graph();
  }
  return result.str();
}

size_t TextBuffer::layer_count() const {
  size_t result = 1;
  const Layer *layer = top_layer;
  while (layer->previous_layer) {
    result++;
    layer = layer->previous_layer;
  }
  return result;
}

TextBuffer::Snapshot *TextBuffer::create_snapshot() {
  top_layer->snapshot_count++;
  base_layer->snapshot_count++;
  return new Snapshot(*this, *top_layer, *base_layer);
}

void TextBuffer::flush_changes() {
  if (!top_layer->text) {
    top_layer->text = text();
    base_layer = top_layer;
    consolidate_layers();
  }
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

const Text &TextBuffer::Snapshot::base_text() const {
  return *base_layer.text;
}

TextBuffer::Snapshot::Snapshot(TextBuffer &buffer, TextBuffer::Layer &layer,
                               TextBuffer::Layer &base_layer)
  : buffer{buffer}, layer{layer}, base_layer{base_layer} {}

void TextBuffer::Snapshot::flush_preceding_changes() {
  if (!layer.text) {
    layer.text = text();
    if (layer.is_above_layer(buffer.base_layer)) buffer.base_layer = &layer;
    buffer.consolidate_layers();
  }
}

TextBuffer::Snapshot::~Snapshot() {
  assert(layer.snapshot_count > 0);
  layer.snapshot_count--;
  base_layer.snapshot_count--;
  if (layer.snapshot_count == 0 || base_layer.snapshot_count == 0) {
    buffer.consolidate_layers();
  }
}

void TextBuffer::consolidate_layers() {
  Layer *layer = top_layer;
  vector<Layer *> mutable_layers;
  bool needed_by_layer_above = false;

  while (layer) {
    if (needed_by_layer_above || layer->snapshot_count > 0) {
      squash_layers(mutable_layers);
      mutable_layers.clear();
      needed_by_layer_above = true;
    } else {
      if (layer == base_layer) {
        squash_layers(mutable_layers);
        mutable_layers.clear();
      }

      if (layer->text) layer->is_derived = false;
      mutable_layers.push_back(layer);
    }

    if (!layer->is_derived) needed_by_layer_above = false;
    layer = layer->previous_layer;
  }

  squash_layers(mutable_layers);
}

void TextBuffer::squash_layers(const vector<Layer *> &layers) {
  if (layers.size() < 2) return;

  bool left_to_right = true;
  for (auto iter = layers.rbegin() + 1, end = layers.rend(); iter != end; ++iter) {
    Layer *layer = *iter;
    if (!layer->text) {
      if (layer->previous_layer->text) {
        for (auto change : layer->patch.get_changes()) {
          layer->previous_layer->text->splice(
            change.new_start,
            change.old_end.traversal(change.old_start),
            *change.new_text
          );
        }
        layer->text = move(layer->previous_layer->text);
        layer->is_derived = false;
      }
    }

    layer->previous_layer->patch.combine(layer->patch, left_to_right);
    layer->patch = move(layer->previous_layer->patch);
  }

  layers.front()->previous_layer = layers.back()->previous_layer;
  for (auto iter = layers.begin() + 1, end = layers.end(); iter != end; ++iter) {
    delete *iter;
  }
}
