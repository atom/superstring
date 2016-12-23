#include <vector>
#include "catch.hpp"
#include "marker-index.h"

using std::vector;

TEST_CASE("MarkerIndex::Insert") {
  MarkerIndex marker_index;
  marker_index.Insert(1, {0, 1}, {2, 3});


}
