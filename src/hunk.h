#ifndef HUNK_H_
#define HUNK_H_

#include "point.h"

class Hunk {
public:
  Point old_start;
  Point new_start;
  Point old_end;
  Point new_end;
};

#endif // HUNK_H_
