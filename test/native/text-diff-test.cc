#include "test-helpers.h"
#include "text.h"
#include "text-diff.h"

using Change = Patch::Change;
using std::vector;

TEST_CASE("text_diff - basic") {
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