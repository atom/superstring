#include "text-diff.h"
#include "diff_match_patch.h"
#include <vector>
#include <string.h>
#include <ostream>
#include <cassert>

using std::ostream;
using std::vector;
using std::u16string;

struct U16StringTraits {
  static bool is_alnum(char16_t c) { return std::iswalnum(c); }
  static bool is_digit(char16_t c) { return std::iswdigit(c); }
  static bool is_space(char16_t c) { return std::iswspace(c); }
  static wchar_t to_wchar(char16_t c) { return c; }
  static char16_t from_wchar(wchar_t c) { return c; }
  static u16string cs(const wchar_t* s) { return u16string(s, s + std::wcslen(s)); }
  static const char16_t eol = '\n';
  static const char16_t tab = '\t';

  // This is not used when computing diffs; the diff-match-patch library
  // uses it for other functionality like parsing textual diffs.
  static int to_int(const char16_t* s) { assert(false); return 0; }
};

using DiffBuilder = diff_match_patch<u16string, U16StringTraits>;

Patch text_diff(const Text &old_text, const Text &new_text) {
  Patch result;

  // TODO - We should be able to avoid copying here by creating an adapter
  // class that implements the basic_string interface but is backed by a
  // vector.
  u16string old_string(old_text.content.begin(), old_text.content.end());
  u16string new_string(new_text.content.begin(), new_text.content.end());

  DiffBuilder diff_builder;
  diff_builder.Diff_Timeout = -1;
  DiffBuilder::Diffs diffs = diff_builder.diff_main(old_string, new_string);

  size_t old_offset = 0;
  size_t new_offset = 0;
  Point old_position;
  Point new_position;

  for (const DiffBuilder::Diff &diff : diffs) {
    size_t length = diff.text.size();
    switch (diff.operation) {
      case DiffBuilder::Operation::EQUAL: {
        old_offset += length;
        new_offset += length;
        old_position = old_text.position_for_offset(old_offset);
        new_position = new_text.position_for_offset(new_offset);
        break;
      }

      case DiffBuilder::Operation::DELETE: {
        old_offset += length;
        Point next_old_position = old_text.position_for_offset(old_offset);
        result.splice(new_position, next_old_position.traversal(old_position), Point());
        old_position = next_old_position;
        break;
      }

      case DiffBuilder::Operation::INSERT: {
        new_offset += length;
        Point next_new_position = new_text.position_for_offset(new_offset);
        result.splice(new_position, Point(), next_new_position.traversal(new_position));
        new_position = next_new_position;
        break;
      }
    }
  }

  return result;
}
