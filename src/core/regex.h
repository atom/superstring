#ifndef REGEX_H_
#define REGEX_H_

#include "pcre2.h"

struct Regex {
  pcre2_code *code;
  pcre2_match_data *match_data;
  std::vector<uint16_t> error_message;

  Regex(const uint16_t *pattern, uint32_t pattern_length) {
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
      error_message.resize(256);
      error_message.resize(pcre2_get_error_message(error_number, error_message.data(), error_message.size()));
      match_data = nullptr;
      return;
    }

    match_data = pcre2_match_data_create_from_pattern(code, nullptr);
  }

  ~Regex() {
    if (code) {
      pcre2_match_data_free(match_data);
      pcre2_code_free(code);
    }
  }

  int match(const uint16_t *data, size_t length) {
    return pcre2_match(
      code,
      data,
      length,
      0,
      PCRE2_PARTIAL_HARD,
      match_data,
      nullptr
    );
  }

  size_t get_match_offset(uint32_t index) const {
    return pcre2_get_ovector_pointer(match_data)[index];
  }
};

#endif  // REGX_H_