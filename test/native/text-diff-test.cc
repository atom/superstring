#include "test-helpers.h"
#include "text.h"
#include "text-slice.h"
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

  // We temporarily move the Text's content in order to diff it without
  // copying. Check that the text is unchanged afterwards.
  REQUIRE(old_text == Text(u"abc\nghi\njk\nmno\n"));
  REQUIRE(new_text == Text(u"abc\ndef\nghi\njkl\nmno\n"));
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

TEST_CASE("text_diff - randomized changes") {
  auto t = time(nullptr);
  for (uint i = 0; i < 1000; i++) {
    uint32_t seed = t * 1000 + i;
    Generator rand(seed);
    cout << "seed: " << seed << "\n";

    Text old_text{get_random_string(rand, 10)};
    Text new_text = old_text;

    // cout << "extent: " << new_text.extent() << " text:\n" << new_text << "\n\n";

    for (uint j = 0; j < 1 + rand() % 10; j++) {
      printf("j: %u\n", j);

      Range deleted_range = get_random_range(rand, new_text);
      Text inserted_text{get_random_string(rand, 3)};

      new_text.splice(deleted_range.start, deleted_range.extent(), inserted_text);

      // cout << "replace " << deleted_range << " with " << inserted_text << "\n\n";
      // cout << "extent: " << new_text.extent() << " text:\n" << new_text << "\n\n";
    }

    Patch patch = text_diff(old_text, new_text);

    for (const Change &change : patch.get_changes()) {
      old_text.splice(
        change.new_start,
        change.old_end.traversal(change.old_start),
        TextSlice(new_text).slice(Range{change.new_start, change.new_end})
      );
    }

    REQUIRE(old_text == new_text);
  }
}