#include "test-helpers.h"
#include "text.h"
#include "text-diff.h"

using Change = Patch::Change;
using std::vector;

TEST_CASE("text_diff - multiple lines") {
  Text old_text{u"abc\nghi\njk\nmno\n"};
  Text new_text{u"abc\ndef\nghi\njkl\nmno\n"};

  Patch patch = text_diff(old_text, new_text);

  REQUIRE(patch.get_changes() == vector<Change>({
    Change{
      Point{1, 0}, Point{1, 0},
      Point{1, 0}, Point{2, 0},
      nullptr, nullptr, 0, 0
    },
    Change{
      Point{2, 2}, Point{2, 2},
      Point{3, 2}, Point{3, 3},
      nullptr, nullptr, 0, 0
    }
  }));
}

TEST_CASE("text_diff - single line") {
  Text old_text{u"abcdefghij"};
  Text new_text{u"abcxyefij"};

  Patch patch = text_diff(old_text, new_text);

  REQUIRE(patch.get_changes() == vector<Change>({
    Change{
      Point{0, 3}, Point{0, 4},
      Point{0, 3}, Point{0, 5},
      nullptr, nullptr, 0, 0
    },
    Change{
      Point{0, 6}, Point{0, 8},
      Point{0, 7}, Point{0, 7},
      nullptr, nullptr, 0, 0
    },
  }));
}