#ifndef MBA_DIFF_H_
#define MBA_DIFF_H_

#include <stdint.h>
#include <vector>

typedef enum {
  DIFF_MATCH = 1,
  DIFF_DELETE,
  DIFF_INSERT
} diff_op;

struct diff_edit {
  short op;
  int off; /* off into s1 if MATCH or DELETE but s2 if INSERT */
  int len;
};

int diff(
  const uint16_t *a, int aoff, int n,
  const uint16_t *b, int boff, int m,
  int dmax, std::vector<diff_edit> *ses
);

#endif  // MBA_DIFF_H_
