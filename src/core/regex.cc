#include "regex.h"
#include "pcre2.h"

using MatchResult = Regex::MatchResult;

Regex::Regex(const uint16_t *pattern, uint32_t pattern_length) {
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
    error_message.assign(message_buffer, message_buffer + length);
    match_data = nullptr;
    return;
  }

  match_data = pcre2_match_data_create_from_pattern(code, nullptr);
}

Regex::~Regex() {
  if (code) {
    pcre2_match_data_free(match_data);
    pcre2_code_free(code);
  }
}

MatchResult Regex::match(const uint16_t *data, size_t length) {
  MatchResult result{MatchResult::None, 0, 0};

  int status = pcre2_match(
    code,
    data,
    length,
    0,
    PCRE2_PARTIAL_HARD,
    match_data,
    nullptr
  );

  if (status < 0) {
    switch (status) {
      case PCRE2_ERROR_PARTIAL:
        result.type = MatchResult::Partial;
        result.start_offset = pcre2_get_ovector_pointer(match_data)[0];
        result.end_offset = pcre2_get_ovector_pointer(match_data)[1];
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
    result.start_offset = pcre2_get_ovector_pointer(match_data)[0];
    result.end_offset = pcre2_get_ovector_pointer(match_data)[1];
  }

  return result;
}
