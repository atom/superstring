#include "text-diff.h"
#include "libmba-diff.h"
#include "text-slice.h"
#include <vector>
#include <cstring>
#include <ostream>
#include <cassert>

using std::move;
using std::ostream;
using std::vector;

static Point previous_column(Point position) {
  assert(position.column > 0);
  position.column--;
  return position;
}

static int MAX_EDIT_DISTANCE = 4 * 1024;

Patch text_diff(const Text &old_text, const Text &new_text) {
  Patch result;
  Text empty;
  Text cr{u"\r"};
  Text lf{u"\n"};

  vector<diff_edit> edit_script;

  int edit_distance = diff(
    old_text.content.data(),
    old_text.content.size(),
    new_text.content.data(),
    new_text.content.size(),
    MAX_EDIT_DISTANCE,
    &edit_script
  );

  if (edit_distance == -1 || edit_distance >= MAX_EDIT_DISTANCE) {
    result.splice(Point(), old_text.extent(), new_text.extent(), old_text, new_text);
    return result;
  }

  size_t old_offset = 0;
  size_t new_offset = 0;
  Point old_position;
  Point new_position;

  for (struct diff_edit &edit : edit_script) {
    switch (edit.op) {
      case DIFF_MATCH:
        if (edit.len == 0) continue;

        // If the previous change ended between a CR and an LF, then expand
        // that change downward to include the LF.
        if (new_text.at(new_offset) == '\n' &&
            ((old_offset > 0 && old_text.at(old_offset - 1) == '\r') ||
             (new_offset > 0 && new_text.at(new_offset - 1) == '\r'))) {
          result.splice(new_position, Point(1, 0), Point(1, 0), lf, lf);
          old_position.row++;
          old_position.column = 0;
          new_position.row++;
          new_position.column = 0;
        }

        old_offset += edit.len;
        new_offset += edit.len;
        old_position = old_text.position_for_offset(old_offset, 0, false);
        new_position = new_text.position_for_offset(new_offset, 0, false);

        // If the next change starts between a CR and an LF, then expand that
        // change leftward to include the CR.
        if (new_text.at(new_offset - 1) == '\r' &&
            ((old_offset < old_text.size() && old_text.at(old_offset) == '\n') ||
             (new_offset < new_text.size() && new_text.at(new_offset) == '\n'))) {
          result.splice(previous_column(new_position), Point(0, 1), Point(0, 1), cr, cr);
        }
        break;

      case DIFF_DELETE: {
        uint32_t deletion_end = old_offset + edit.len;
        Text deleted_text{old_text.begin() + old_offset, old_text.begin() + deletion_end};
        old_offset = deletion_end;
        Point next_old_position = old_text.position_for_offset(old_offset, 0, false);
        result.splice(new_position, next_old_position.traversal(old_position), Point(), deleted_text, empty);
        old_position = next_old_position;
        break;
      }

      case DIFF_INSERT: {
        uint32_t insertion_end = new_offset + edit.len;
        Text inserted_text{new_text.begin() + new_offset, new_text.begin() + insertion_end};
        new_offset = insertion_end;
        Point next_new_position = new_text.position_for_offset(new_offset, 0, false);
        result.splice(new_position, Point(), next_new_position.traversal(new_position), empty, inserted_text);
        new_position = next_new_position;
        break;
      }
    }
  }

  return result;
}
