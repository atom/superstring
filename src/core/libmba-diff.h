#ifndef MBA_DIFF_H_
#define MBA_DIFF_H_

#include <cstdint>
#include <vector>

typedef enum {
  DIFF_MATCH = 1,
  DIFF_DELETE,
  DIFF_INSERT
} diff_op;

struct diff_edit {
  diff_op op;
  uint32_t off; /* off into s1 if MATCH or DELETE but s2 if INSERT */
  uint32_t len;
};

int diff(
  const char16_t *old_text, uint32_t old_length,
  const char16_t *new_text, uint32_t new_length,
  int dmax, std::vector<diff_edit> *ses
);

#endif  // MBA_DIFF_H_
