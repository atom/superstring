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
#include "text.h"
#include "text-buffer.h"
#include <iostream>

using std::cout;
using std::cerr;
using uint = unsigned int;

class TextBuffer;

class Generator {
  std::default_random_engine engine;
  std::uniform_int_distribution<uint32_t> distribution;

public:
  Generator(uint32_t seed) : engine{seed} {}
  uint32_t operator()() { return distribution(engine); }
};

bool operator==(const Patch::Change &left, const Patch::Change &right);
std::unique_ptr<Text> get_text(const std::u16string content);
std::u16string get_random_string(Generator &, uint32_t character_count = 20);
Text get_random_text(Generator &);
Range get_random_range(Generator &, const Text &);
Range get_random_range(Generator &, TextBuffer &);

namespace std {
  inline std::ostream &operator<<(std::ostream &stream, const std::u16string &text) {
    for (uint16_t character : text) {
      if (character == '\r') {
        stream << "\\\\r";
      } else if (character < 255) {
        stream << static_cast<char>(character);
      } else {
        stream << "\\u";
        stream << character;
      }
    }

    return stream;
  }

  inline std::ostream &operator<<(std::ostream &stream, const TextBuffer::SubsequenceMatch &match) {
    stream << "SubsequenceMatch{ word: " <<  match.word << ", positions: [";

    for (size_t i = 0; i < match.positions.size(); i++) {
      stream << match.positions[i];
      if (i < match.positions.size() - 1) stream << ", ";
    }

    stream << "], match_indices: [";

    for (size_t i = 0; i < match.match_indices.size(); i++) {
      stream << match.match_indices[i];
      if (i < match.match_indices.size() - 1) stream << ", ";
    }

    stream << "], score: " << match.score << " }";

    return stream;
  }
}

template <typename T>
std::ostream &operator<<(std::ostream &stream, const optional<T> &value) {
  if (value) {
    return stream << *value;
  } else {
    return stream << "nullopt";
  }
}

#endif // SUPERSTRING_TEST_HELPERS_H
