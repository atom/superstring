#include <random>

class MarkerIndex {
 public:
  MarkerIndex(unsigned seed);
  int GenerateRandomNumber();
 private:
  std::default_random_engine random_engine;
  std::uniform_int_distribution<int> random_distribution;
};
