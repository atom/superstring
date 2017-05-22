#ifndef REGEX_H_
#define REGEX_H_

#include <string>

struct pcre2_real_code_16;
struct pcre2_real_match_data_16;

struct Regex {
  pcre2_real_code_16 *code;
  pcre2_real_match_data_16 *match_data;
  std::u16string error_message;

  Regex(const uint16_t *pattern, uint32_t pattern_length);
  ~Regex();

  struct MatchResult {
    enum {
      None,
      Error,
      Partial,
      Full,
    } type;

    size_t start_offset;
    size_t end_offset;
  };

  MatchResult match(const uint16_t *data, size_t length, bool is_last = false);
};

#endif  // REGX_H_