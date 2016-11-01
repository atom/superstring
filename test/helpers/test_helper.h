#include <catch.hpp>
#include "patch.h"
#include <ostream>

using std::vector;

bool operator==(const Hunk&left, const Hunk &right) {
  return
    left.old_start == right.old_start &&
    left.new_start == right.new_start &&
    left.old_end == right.old_end &&
    left.new_end == right.new_end;
}
