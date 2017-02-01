#include "text-buffer.h"

TextBuffer::TextBuffer(Text &&base_text) :
  base_text {std::move(base_text)},
  extent_ {base_text.extent()} {}

TextBuffer::TextBuffer(std::u16string text) : base_text {Text {text}} {}

TextBuffer TextBuffer::build(std::istream &stream, size_t input_size, const char *encoding_name,
                             size_t chunk_size, std::function<void(size_t)> progress_callback) {
  return TextBuffer {Text::build(stream, input_size, encoding_name, chunk_size, progress_callback)};
}

Point TextBuffer::extent() const {
  return extent_;
}

Text TextBuffer::text() {
  return Text {begin(), end()};
}

// Text TextBuffer::text_in_range(Range range) const {
//   // return Text{begin() + range.start, begin() + range.end};
// }

void TextBuffer::set_text_in_range(Range old_range, Text &&new_text) {
  patch.splice(old_range.start, old_range.extent(), new_text.extent(), optional<Text> {}, new_text);
}

TextBuffer::iterator TextBuffer::begin() {
  return iterator {*this};
}

TextBuffer::iterator TextBuffer::end() {
  return iterator {*this}.traverse(extent());
}

TextBuffer::iterator::iterator(TextBuffer &buffer) :
  buffer {buffer},
  offset {0},
  next_hunk {buffer.patch.hunk_ending_after_new_position({0, 0})},
  next_hunk_offset {
    next_hunk ?
      optional<uint32_t> {buffer.base_text.offset_for_position(next_hunk->old_start)} :
      optional<uint32_t> {}
  } {}

uint16_t TextBuffer::iterator::operator*() const {
  if (next_hunk && offset >= *next_hunk_offset) {
    return next_hunk->new_text->at(offset - *next_hunk_offset);
  } else {
    return buffer.base_text.at(offset);
  }
}

TextBuffer::iterator &TextBuffer::iterator::operator++() {
  offset++;
  if (next_hunk && offset == *next_hunk_offset + next_hunk->new_text->size()) {
    offset = buffer.base_text.offset_for_position(next_hunk->old_end);
    if ((next_hunk = buffer.patch.hunk_ending_after_new_position(next_hunk->new_end, true))) {
      next_hunk_offset = buffer.base_text.offset_for_position(next_hunk->old_start);
    } else {
      next_hunk_offset = optional<uint32_t> {};
    }
  }
  return *this;
}

bool TextBuffer::iterator::operator!=(const iterator &other) const {
  return &buffer == &other.buffer && offset != other.offset;
}

// TODO: implement
TextBuffer::iterator::difference_type TextBuffer::iterator::operator-(const iterator &other) const {
  return 0;
}

// TODO: implement
TextBuffer::iterator TextBuffer::iterator::traverse(Point point) const {
  return buffer.begin();
}
