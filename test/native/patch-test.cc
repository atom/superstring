#include "test-helpers.h"

typedef Patch::Hunk Hunk;

TEST_CASE("Records simple non-overlapping splices") {
  Patch patch;

  patch.splice(Point{0, 5}, Point{0, 3}, Point{0, 4}, nullptr, nullptr);
  patch.splice(Point{0, 10}, Point{0, 3}, Point{0, 4}, nullptr, nullptr);
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

  patch.splice(Point{0, 2}, Point{0, 2}, Point{0, 1}, nullptr, nullptr);
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

  patch.splice(Point{0, 0}, Point{0, 0}, Point{0, 10}, nullptr, nullptr);
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
    Point{0, 5},
    Point{0, 3},
    Point{0, 4},
    get_text("abc"),
    get_text("1234")
  );
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 5}, Point{0, 9},
      get_text("abc").get(),
      get_text("1234").get()
    },
  }));

  // overlaps lower bound, has no upper bound.
  patch.splice(
    Point{0, 7},
    Point{0, 3},
    Point{0, 4},
    get_text("34d"),
    get_text("5678")
  );
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 5}, Point{0, 9},
      Point{0, 5}, Point{0, 11},
      get_text("abcd").get(),
      get_text("125678").get()
    },
  }));

  // overlaps upper bound, has no lower bound.
  patch.splice(
    Point{0, 3},
    Point{0, 3},
    Point{0, 4},
    get_text("efa"),
    get_text("1234")
  );
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    {
      Point{0, 3}, Point{0, 9},
      Point{0, 3}, Point{0, 12},
      get_text("efabcd").get(),
      get_text("123425678").get()
    },
  }));

  // doesn't overlap lower bound, has no upper bound
  patch.splice(
    Point{0, 15},
    Point{0, 3},
    Point{0, 4},
    get_text("ghi"),
    get_text("5678")
  );
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 3}, Point{0, 9},
      Point{0, 3}, Point{0, 12},
      get_text("efabcd").get(),
      get_text("123425678").get()
    },
    Hunk{
      Point{0, 12}, Point{0, 15},
      Point{0, 15}, Point{0, 19},
      get_text("ghi").get(),
      get_text("5678").get()
    },
  }));

  // surrounds two hunks, has no lower or upper bound
  patch.splice(
    Point{0, 1},
    Point{0, 21},
    Point{0, 5},
    get_text("xx123425678yyy5678zzz"),
    get_text("99999")
  );
  REQUIRE(patch.get_hunks() == vector<Hunk>({
    Hunk{
      Point{0, 1}, Point{0, 18},
      Point{0, 1}, Point{0, 6},
      get_text("xxefabcdyyyghizzz").get(),
      get_text("99999").get()
    }
  }));
}

TEST_CASE("Serializes and deserializes") {
  Patch patch;

  patch.splice(Point{0, 5}, Point{0, 3}, Point{0, 4}, nullptr, nullptr);
  patch.splice(Point{0, 10}, Point{0, 3}, Point{0, 4}, nullptr, nullptr);
  patch.splice(Point{0, 2}, Point{0, 2}, Point{0, 1}, nullptr, nullptr);
  patch.splice(Point{0, 0}, Point{0, 0}, Point{0, 10}, nullptr, nullptr);
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

  vector<uint8_t> serialization_vector;
  patch.serialize(&serialization_vector);
  Patch patch_copy(serialization_vector);
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

  REQUIRE(patch_copy.splice(Point{0, 1}, Point{0, 1}, Point{0, 2}, nullptr, nullptr) == false);
}
