#include "auto-wrap.h"
#include "text-buffer.h"
#include <emscripten/bind.h>

using std::string;
using std::u16string;

static TextBuffer *construct(const std::string &text) {
  return new TextBuffer(u16string(text.begin(), text.end()));
}

static emscripten::val search_sync(TextBuffer &buffer, std::string pattern) {
  auto result = buffer.search(u16string(pattern.begin(), pattern.end()));
  if (!result.error_message.empty()) {
    return emscripten::val(string(
      result.error_message.begin(),
      result.error_message.end()
    ));
  }

  if (result.range) {
    return emscripten::val(*result.range);
  }

  return emscripten::val::null();
}

static emscripten::val line_ending_for_row(TextBuffer &buffer, uint32_t row) {
  auto line_ending = buffer.line_ending_for_row(row);
  if (line_ending) {
    string result;
    for (const uint16_t *character = line_ending; *character != 0; character++) {
      result += (char)*character;
    }
    return emscripten::val(result);
  }
  return emscripten::val::undefined();
}

static uint32_t character_index_for_position(TextBuffer &buffer, Point position) {
  return buffer.clip_position(position).offset;
}

static Point position_for_character_index(TextBuffer &buffer, long index) {
  return index < 0 ?
    Point{0, 0} :
    buffer.position_for_offset(static_cast<uint32_t>(index));
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
    .function("lineLengthForRow", WRAP(&TextBuffer::line_length_for_row))
    .function("lineEndingForRow", line_ending_for_row)
    .function("lineForRow", WRAP(&TextBuffer::line_for_row))
    .function("characterIndexForPosition", character_index_for_position)
    .function("positionForCharacterIndex", position_for_character_index)
    .function("isModified", &TextBuffer::is_modified)
    .function("searchSync", search_sync);
}
