/*
 * This file is based on `diff.c` from `libmba`
 * http://www.ioplex.com/~miallen/libmba
 *
 * It has been modified in the following ways:
 *
 * 1. The parameters have been changed from `void *` to `char16_t *`;
 *    the algorithm is now hard-coded to work on UTF16 input.
 * 2. The edit script and the internal buffer are now stored as `std::vector`
 *    instead of `libmba`'s custom C array type `varray`. This change speeds up
 *    the algorithm significantly and eliminates a dependency on other `libmba`
 *    components.
 * 3. Some optional parameters to the function have been removed.
 * 4. The whitespace has been normalized.
 * 5. Before computing the edit script, the common suffix is removed from th
 *    two input strings.
 */

/* diff - compute a shortest edit script (SES) given two sequences
 * Copyright (c) 2004 Michael B. Allen <mba2000 ioplex.com>
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/* This algorithm is basically Myers' solution to SES/LCS with
 * the Hirschberg linear space refinement as described in the
 * following publication:
 *
 *   E. Myers, ``An O(ND) Difference Algorithm and Its Variations,''
 *   Algorithmica 1, 2 (1986), 251-266.
 *   http://www.cs.arizona.edu/people/gene/PAPERS/diff.ps
 *
 * This is the same algorithm used by GNU diff(1).
 */

#include "libmba-diff.h"
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <vector>

using std::vector;

#define FV(k) _v(ctx, (k), 0)
#define RV(k) _v(ctx, (k), 1)

struct _ctx {
  vector<int> buf;
  vector<diff_edit> *ses;
  int dmax;
};

struct middle_snake {
  uint32_t x, y, u, v;
};

static void _setv(struct _ctx *ctx, int k, int r, int val) {
  /* Pack -N to N into 0 to N * 2 */
  uint32_t j = k <= 0 ? -k * 4 + r : k * 4 + (r - 2);
  if (ctx->buf.size() < j + 1) {
    ctx->buf.resize(j + 1);
  }
  ctx->buf[j] = val;
}

static int _v(struct _ctx *ctx, int k, int r) {
  uint32_t j = k <= 0 ? -k * 4 + r : k * 4 + (r - 2);
  return ctx->buf[j];
}

static int _find_middle_snake(
  const char16_t *a, int aoff, int n,
  const char16_t *b, int boff, int m,
  struct _ctx *ctx, struct middle_snake *ms
) {
  int delta, odd, mid, d;

  delta = n - m;
  odd = delta & 1;
  mid = (n + m) / 2;
  mid += odd;

  _setv(ctx, 1, 0, 0);
  _setv(ctx, delta - 1, 1, n);

  for (d = 0; d <= mid; d++) {
    int k, x, y;

    if ((2 * d - 1) >= ctx->dmax) {
      return ctx->dmax;
    }

    for (k = d; k >= -d; k -= 2) {
      if (k == -d || (k != d && FV(k - 1) < FV(k + 1))) {
        x = FV(k + 1);
      } else {
        x = FV(k - 1) + 1;
      }
      y = x - k;

      ms->x = x;
      ms->y = y;
      const char16_t *a0 = a + aoff;
      const char16_t *b0 = b + boff;
      while (x < n && y < m && a0[x] == b0[y]) {
        x++; y++;
      }
      _setv(ctx, k, 0, x);

      if ((odd != 0) && k >= (delta - (d - 1)) && k <= (delta + (d - 1))) {
        if (x >= RV(k)) {
          ms->u = x;
          ms->v = y;
          return 2 * d - 1;
        }
      }
    }

    for (k = d; k >= -d; k -= 2) {
      int kr = (n - m) + k;

      if (k == d || (k != -d && RV(kr - 1) < RV(kr + 1))) {
        x = RV(kr - 1);
      } else {
        x = RV(kr + 1) - 1;
      }
      y = x - kr;

      ms->u = x;
      ms->v = y;
      const char16_t *a0 = a + aoff;
      const char16_t *b0 = b + boff;
      while (x > 0 && y > 0 && a0[x - 1] == b0[y - 1]) {
        x--; y--;
      }
      _setv(ctx, kr, 1, x);

      if ((odd == 0) && kr >= -d && kr <= d) {
        if (x <= FV(kr)) {
          ms->x = x;
          ms->y = y;
          return 2 * d;
        }
      }
    }
  }

  errno = EFAULT;

  return -1;
}

static void _edit(struct _ctx *ctx, diff_op op, int off, int len) {
  // Add an edit to the SES (or coalesce if the op is the same)
  auto *e = &ctx->ses->back();
  if (e->op != op) {
    if (e->op != 0) {
      ctx->ses->push_back(diff_edit{});
      e = &ctx->ses->back();
    }
    e->op = op;
    e->off = off;
    e->len = len;
  } else {
    e->len += len;
  }
}

static int _ses(
  const char16_t *a, uint32_t aoff, uint32_t n,
  const char16_t *b, uint32_t boff, uint32_t m,
  struct _ctx *ctx
) {
  struct middle_snake ms;
  int d;

  if (n == 0) {
    _edit(ctx, DIFF_INSERT, boff, m);
    d = m;
  } else if (m == 0) {
    _edit(ctx, DIFF_DELETE, aoff, n);
    d = n;
  } else {
    /* Find the middle "snake" around which we
     * recursively solve the sub-problems.
     */
    d = _find_middle_snake(a, aoff, n, b, boff, m, ctx, &ms);
    if (d == -1) {
      return -1;
    } else if (d >= ctx->dmax) {
      return ctx->dmax;
    } else if (ctx->ses == NULL) {
      return d;
    } else if (d > 1) {
      if (_ses(a, aoff, ms.x, b, boff, ms.y, ctx) == -1) {
        return -1;
      }

      _edit(ctx, DIFF_MATCH, aoff + ms.x, ms.u - ms.x);

      aoff += ms.u;
      boff += ms.v;
      n -= ms.u;
      m -= ms.v;
      if (_ses(a, aoff, n, b, boff, m, ctx) == -1) {
        return -1;
      }
    } else {
      int x = ms.x;
      int u = ms.u;

     /* There are only 4 base cases when the
      * edit distance is 1.
      *
      * n > m   m > n
      *
      *   -       |
      *    \       \    x != u
      *     \       \
      *
      *   \       \
      *    \       \    x == u
      *     -       |
      */
      if (m > n) {
        if (x == u) {
          _edit(ctx, DIFF_MATCH, aoff, n);
          _edit(ctx, DIFF_INSERT, boff + (m - 1), 1);
        } else {
          _edit(ctx, DIFF_INSERT, boff, 1);
          _edit(ctx, DIFF_MATCH, aoff, n);
        }
      } else {
        if (x == u) {
          _edit(ctx, DIFF_MATCH, aoff, m);
          _edit(ctx, DIFF_DELETE, aoff + (n - 1), 1);
        } else {
          _edit(ctx, DIFF_DELETE, aoff, 1);
          _edit(ctx, DIFF_MATCH, aoff + 1, m);
        }
      }
    }
  }

  return d;
}

int diff(const char16_t *a, uint32_t n, const char16_t *b, uint32_t m,
         int dmax, vector<diff_edit> *ses) {
  struct _ctx ctx;
  ctx.ses = ses;
  ctx.dmax = dmax != 0 ? dmax : INT_MAX;
  ses->push_back(diff_edit{static_cast<diff_op>(0), 0, 0});

  uint32_t common_prefix_length = 0;
  while (common_prefix_length < n &&
         common_prefix_length < m &&
         a[common_prefix_length] == b[common_prefix_length]) {
    common_prefix_length++;
  }

  _edit(&ctx, DIFF_MATCH, 0, common_prefix_length);

  uint32_t common_suffix_length = 0;
  while (n > common_prefix_length &&
         m > common_prefix_length &&
         a[n - 1] == b[m - 1]) {
    n--;
    m--;
    common_suffix_length++;
  }

  int d = _ses(
    a,
    common_prefix_length,
    n - common_prefix_length,
    b,
    common_prefix_length,
    m - common_prefix_length,
    &ctx
  );

  if (d == -1) return -1;

  _edit(&ctx, DIFF_MATCH, n, common_suffix_length);

  if (ses->front().op == 0) ses->clear();
  return d;
}
