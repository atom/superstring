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

std::ostream &operator<<(std::ostream &stream, const Point &point) {
  return stream << "(" << point.row << ", " << point.column << ")";
}

std::ostream &operator<<(std::ostream &stream, const Hunk &hunk) {
  return stream <<
    "{Hunk old: " << hunk.old_start << " - " << hunk.old_end <<
    ", new: " << hunk.new_start << " - " << hunk.new_end << "}";
}
