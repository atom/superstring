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
  Text *old_text;
  Text *new_text;
};

inline std::ostream &operator<<(std::ostream &stream, const Text *text) {
  if (text) {
    stream << "'";
    for (uint32_t i = 0; i < text->length; i++) {
      if (text->content[i] < CHAR_MAX) {
        stream << (char)text->content[i];
      } else {
        stream << "\\u" << text->content[i];
      }
    }
    stream << "'";
    return stream;
  } else {
    return stream << "null";
  }
}

inline std::ostream &operator<<(std::ostream &stream, const Hunk &hunk) {
  return stream <<
    "{Hunk old: (" << hunk.old_start << " - " << hunk.old_end << "): " << hunk.old_text <<
    ", new: (" << hunk.new_start << " - " << hunk.new_end << "): " << hunk.new_text << "}";
}

#endif // HUNK_H_
