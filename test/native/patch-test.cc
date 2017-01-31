#include "test-helpers.h"

typedef Patch::Hunk Hunk;

static optional<Text> null_text;

TEST_CASE("Records simple non-overlapping splices") {
  Patch patch;

  patch.splice(Point{0, 5}, Point{0, 3}, Point{0, 4});
  patch.splice(Point{0, 10}, Point{0, 3}, Point{0, 4});
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 5}, Point{0, 9},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 9}, Point{0, 12},
      Point{0, 10}, Point{0, 14},
      nullptr, nullptr
    }
  }));

  patch.splice(Point{0, 2}, Point{0, 2}, Point{0, 1});
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 2}, Point{0, 4},
      Point{0, 2}, Point{0, 3},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 4}, Point{0, 8},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 9}, Point{0, 12},
      Point{0, 9}, Point{0, 13},
      nullptr, nullptr
    }
  }));

  patch.splice(Point{0, 0}, Point{0, 0}, Point{0, 10});
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 0}, Point{0, 0},
      Point{0, 0}, Point{0, 10},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 2}, Point{0, 4},
      Point{0, 12}, Point{0, 13},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 14}, Point{0, 18},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 9}, Point{0, 12},
      Point{0, 19}, Point{0, 23},
      nullptr, nullptr
    }
  }));
}

TEST_CASE("Records overlapping splices with text") {
  Patch patch;

  patch.splice(
    Point {0, 5},
    Point {0, 3},
    Point {0, 4},
    Text {u"abc"},
    Text {u"1234"}
  );
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 5}, Point{0, 9},
      get_text(u"abc").get(),
      get_text(u"1234").get()
    },
  }));

  // overlaps lower bound, has no upper bound.
  patch.splice(
    Point {0, 7},
    Point {0, 3},
    Point {0, 4},
    Text {u"34d"},
    Text {u"5678"}
  );
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 5}, Point{0, 9},
      Point{0, 5}, Point{0, 11},
      get_text(u"abcd").get(),
      get_text(u"125678").get()
    },
  }));

  // overlaps upper bound, has no lower bound.
  patch.splice(
    Point {0, 3},
    Point {0, 3},
    Point {0, 4},
    Text {u"efa"},
    Text {u"1234"}
  );
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    {
      Point{0, 3}, Point{0, 9},
      Point{0, 3}, Point{0, 12},
      get_text(u"efabcd").get(),
      get_text(u"123425678").get()
    },
  }));

  // doesn't overlap lower bound, has no upper bound
  patch.splice(
    Point{0, 15},
    Point{0, 3},
    Point{0, 4},
    Text {u"ghi"},
    Text {u"5678"}
  );
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 3}, Point{0, 9},
      Point{0, 3}, Point{0, 12},
      get_text(u"efabcd").get(),
      get_text(u"123425678").get()
    },
    Hunk{
      Point{0, 12}, Point{0, 15},
      Point{0, 15}, Point{0, 19},
      get_text(u"ghi").get(),
      get_text(u"5678").get()
    },
  }));

  // surrounds two hunks, has no lower or upper bound
  patch.splice(
    Point{0, 1},
    Point{0, 21},
    Point{0, 5},
    Text {u"xx123425678yyy5678zzz"},
    Text {u"99999"}
  );
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 1}, Point{0, 18},
      Point{0, 1}, Point{0, 6},
      get_text(u"xxefabcdyyyghizzz").get(),
      get_text(u"99999").get()
    }
  }));
}

TEST_CASE("Serializes and deserializes") {
  Patch patch;

  patch.splice(Point{0, 5}, Point{0, 3}, Point{0, 4});
  patch.splice(Point{0, 10}, Point{0, 3}, Point{0, 4});
  patch.splice(Point{0, 2}, Point{0, 2}, Point{0, 1});
  patch.splice(Point{0, 0}, Point{0, 0}, Point{0, 10});
  patch.hunk_for_old_position(Point{0, 5}); // splay the middle
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 0}, Point{0, 0},
      Point{0, 0}, Point{0, 10},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 2}, Point{0, 4},
      Point{0, 12}, Point{0, 13},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 14}, Point{0, 18},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 9}, Point{0, 12},
      Point{0, 19}, Point{0, 23},
      nullptr, nullptr
    }
  }));

  Serializer serializer;
  patch.serialize(serializer);
  Patch patch_copy(serializer);
  REQUIRE(patch_copy.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 0}, Point{0, 0},
      Point{0, 0}, Point{0, 10},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 2}, Point{0, 4},
      Point{0, 12}, Point{0, 13},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 14}, Point{0, 18},
      nullptr, nullptr
    },
    Hunk{
      Point{0, 9}, Point{0, 12},
      Point{0, 19}, Point{0, 23},
      nullptr, nullptr
    }
  }));

  REQUIRE(patch_copy.splice(Point{0, 1}, Point{0, 1}, Point{0, 2}) == false);
}
