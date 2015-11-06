#include <climits>
#include "point.h"

Point Point::Max() {
  return Point(UINT_MAX, UINT_MAX);
}

Point::Point() : Point(0, 0) {}

Point::Point(unsigned row, unsigned column) : row {row}, column {column} {}

int Point::Compare(const Point &other) const {
  if (row < other.row) return -1;
  if (row > other.row) return 1;
  if (column < other.column) return -1;
  if (column > other.column) return 1;
  return 0;
}

bool Point::IsZero() const {
  return row == 0 && column == 0;
}

Point Point::Traverse(const Point &traversal) const {
  if (traversal.row == 0) {
    return Point(row , column + traversal.column);
  } else {
    return Point(row + traversal.row, traversal.column);
  }
}

Point Point::Traversal(const Point &start) const {
  if (row == start.row) {
    return Point(0, column - start.column);
  } else {
    return Point(row - start.row, column);
  }
}

bool Point::operator==(const Point &other) const {
  return Compare(other) == 0;
}

bool Point::operator<(const Point &other) const {
  return Compare(other) < 0;
}

bool Point::operator<=(const Point &other) const {
  return Compare(other) <= 0;
}

bool Point::operator>(const Point &other) const {
  return Compare(other) > 0;
}

bool Point::operator>=(const Point &other) const {
  return Compare(other) >= 0;
}
