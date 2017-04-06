#include "auto-wrap.h"
#include "text-buffer.h"
#include <emscripten/bind.h>

static TextBuffer *construct(const std::string &text) {
  return new TextBuffer(std::u16string(text.begin(), text.end()));
}

static int search(TextBuffer &buffer, std::string pattern) {
  return buffer.search(std::u16string(pattern.begin(), pattern.end()));
}

EMSCRIPTEN_BINDINGS(TextBuffer) {
  emscripten::class_<TextBuffer>("TextBuffer")
    .constructor<>()
    .constructor(construct, emscripten::allow_raw_pointers())
    .function("getText", WRAP(&TextBuffer::text))
    .function("setText", WRAP(&TextBuffer::set_text))
    .function("getTextInRange", WRAP(&TextBuffer::text_in_range))
    .function("setTextInRange", WRAP(&TextBuffer::set_text_in_range))
    .function("getLength", &TextBuffer::size)
    .function("getExtent", &TextBuffer::extent)
    .function("lineLengthForRow", &TextBuffer::line_length_for_row)
    .function("isModified", &TextBuffer::is_modified)
    .function("searchSync", search);
}
