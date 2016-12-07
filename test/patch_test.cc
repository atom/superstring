#include "helpers/test_helper.h"

TEST_CASE("Records simple non-overlapping splices") {
  Patch patch;

  patch.Splice(Point{0, 5}, Point{0, 3}, Point{0, 4}, nullptr);
  patch.Splice(Point{0, 10}, Point{0, 3}, Point{0, 4}, nullptr);
  REQUIRE(patch.GetHunks() == vector<Hunk>({
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 5}, Point{0, 9},
    },
    Hunk{
      Point{0, 9}, Point{0, 12},
      Point{0, 10}, Point{0, 14},
    }
  }));

  patch.Splice(Point{0, 2}, Point{0, 2}, Point{0, 1}, nullptr);
  REQUIRE(patch.GetHunks() == vector<Hunk>({
    Hunk{
      Point{0, 2}, Point{0, 4},
      Point{0, 2}, Point{0, 3}
    },
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 4}, Point{0, 8},
    },
    Hunk{
      Point{0, 9}, Point{0, 12},
      Point{0, 9}, Point{0, 13},
    }
  }));

  patch.Splice(Point{0, 0}, Point{0, 0}, Point{0, 10}, nullptr);
  REQUIRE(patch.GetHunks() == vector<Hunk>({
    Hunk{
      Point{0, 0}, Point{0, 0},
      Point{0, 0}, Point{0, 10}
    },
    Hunk{
      Point{0, 2}, Point{0, 4},
      Point{0, 12}, Point{0, 13}
    },
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 14}, Point{0, 18},
    },
    Hunk{
      Point{0, 9}, Point{0, 12},
      Point{0, 19}, Point{0, 23},
    }
  }));
}

TEST_CASE("Serializes and deserializes") {
  Patch patch;

  patch.Splice(Point{0, 5}, Point{0, 3}, Point{0, 4}, nullptr);
  patch.Splice(Point{0, 10}, Point{0, 3}, Point{0, 4}, nullptr);
  patch.Splice(Point{0, 2}, Point{0, 2}, Point{0, 1}, nullptr);
  patch.Splice(Point{0, 0}, Point{0, 0}, Point{0, 10}, nullptr);
  patch.HunkForOldPosition(Point{0, 5}); // splay the middle
  REQUIRE(patch.GetHunks() == vector<Hunk>({
    Hunk{
      Point{0, 0}, Point{0, 0},
      Point{0, 0}, Point{0, 10}
    },
    Hunk{
      Point{0, 2}, Point{0, 4},
      Point{0, 12}, Point{0, 13}
    },
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 14}, Point{0, 18},
    },
    Hunk{
      Point{0, 9}, Point{0, 12},
      Point{0, 19}, Point{0, 23},
    }
  }));

  vector<uint8_t> serialization_vector;
  patch.Serialize(&serialization_vector);
  Patch patch_copy(serialization_vector);
  REQUIRE(patch_copy.GetHunks() == vector<Hunk>({
    Hunk{
      Point{0, 0}, Point{0, 0},
      Point{0, 0}, Point{0, 10}
    },
    Hunk{
      Point{0, 2}, Point{0, 4},
      Point{0, 12}, Point{0, 13}
    },
    Hunk{
      Point{0, 5}, Point{0, 8},
      Point{0, 14}, Point{0, 18},
    },
    Hunk{
      Point{0, 9}, Point{0, 12},
      Point{0, 19}, Point{0, 23},
    }
  }));

  REQUIRE(patch_copy.Splice(Point{0, 1}, Point{0, 1}, Point{0, 2}, nullptr) == false);
}
