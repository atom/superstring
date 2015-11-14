#include <set>
#include "marker-id.h"

struct SpliceResult {
  std::set<MarkerId> touch;
  std::set<MarkerId> inside;
  std::set<MarkerId> overlap;
  std::set<MarkerId> surround;
};
