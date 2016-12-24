#include "range.h"
#include <utility>

Range::Range(Point s, Point e) : start{s}, end{e} {
  if (start > end) std::swap(start, end);
}
