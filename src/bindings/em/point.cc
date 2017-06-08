#include "point.h"
#include <emscripten/bind.h>
#include <algorithm>
#include <limits>

double get_row(const Point &point) {
  return point.row;
}

void set_row(Point &point, double row) {
  if (row < 0) {
    point.row = 0;
  } else {
    point.row = std::min(
      row,
      static_cast<double>(std::numeric_limits<unsigned>::max())
    );
  }
}

double get_column(const Point &point) {
  return point.column;
}

void set_column(Point &point, double column) {
  if (column < 0) {
    point.column = 0;
  } else {
    point.column = std::min(
      column,
      static_cast<double>(std::numeric_limits<unsigned>::max())
    );
  }
}

EMSCRIPTEN_BINDINGS(Point) {
  emscripten::value_object<Point>("Point")
    .field("row", &get_row, &set_row)
    .field("column", &get_column, &set_column);
}
