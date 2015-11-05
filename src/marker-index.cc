#include "marker-index.h"
#include <climits>
#include <random>
#include <stdlib.h>

using std::default_random_engine;

MarkerIndex::MarkerIndex(unsigned seed)
  : random_engine{static_cast<default_random_engine::result_type>(seed)},
    random_distribution{0, INT_MAX} {}

int MarkerIndex::GenerateRandomNumber() {
  return random_distribution(random_engine);
}
