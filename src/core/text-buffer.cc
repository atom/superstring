#include "text-buffer.h"

TextBuffer::TextBuffer(Text &&text) :
  base_text {std::move(text)},
  extent_ {base_text.extent()} {}

TextBuffer::TextBuffer(std::u16string text) : TextBuffer {Text {text}} {}

TextBuffer TextBuffer::build(std::istream &stream, size_t input_size, const char *encoding_name,
                             size_t cchange_size, std::function<void(size_t)> progress_callback) {
  return TextBuffer {Text::build(stream, input_size, encoding_name, cchange_size, progress_callback)};
}

Point TextBuffer::extent() const {
  return extent_;
}

Text TextBuffer::text() {
  return Text {begin(), end()};
}

Text TextBuffer::text_in_range(Range range) {
  return Text {
    begin().traverse(range.start),
    begin().traverse(range.end)
  };
}

void TextBuffer::set_text_in_range(Range old_range, Text &&new_text) {
  Point new_range_end = old_range.start.traverse(new_text.extent());
  uint32_t deleted_text_size = begin().traverse(old_range.end) - begin().traverse(old_range.start);
  patch.splice(old_range.start, old_range.extent(), new_text.extent(), optional<Text> {}, new_text, deleted_text_size);
  extent_ = new_range_end.traverse(extent_.traversal(old_range.end));
}

TextBuffer::Iterator TextBuffer::begin() {
  return Iterator {*this};
}

TextBuffer::Iterator TextBuffer::end() {
  return Iterator {*this}.traverse(extent());
}

TextBuffer::Iterator::Iterator(TextBuffer &buffer) :
  buffer {buffer},
  next_change {optional<Patch::Change> {}},
  offset_from_next_change_start {0},
  next_change_base_offset {0},
  current_offset {0} {
  fetch_next_change();
}

uint16_t TextBuffer::Iterator::operator*() const {
  if (next_change && offset_from_next_change_start >= 0) {
    return next_change->new_text->at(offset_from_next_change_start);
  } else {
    return buffer.base_text.at(next_change_base_offset + offset_from_next_change_start);
  }
}

TextBuffer::Iterator &TextBuffer::Iterator::operator++() {
  current_offset++;
  offset_from_next_change_start++;
  if (next_change && offset_from_next_change_start == next_change->new_text->size()) {
    fetch_next_change();
  }
  return *this;
}

bool TextBuffer::Iterator::operator!=(const Iterator &other) const {
  return &buffer != &other.buffer || current_offset != other.current_offset;
}

TextBuffer::Iterator::difference_type TextBuffer::Iterator::operator-(const Iterator &other) const {
  return current_offset - other.current_offset;
}

void TextBuffer::Iterator::fetch_next_change() {
  if (next_change) {
    next_change = buffer.patch.change_ending_after_new_position(next_change->new_end, true);
  } else {
    next_change = buffer.patch.change_ending_after_new_position(Point {0, 0});
  }

  uint32_t base_offset = next_change_base_offset + offset_from_next_change_start;
  if (next_change) {
    next_change_base_offset = buffer.base_text.offset_for_position(next_change->old_start);
  } else {
    next_change_base_offset = buffer.base_text.size();
  }
  offset_from_next_change_start = static_cast<int64_t>(base_offset) - next_change_base_offset;
}

TextBuffer::Iterator TextBuffer::Iterator::traverse(Point position) const {
  auto result = buffer.begin();

  auto preceding_change = buffer.patch.change_for_new_position(position);
  if (preceding_change) {
    uint32_t preceding_change_base_offset =
      buffer.base_text.offset_for_position(preceding_change->old_start);
    int32_t preceding_text_delta =
      preceding_change->preceding_new_text_size -
      preceding_change->preceding_old_text_size;
    uint32_t preceding_change_current_offset =
      preceding_change_base_offset + preceding_text_delta;

    uint32_t offset_from_preceding_change_start;
    if (position <= preceding_change->new_end) {
      offset_from_preceding_change_start =
        preceding_change->new_text->offset_for_position(
          position.traversal(preceding_change->new_start)
        );
    } else {
      Point base_text_position =
        preceding_change->old_end.traverse(
          position.traversal(preceding_change->new_end)
        );
      offset_from_preceding_change_start =
        preceding_change->new_text->size() +
        (
          buffer.base_text.offset_for_position(base_text_position) -
          buffer.base_text.offset_for_position(preceding_change->old_end)
        );
    }

    result.next_change = preceding_change;
    result.next_change_base_offset = preceding_change_base_offset;
    result.offset_from_next_change_start = offset_from_preceding_change_start;
    result.current_offset = preceding_change_current_offset + offset_from_preceding_change_start;
    result.fetch_next_change();
  } else {
    result.current_offset = buffer.base_text.offset_for_position(position);
    result.offset_from_next_change_start = static_cast<int64_t>(result.current_offset) - result.next_change_base_offset;
  }

  return result;
}
