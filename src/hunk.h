#ifndef HUNK_H_
#define HUNK_H_

#include <ostream>
#include "point.h"
#include "text.h"

class Hunk {
public:
  Point old_start;
  Point old_end;
  Point new_start;
  Point new_end;
  Text *new_text;
};

inline std::ostream &operator<<(std::ostream &stream, const Hunk &hunk) {
  return stream <<
    "{Hunk old: " << hunk.old_start << " - " << hunk.old_end <<
    ", new: " << hunk.new_start << " - " << hunk.new_end << "}";
}

#endif // HUNK_H_
