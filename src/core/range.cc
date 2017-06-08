#include "range.h"

Point Range::extent() const {
  return end.traversal(start);
}
