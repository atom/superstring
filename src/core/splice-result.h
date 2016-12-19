#include <unordered_set>
#include "marker-id.h"

struct SpliceResult {
  std::unordered_set<MarkerId> touch;
  std::unordered_set<MarkerId> inside;
  std::unordered_set<MarkerId> overlap;
  std::unordered_set<MarkerId> surround;
};
