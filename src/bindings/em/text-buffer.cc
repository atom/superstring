#include "auto-wrap.h"
#include "text-buffer.h"
#include <emscripten/bind.h>

using std::string;
using std::u16string;

static TextBuffer *construct(const std::wstring &text) {
  return new TextBuffer(u16string(text.begin(), text.end()));
}

static emscripten::val find_sync(TextBuffer &buffer, std::wstring js_pattern, bool ignore_case, Range range) {
  u16string pattern(js_pattern.begin(), js_pattern.end());
  u16string error_message;
  Regex regex(pattern, &error_message, ignore_case);
  if (!error_message.empty()) {
    return emscripten::val(string(error_message.begin(), error_message.end()));
  }

  auto result = buffer.find(regex, range);
  if (result) {
    return emscripten::val(*result);
  }

  return emscripten::val::null();
}

static emscripten::val find_all_sync(TextBuffer &buffer, std::wstring js_pattern, bool ignore_case, Range range) {
  u16string pattern(js_pattern.begin(), js_pattern.end());
  u16string error_message;
  Regex regex(pattern, &error_message, ignore_case);
  if (!error_message.empty()) {
    return emscripten::val(string(error_message.begin(), error_message.end()));
  }

  return em_transmit(buffer.find_all(regex, range));
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

static uint32_t get_line_count(TextBuffer &buffer) {
  return buffer.extent().row + 1;
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
    .function("setText", WRAP_OVERLOAD(&TextBuffer::set_text, void (TextBuffer::*)(u16string &&)))
    .function("getTextInRange", WRAP(&TextBuffer::text_in_range))
    .function("setTextInRange", WRAP_OVERLOAD(&TextBuffer::set_text_in_range, void (TextBuffer::*)(Range, u16string &&)))
    .function("getLength", &TextBuffer::size)
    .function("getExtent", &TextBuffer::extent)
    .function("getLineCount", get_line_count)
    .function("reset", WRAP(&TextBuffer::reset))
    .function("lineLengthForRow", WRAP(&TextBuffer::line_length_for_row))
    .function("lineEndingForRow", line_ending_for_row)
    .function("lineForRow", WRAP(&TextBuffer::line_for_row))
    .function("characterIndexForPosition", character_index_for_position)
    .function("positionForCharacterIndex", position_for_character_index)
    .function("isModified", WRAP_OVERLOAD(&TextBuffer::is_modified, bool (TextBuffer::*)() const))
    .function("findSync", find_sync)
    .function("findAllSync", find_all_sync)
    .function("findWordsWithSubsequenceInRange", WRAP(&TextBuffer::find_words_with_subsequence_in_range));

  emscripten::value_object<TextBuffer::SubsequenceMatch>("SubsequenceMatch")
    .field("word", WRAP_FIELD(TextBuffer::SubsequenceMatch, word))
    .field("positions", WRAP_FIELD(TextBuffer::SubsequenceMatch, positions))
    .field("matchIndices", WRAP_FIELD(TextBuffer::SubsequenceMatch, match_indices))
    .field("score", WRAP_FIELD(TextBuffer::SubsequenceMatch, score));
}
