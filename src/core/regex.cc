#include "regex.h"
#include <stdlib.h>
#include "pcre2.h"

using std::u16string;
using MatchResult = Regex::MatchResult;

const char16_t EMPTY_PATTERN[] = u".{0}";

Regex::Regex() : code{nullptr} {}

static u16string preprocess_pattern(const char16_t *pattern, uint32_t length) {
  u16string result;
  for (unsigned i = 0; i < length;) {
    char16_t c = pattern[i];

    // Replace escape sequences like '\u00cf' with their literal UTF16 value
    if (c == '\\' && i + 1 < length) {
      if (pattern[i + 1] == 'u') {
        if (i + 6 <= length) {
          std::string char_code_string(&pattern[i + 2], &pattern[i + 6]);
          char16_t char_code_value = strtol(char_code_string.data(), nullptr, 16);
          if (char_code_value != 0) {
            result += char_code_value;
            i += 6;
            continue;
          }
        }

        // Replace invalid '\u' escape sequences with the literal characters '\' and 'u'
        result += u"\\\\u";
        i += 2;
        continue;
      } else if (pattern[i + 1] == '\\') {
        // Prevent '\\u' from UTF16 replacement
        result += u"\\\\";
        i += 2;
        continue;
      }
    }

    result += c;
    i++;
  }

  return result;
}


Regex::Regex(const char16_t *pattern, uint32_t pattern_length, u16string *error_message, bool ignore_case, bool unicode) {
  if (pattern_length == 0) {
    pattern = EMPTY_PATTERN;
    pattern_length = 4;
  }

  u16string final_pattern = preprocess_pattern(pattern, pattern_length);

  int error_number = 0;
  size_t error_offset = 0;
  uint32_t options = PCRE2_MULTILINE;
  if (ignore_case) options |= PCRE2_CASELESS;
  if (unicode) options |= PCRE2_UTF;
  code = pcre2_compile(
    reinterpret_cast<const uint16_t *>(final_pattern.data()),
    final_pattern.size(),
    options,
    &error_number,
    &error_offset,
    nullptr
  );

  if (code == nullptr) {
    uint16_t message_buffer[256];
    size_t length = pcre2_get_error_message(error_number, message_buffer, 256);
    error_message->assign(message_buffer, message_buffer + length);
    return;
  }

  pcre2_jit_compile(
    code,
    PCRE2_JIT_COMPLETE|PCRE2_JIT_PARTIAL_HARD|PCRE2_JIT_PARTIAL_SOFT
  );
}

Regex::Regex(const u16string &pattern, u16string *error_message, bool ignore_case, bool unicode)
  : Regex(pattern.data(), pattern.size(), error_message, ignore_case, unicode) {}

Regex::Regex(Regex &&other) : code{other.code} {
  other.code = nullptr;
}

Regex::~Regex() {
  if (code != nullptr) pcre2_code_free(code);
}

Regex::MatchData::MatchData(const Regex &regex)
  : data{pcre2_match_data_create_from_pattern(regex.code, nullptr)} {}

Regex::MatchData::~MatchData() {
  pcre2_match_data_free(data);
}

MatchResult Regex::match(const char16_t *string, size_t length,
                         MatchData &match_data, unsigned options) const {
  MatchResult result{MatchResult::None, 0, 0};

  unsigned int pcre_options = 0;
  if (!(options & MatchOptions::IsEndSearch)) pcre_options |= PCRE2_PARTIAL_HARD;
  if (!(options & MatchOptions::IsBeginningOfLine)) pcre_options |= PCRE2_NOTBOL;
  if (!(options & MatchOptions::IsEndOfLine)) pcre_options |= PCRE2_NOTEOL;

  int status = pcre2_match(
    code,
    reinterpret_cast<const uint16_t *>(string),
    length,
    0,
    pcre_options,
    match_data.data,
    nullptr
  );

  if (status < 0) {
    switch (status) {
      case PCRE2_ERROR_PARTIAL:
        result.type = MatchResult::Partial;
        result.start_offset = pcre2_get_ovector_pointer(match_data.data)[0];
        result.end_offset = pcre2_get_ovector_pointer(match_data.data)[1];
        break;
      case PCRE2_ERROR_NOMATCH:
        result.type = MatchResult::None;
        break;
      default:
        result.type = MatchResult::Error;
        break;
    }
  } else {
    result.type = MatchResult::Full;
    result.start_offset = pcre2_get_ovector_pointer(match_data.data)[0];
    result.end_offset = pcre2_get_ovector_pointer(match_data.data)[1];
  }

  return result;
}
