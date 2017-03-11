#include "text-buffer.h"
#include "text-slice.h"

using std::move;

TextBuffer::TextBuffer(Text &&text) :
  base_text {std::move(text)},
  extent_ {base_text.extent()},
  size_ {base_text.size()} {}

TextBuffer::TextBuffer(std::u16string text) : TextBuffer {Text {text}} {}

TextBuffer TextBuffer::build(std::istream &stream, size_t input_size, const char *encoding_name,
                             size_t cchange_size, std::function<void(size_t)> progress_callback) {
  return TextBuffer {Text::build(stream, input_size, encoding_name, cchange_size, progress_callback)};
}

Point TextBuffer::extent() const {
  return extent_;
}

uint32_t TextBuffer::size() const {
  return size_;
}

uint32_t TextBuffer::line_length_for_row(uint32_t row) {
  return begin().traverse(Point {row, UINT32_MAX}) - begin().traverse(Point {row, 0});
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
  return Text {begin(), end()};
}

Text TextBuffer::text_in_range(Range range) {
  return Text {
    begin().traverse(range.start),
    begin().traverse(range.end)
  };
}

void TextBuffer::set_text_in_range(Range old_range, Text &&new_text) {
  old_range = clip_range(old_range);
  Point new_range_end = old_range.start.traverse(new_text.extent());
  uint32_t deleted_text_size = begin().traverse(old_range.end) - begin().traverse(old_range.start);
  extent_ = new_range_end.traverse(extent_.traversal(old_range.end));
  size_ += new_text.size() - deleted_text_size;
  patch.splice(old_range.start, old_range.extent(), new_text.extent(), optional<Text> {}, move(new_text), deleted_text_size);
}

TextBuffer::Iterator TextBuffer::begin() {
  return Iterator {*this, false};
}

TextBuffer::Iterator TextBuffer::end() {
  return Iterator {*this, true};
}

TextBuffer::Iterator::Iterator(TextBuffer &buffer, bool end) :
  buffer {buffer},
  next_change {optional<Patch::Change> {}},
  offset_from_next_change_start {0} {
  if (end) {
    next_change_base_offset = buffer.base_text.size();
    current_offset = buffer.size();
  } else {
    next_change_base_offset = 0;
    current_offset = 0;
    fetch_next_change();
  }
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
  do {
    int64_t preceding_text_delta;
    if (next_change) {
      preceding_text_delta =
        static_cast<int64_t>(next_change->preceding_new_text_size) + next_change->new_text->size() -
        next_change->preceding_old_text_size - next_change->old_text_size;
      next_change = buffer.patch.change_ending_after_new_position(next_change->new_end, true);
    } else {
      preceding_text_delta = 0;
      next_change = buffer.patch.change_ending_after_new_position(Point {0, 0});
    }

    if (next_change) {
      next_change_base_offset = buffer.base_text.offset_for_position(next_change->old_start);
    } else {
      next_change_base_offset = buffer.base_text.size();
    }

    uint32_t next_change_current_offset = next_change_base_offset + preceding_text_delta;
    offset_from_next_change_start = static_cast<int64_t>(current_offset) - next_change_current_offset;
  } while (next_change && next_change->new_end.is_zero());
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

    int64_t offset_from_preceding_change_start;
    if (position < preceding_change->new_end) {
      offset_from_preceding_change_start =
        preceding_change->new_text->offset_for_position(
          position.traversal(preceding_change->new_start)
        );

      if (offset_from_preceding_change_start == 0 && preceding_change_base_offset > 0) {
        if (buffer.base_text.at(preceding_change_base_offset - 1) == '\r' &&
            preceding_change->new_text->content.front() == '\n')
        offset_from_preceding_change_start--;
      }
    } else {
      Point base_position =
        preceding_change->old_end.traverse(
          position.traversal(preceding_change->new_end)
        );

      uint32_t base_offset =
        buffer.base_text.offset_for_position(base_position);
      uint32_t preceding_change_end_base_offset =
        buffer.base_text.offset_for_position(preceding_change->old_end);
      offset_from_preceding_change_start =
        preceding_change->new_text->size() + base_offset - preceding_change_end_base_offset;

      if (base_offset == preceding_change_end_base_offset &&
          base_offset < buffer.base_text.size()) {
        uint16_t previous_character = 0;
        if (offset_from_preceding_change_start > 0) {
          previous_character = preceding_change->new_text->content.back();
        } else if (preceding_change_base_offset > 0) {
          previous_character = buffer.base_text.at(preceding_change_base_offset - 1);
        }

        if (previous_character == '\r' && buffer.base_text.at(base_offset) == '\n') {
          offset_from_preceding_change_start--;
        }
      }
    }

    result.next_change = preceding_change;
    result.next_change_base_offset = preceding_change_base_offset;
    result.offset_from_next_change_start = offset_from_preceding_change_start;
    result.current_offset = preceding_change_current_offset + offset_from_preceding_change_start;
    if (offset_from_preceding_change_start >= 0) result.fetch_next_change();
  } else {
    result.current_offset = buffer.base_text.offset_for_position(position);
    result.offset_from_next_change_start = static_cast<int64_t>(result.current_offset) - result.next_change_base_offset;
  }

  return result;
}
