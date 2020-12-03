#include "text-slice.h"
#include "text.h"
#include <cassert>

TextSlice::TextSlice() :
  text{nullptr} {}

TextSlice::TextSlice(const Text *text, Point start_position, Point end_position) :
  text{text}, start_position{start_position}, end_position{end_position} {}

TextSlice::TextSlice(const Text &text) :
  text{&text}, start_position{Point()}, end_position{text.extent()} {}

size_t TextSlice::start_offset() const {
  if (start_position.is_zero()) return 0;
  assert(start_position.row < text->line_offsets.size());
  return text->line_offsets[start_position.row] + start_position.column;
}

size_t TextSlice::end_offset() const {
  if (end_position.is_zero()) return 0;
  return text->line_offsets[end_position.row] + end_position.column;
}

bool TextSlice::is_valid() const {
  uint32_t start_offset = this->start_offset();
  uint32_t end_offset = this->end_offset();

  if (start_offset > end_offset) {
    return false;
  }

  if (start_position.row + 1 < text->line_offsets.size()) {
    if (start_offset >= text->line_offsets[start_position.row + 1]) {
      return false;
    }
  }

  if (end_position.row + 1 < text->line_offsets.size()) {
    if (end_offset >= text->line_offsets[end_position.row + 1]) {
      return false;
    }
  }

  if (end_offset > text->size()) {
    return false;
  }

  return true;
}

std::pair<TextSlice, TextSlice> TextSlice::split(Point split_point) const {
  Point absolute_split_point = Point::min(
    end_position,
    start_position.traverse(split_point)
  );

  return std::pair<TextSlice, TextSlice>{
    TextSlice{text, start_position, absolute_split_point},
    TextSlice{text, absolute_split_point, end_position}
  };
}

std::pair<TextSlice, TextSlice> TextSlice::split(uint32_t split_offset) const {
  return split(position_for_offset(split_offset));
}

Point TextSlice::position_for_offset(uint32_t offset, uint32_t min_row) const {
  return text->position_for_offset(
    offset + start_offset(),
    start_position.row + min_row
  ).traversal(start_position);
}

TextSlice TextSlice::prefix(Point prefix_end) const {
  return split(prefix_end).first;
}

TextSlice TextSlice::prefix(uint32_t prefix_end) const {
  return split(prefix_end).first;
}

TextSlice TextSlice::suffix(Point suffix_start) const {
  return split(suffix_start).second;
}

TextSlice TextSlice::slice(Range range) const {
  return suffix(range.start).prefix(range.extent());
}

Point TextSlice::extent() const {
  return end_position.traversal(start_position);
}

const char16_t *TextSlice::data() const {
  return text->data() + start_offset();
}

uint32_t TextSlice::size() const {
  return end_offset() - start_offset();
}

bool TextSlice::empty() const {
  return size() == 0;
}

Text::const_iterator TextSlice::begin() const {
  return text->cbegin() + start_offset();
}

Text::const_iterator TextSlice::end() const {
  return text->cbegin() + end_offset();
}

uint16_t TextSlice::front() const {
  return *begin();
}

uint16_t TextSlice::back() const {
  return *(end() - 1);
}
