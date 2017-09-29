#include "range.h"

Range Range::all_inclusive() {
  return Range{Point(), Point::max()};
}

Point Range::extent() const {
  return end.traversal(start);
}
