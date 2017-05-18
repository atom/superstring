#include "text-diff.h"
#include "diff_match_patch.h"
#include "text-slice.h"
#include <string.h>
#include <string>
#include <cassert>

using std::move;
using std::u16string;

struct StringAdapterTraits {
  static bool is_alnum(char16_t c) { return std::iswalnum(c); }
  static bool is_digit(char16_t c) { return std::iswdigit(c); }
  static bool is_space(char16_t c) { return std::iswspace(c); }
  static wchar_t to_wchar(char16_t c) { return c; }
  static char16_t from_wchar(wchar_t c) { return c; }
  static const char16_t eol = '\n';
  static const char16_t tab = '\t';

  // These functions are not used when computing diffs; the diff-match-patch
  // library uses them in functions that we don't call.
  static u16string cs(const wchar_t* s) { assert(false); return u16string(); }
  static int to_int(const char16_t* s) { assert(false); return 0; }
};

using DiffBuilder = diff_match_patch<u16string, StringAdapterTraits>;

static Point previous_column(Point position) {
  assert(position.column > 0);
  position.column--;
  return position;
}

Patch text_diff(const Text &old_text, const Text &new_text) {
  Patch result;

  u16string old_string(old_text.content.begin(), old_text.content.end());
  u16string new_string(new_text.content.begin(), new_text.content.end());

  DiffBuilder diff_builder;
  diff_builder.Diff_Timeout = 5;
  DiffBuilder::Diffs diffs = diff_builder.diff_main(old_string, new_string);

  size_t old_offset = 0;
  size_t new_offset = 0;
  Point old_position;
  Point new_position;

  Text empty;
  Text cr{u"\r"};
  Text lf{u"\n"};

  for (DiffBuilder::Diff &diff : diffs) {
    switch (diff.operation) {
      case DiffBuilder::Operation::EQUAL:

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

        old_offset += diff.text.size();
        new_offset += diff.text.size();
        old_position = old_text.position_for_offset(old_offset, false);
        new_position = new_text.position_for_offset(new_offset, false);

        // If the next change starts between a CR and an LF, then expand that
        // change leftward to include the CR.
        if (new_text.at(new_offset - 1) == '\r' &&
            ((old_offset < old_text.size() && old_text.at(old_offset) == '\n') ||
             (new_offset < new_text.size() && new_text.at(new_offset) == '\n'))) {
          result.splice(previous_column(new_position), Point(0, 1), Point(0, 1), cr, cr);
        }
        break;

      case DiffBuilder::Operation::DELETE: {
        Text deleted_text{move(diff.text)};
        old_offset += diff.text.size();
        Point next_old_position = old_text.position_for_offset(old_offset, false);
        result.splice(new_position, next_old_position.traversal(old_position), Point(), deleted_text, empty);
        old_position = next_old_position;
        break;
      }

      case DiffBuilder::Operation::INSERT: {
        Text inserted_text{move(diff.text)};
        new_offset += diff.text.size();
        Point next_new_position = new_text.position_for_offset(new_offset, false);
        result.splice(new_position, Point(), next_new_position.traversal(new_position), empty, inserted_text);
        new_position = next_new_position;
        break;
      }
    }
  }

  return result;
}
