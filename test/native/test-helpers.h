#ifndef SUPERSTRING_TEST_HELPERS_H
#define SUPERSTRING_TEST_HELPERS_H

#include "patch.h"
#include <catch.hpp>
#include <cstring>
#include <memory>
#include <ostream>
#include <vector>
#include <random>
#include "range.h"

using std::vector;
using std::u16string;

class TextBuffer;

bool operator==(const Patch::Change &left, const Patch::Change &right);
std::unique_ptr<Text> get_text(const u16string content);
std::u16string get_random_string();
Text get_random_text();
Range get_random_range(const Text &);
Range get_random_range(TextBuffer &);

class Generator {
  std::default_random_engine engine;
  std::uniform_int_distribution<uint32_t> distribution;

public:
  Generator(uint32_t seed) : engine{seed} {}
  uint32_t operator()() { return distribution(engine); }
};

#endif // SUPERSTRING_TEST_HELPERS_H
