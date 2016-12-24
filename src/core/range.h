#ifndef RANGE_H_
#define RANGE_H_

#include "point.h"

struct Range {
  Point start;
  Point end;

  Range(Point start, Point end);
};

#endif // RANGE_H_
