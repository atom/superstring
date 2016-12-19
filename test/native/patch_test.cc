#include "test_helpers.h"

TEST_CASE("Records simple non-overlapping splices") {
  Patch patch;

  patch.Splice(Point{0, 5}, Point{0, 3}, Point{0, 4}, nullptr, nullptr);
  patch.Splice(Point{0, 10}, Point{0, 3}, Point{0, 4}, nullptr, nullptr);
  REQUIRE(patch.GetHunks() == vector<Hunk>({
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

  patch.Splice(Point{0, 2}, Point{0, 2}, Point{0, 1}, nullptr, nullptr);
  REQUIRE(patch.GetHunks() == vector<Hunk>({
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

  patch.Splice(Point{0, 0}, Point{0, 0}, Point{0, 10}, nullptr, nullptr);
  REQUIRE(patch.GetHunks() == vector<Hunk>({
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

  patch.Splice(
    Point{0, 5},
    Point{0, 3},
    Point{0, 4},
    GetText("abc"),
    GetText("1234")
  );
  REQUIRE(patch.GetHunks() == vector<Hunk>({
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 5}, Point{0, 9},
      GetText("abc").get(),
      GetText("1234").get()
    },
  }));

  // overlaps lower bound, has no upper bound.
  patch.Splice(
    Point{0, 7},
    Point{0, 3},
    Point{0, 4},
    GetText("34d"),
    GetText("5678")
  );
  REQUIRE(patch.GetHunks() == vector<Hunk>({
    Hunk{
      Point{0, 5}, Point{0, 9},
      Point{0, 5}, Point{0, 11},
      GetText("abcd").get(),
      GetText("125678").get()
    },
  }));

  // overlaps upper bound, has no lower bound.
  patch.Splice(
    Point{0, 3},
    Point{0, 3},
    Point{0, 4},
    GetText("efa"),
    GetText("1234")
  );
  REQUIRE(patch.GetHunks() == vector<Hunk>({
    Hunk{
      Point{0, 3}, Point{0, 9},
      Point{0, 3}, Point{0, 12},
      GetText("efabcd").get(),
      GetText("123425678").get()
    },
  }));

  // doesn't overlap lower bound, has no upper bound
  patch.Splice(
    Point{0, 15},
    Point{0, 3},
    Point{0, 4},
    GetText("ghi"),
    GetText("5678")
  );
  REQUIRE(patch.GetHunks() == vector<Hunk>({
    Hunk{
      Point{0, 3}, Point{0, 9},
      Point{0, 3}, Point{0, 12},
      GetText("efabcd").get(),
      GetText("123425678").get()
    },
    Hunk{
      Point{0, 12}, Point{0, 15},
      Point{0, 15}, Point{0, 19},
      GetText("ghi").get(),
      GetText("5678").get()
    },
  }));

  // surrounds two hunks, has no lower or upper bound
  patch.Splice(
    Point{0, 1},
    Point{0, 21},
    Point{0, 5},
    GetText("xx123425678yyy5678zzz"),
    GetText("99999")
  );
  REQUIRE(patch.GetHunks() == vector<Hunk>({
    Hunk{
      Point{0, 1}, Point{0, 18},
      Point{0, 1}, Point{0, 6},
      GetText("xxefabcdyyyghizzz").get(),
      GetText("99999").get()
    }
  }));
}

TEST_CASE("Serializes and deserializes") {
  Patch patch;

  patch.Splice(Point{0, 5}, Point{0, 3}, Point{0, 4}, nullptr, nullptr);
  patch.Splice(Point{0, 10}, Point{0, 3}, Point{0, 4}, nullptr, nullptr);
  patch.Splice(Point{0, 2}, Point{0, 2}, Point{0, 1}, nullptr, nullptr);
  patch.Splice(Point{0, 0}, Point{0, 0}, Point{0, 10}, nullptr, nullptr);
  patch.HunkForOldPosition(Point{0, 5}); // splay the middle
  REQUIRE(patch.GetHunks() == vector<Hunk>({
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
  patch.Serialize(&serialization_vector);
  Patch patch_copy(serialization_vector);
  REQUIRE(patch_copy.GetHunks() == vector<Hunk>({
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

  REQUIRE(patch_copy.Splice(Point{0, 1}, Point{0, 1}, Point{0, 2}, nullptr, nullptr) == false);
}
