#include "text-buffer.h"

TextBuffer::TextBuffer(Text &&base_text) : base_text {std::move(base_text)} {}

TextBuffer TextBuffer::build(std::istream &stream, size_t input_size, const char *encoding_name,
                             size_t cchange_size, std::function<void(size_t)> progress_callback) {
  return TextBuffer {Text::build(stream, input_size, encoding_name, cchange_size, progress_callback)};
}

Text TextBuffer::text() const {
  return Text {base_text};
}
