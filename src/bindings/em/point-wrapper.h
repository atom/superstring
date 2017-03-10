#pragma once

#include <algorithm>
#include <limits>
#include "point.h"

struct PointWrapper {
  double row;
  double column;

  PointWrapper(void) : row(0), column(0) {}
  PointWrapper(Point const & point) : row(point.row), column(point.column) {}

  operator Point(void) const {
    unsigned row = std::min(this->row, static_cast<double>(std::numeric_limits<unsigned>::max()));
    unsigned column = std::min(this->column, static_cast<double>(std::numeric_limits<unsigned>::max()));
    return Point(row, column);
  }
};
