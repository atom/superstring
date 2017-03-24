#include "text-buffer.h"
#include "text-slice.h"
#include <vector>

using std::move;
using std::vector;

TextBuffer::BaseLayer::BaseLayer(Text &&text) : text{text} {}

uint32_t TextBuffer::BaseLayer::size() const {
  return text.size();
}

Point TextBuffer::BaseLayer::extent() const {
  return text.extent();
}

uint16_t TextBuffer::BaseLayer::character_at(Point position) {
  return text.at(position);
}

ClipResult TextBuffer::BaseLayer::clip_position(Point position) {
  return text.clip_position(position);
}

void TextBuffer::BaseLayer::add_chunks_in_range(TextChunkCallback *callback, Point start, Point end) {
  (*callback)(TextSlice(text).prefix(end).suffix(start));
}

static inline Point previous_column(Point position) {
  return Point(position.row, position.column - 1);
}

TextBuffer::DerivedLayer::DerivedLayer(TextBuffer::Layer *previous_layer) :
  previous_layer{previous_layer},
  extent_{previous_layer->extent()},
  size_{previous_layer->size()} {}

uint32_t TextBuffer::DerivedLayer::size() const {
  return size_;
}

uint16_t TextBuffer::DerivedLayer::character_at(Point position) {
  auto change = patch.change_for_new_position(position);
  if (!change) return previous_layer->character_at(position);

  if (position < change->new_end) {
    return change->new_text->at(position);
  } else {
    return previous_layer->character_at(
      change->old_end.traverse(position.traversal(change->new_end))
    );
  }
}

ClipResult TextBuffer::DerivedLayer::clip_position(Point position) {
  auto preceding_change = patch.change_for_new_position(position);
  if (!preceding_change) return previous_layer->clip_position(position);

  ClipResult preceding_change_base_location =
    previous_layer->clip_position(preceding_change->old_start);
  Point preceding_change_base_position = preceding_change_base_location.position;
  uint32_t preceding_change_base_offset = preceding_change_base_location.offset;

  uint32_t preceding_change_current_offset =
    preceding_change_base_offset +
    preceding_change->preceding_new_text_size -
    preceding_change->preceding_old_text_size;

  if (position < preceding_change->new_end) {
    ClipResult position_within_preceding_change =
      preceding_change->new_text->clip_position(
        position.traversal(preceding_change->new_start)
      );

    if (position_within_preceding_change.offset == 0 && preceding_change_base_position.column > 0) {
      if (previous_layer->character_at(previous_column(preceding_change_base_position)) == '\r' &&
          preceding_change->new_text->content.front() == '\n') {
        Point result = preceding_change->new_start;
        result.column--;
        return {result, preceding_change_current_offset - 1};
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
    Point base_position = base_location.position;
    uint32_t base_offset = base_location.offset;

    ClipResult distance_past_preceding_change = {
      base_position.traversal(preceding_change->old_end),
      base_offset - preceding_change_base_offset - preceding_change->old_text_size
    };

    ClipResult result = {
      preceding_change->new_end.traverse(distance_past_preceding_change.position),
      preceding_change_current_offset + preceding_change->new_text->size() + distance_past_preceding_change.offset
    };

    if (distance_past_preceding_change.offset == 0 &&
        base_offset < previous_layer->size()) {
      uint16_t previous_character = 0;
      if (preceding_change->new_text->size() > 0) {
        previous_character = preceding_change->new_text->content.back();
      } else if (preceding_change_base_offset > 0) {
        previous_character = previous_layer->character_at(previous_column(preceding_change_base_position));
      }

      if (previous_character == '\r' && previous_layer->character_at(base_position) == '\n') {
        result.offset--;
        result.position.column--;
      }
    }

    return result;
  }
}

void TextBuffer::DerivedLayer::add_chunks_in_range(TextChunkCallback *callback, Point start, Point end) {
  Point goal_position = clip_position(end).position;
  Point current_position = clip_position(start).position;
  Point base_position = current_position;
  auto change = patch.change_for_new_position(current_position);

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

    change = patch.change_ending_after_new_position(current_position, true);

    Point next_base_position, next_position;
    if (change) {
      next_position = Point::min(goal_position, change->new_start);
      next_base_position = Point::min(base_position.traverse(goal_position.traversal(current_position)), change->old_start);
    } else {
      next_position = goal_position;
      next_base_position = base_position.traverse(goal_position.traversal(current_position));
    }

    previous_layer->add_chunks_in_range(callback, base_position, next_base_position);
    base_position = next_base_position;
    current_position = next_position;
  }
}

void TextBuffer::DerivedLayer::set_text_in_range(Range old_range, Text &&new_text) {
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

Point TextBuffer::DerivedLayer::extent() const {
  return extent_;
}

TextBuffer::TextBuffer(Text &&text) :
  base_layer{std::move(text)} {
  derived_layers.emplace_back(&base_layer);
}

TextBuffer::TextBuffer() {
  derived_layers.emplace_back(&base_layer);
}

TextBuffer::TextBuffer(std::u16string text) : TextBuffer {Text {text}} {}

bool TextBuffer::reset(Text &&new_base_text) {
  if (derived_layers.size() > 1) return false;
  if (derived_layers[0].patch.get_change_count() > 0) return false;
  derived_layers[0].extent_ = new_base_text.extent();
  derived_layers[0].size_ = new_base_text.size();
  base_layer.text = move(new_base_text);
  return true;
}

bool TextBuffer::save(std::ostream &stream, const char *encoding_name, size_t chunk_size) {
  return true;
}

Point TextBuffer::extent() const {
  return derived_layers.back().extent();
}

uint32_t TextBuffer::size() const {
  return derived_layers.back().size();
}

uint32_t TextBuffer::line_length_for_row(uint32_t row) {
  return derived_layers.back().clip_position(Point{row, UINT32_MAX}).position.column;
}

Point TextBuffer::clip_position(Point position) {
  if (position > extent()) {
    return extent();
  } else {
    uint32_t max_column = line_length_for_row(position.row);
    if (position.column > max_column) {
      return Point {position.row, max_column};
    } else {
      return position;
    }
  }
}

Range TextBuffer::clip_range(Range range) {
  return Range {clip_position(range.start), clip_position(range.end)};
}

Text TextBuffer::text() {
  return text_in_range(Range{Point(), extent()});
}

Text TextBuffer::text_in_range(Range range) {
  struct Appender : public TextChunkCallback {
    Text text;
    void operator()(TextSlice chunk) { text.append(chunk); }
  };

  Appender appender;
  derived_layers.back().add_chunks_in_range(&appender, range.start, range.end);
  return appender.text;
}

void TextBuffer::set_text(Text &&new_text) {
  set_text_in_range(Range {Point(0, 0), extent()}, move(new_text));
}

void TextBuffer::set_text_in_range(Range old_range, Text &&new_text) {
  derived_layers.back().set_text_in_range(old_range, move(new_text));
}

