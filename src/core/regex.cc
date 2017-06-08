#include "regex.h"
#include "pcre2.h"

using std::u16string;
using MatchResult = Regex::MatchResult;

const char16_t EMPTY_PATTERN[] = u".{0}";

Regex::Regex() : code{nullptr} {}

Regex::Regex(const uint16_t *pattern, uint32_t pattern_length, u16string *error_message) {
  if (pattern_length == 0) {
    pattern = reinterpret_cast<const uint16_t *>(EMPTY_PATTERN);
    pattern_length = 4;
  }

  int error_number = 0;
  size_t error_offset = 0;
  code = pcre2_compile(
    pattern,
    pattern_length,
    PCRE2_MULTILINE,
    &error_number,
    &error_offset,
    nullptr
  );

  if (!code) {
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

Regex::Regex(const u16string &pattern, u16string *error_message)
  : Regex(reinterpret_cast<const uint16_t *>(pattern.data()), pattern.size(), error_message) {}

Regex::Regex(Regex &&other) : code{other.code} {
  other.code = nullptr;
}

Regex::~Regex() {
  if (code) pcre2_code_free(code);
}

Regex::MatchData::MatchData(const Regex &regex)
  : data{pcre2_match_data_create_from_pattern(regex.code, nullptr)} {}

Regex::MatchData::~MatchData() {
  pcre2_match_data_free(data);
}

MatchResult Regex::match(const uint16_t *string, size_t length,
                         MatchData &match_data, unsigned options) const {
  MatchResult result{MatchResult::None, 0, 0};

  unsigned int pcre_options = 0;
  if (!(options & MatchOptions::IsEndOfFile)) pcre_options |= PCRE2_PARTIAL_HARD;
  if (!(options & MatchOptions::IsBeginningOfLine)) pcre_options |= PCRE2_NOTBOL;

  int status = pcre2_match(
    code,
    string,
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
