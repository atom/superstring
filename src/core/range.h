#ifndef RANGE_H_
#define RANGE_H_

#include "point.h"

struct Range {
  Point start;
  Point end;

  Point extent() const;
};

#endif // RANGE_H_
