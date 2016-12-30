#include <chrono>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include "catch.hpp"
#include "point.h"
#include "range.h"
#include "marker-index.h"

using namespace std::chrono;
using std::vector;

Range GetRandomRange() {
  Point start(rand() % 100, rand() % 100);
  Point end = start;
  if (rand() % 10 < 5) {
    end = end.Traverse(Point(rand() % 10, rand() % 10));
  }
  return Range{start, end};
}


TEST_CASE("MarkerIndex::Insert") {
  srand(0);
  MarkerIndex marker_index;
  vector<Range> ranges;
  uint count = 20000;

  for (uint i = 0; i < count; i++) {
    ranges.push_back(GetRandomRange());
  }

  milliseconds start = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
  for (uint i = 0; i < count; i++) {
    marker_index.Insert(i, ranges[i].start, ranges[i].end);
  }
  milliseconds end = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
  std::cout << "Inserting " << (end - start).count();
}
