#include "test-helpers.h"
#include "patch.h"
#include "range.h"
#include "text-buffer.h"
#include <catch.hpp>
#include <cstring>
#include <memory>
#include <ostream>
#include <vector>

using std::vector;
using std::u16string;

bool text_eq(const Text *left, const Text *right) {
  if (left == right)
    return true;
  if (!left && right)
    return false;
  if (left && !right)
    return false;
  return *left == *right;
}

bool operator==(const Patch::Change &left, const Patch::Change &right) {
  return left.old_start == right.old_start &&
         left.new_start == right.new_start && left.old_end == right.old_end &&
         left.new_end == right.new_end &&
         text_eq(left.old_text, right.old_text) &&
         text_eq(left.new_text, right.new_text);
}

std::unique_ptr<Text> get_text(const u16string content) {
  return std::unique_ptr<Text> { new Text(content) };
}

std::u16string get_random_string(Generator &rand, uint32_t character_count) {
  u16string content;
  content.reserve(character_count);
  for (uint i = 0; i < character_count; i++) {
    if (rand() % 20 < 1) {
      content.push_back('\n');
    } else if (rand() % 20 < 1) {
      content.push_back('\r');
      content.push_back('\n');
      i++;
    } else if (rand() % 20 < 1) {
      content.push_back('\r');
    } else {
      uint16_t character = 'a' + (rand() % 26);
      content.push_back(character);
    }
  }
  return content;
}

Text get_random_text(Generator &rand) {
  return Text {get_random_string(rand)};
}

Range get_random_range(Generator &rand, const Text &text) {
  uint32_t start_row = rand() % (text.extent().row + 1);
  uint32_t max_column = text.line_length_for_row(start_row);
  uint32_t start_column = 0;
  if (max_column > 0) start_column = rand() % max_column;
  Point start {start_row, start_column};
  Point end {start};
  while (rand() % 10 < 3) {
    end = text.clip_position(end.traverse(Point(rand() % 2, rand() % 10))).position;
  }
  return {start, end};
}

Range get_random_range(Generator &rand, TextBuffer &buffer) {
  return get_random_range(rand, buffer.text());
}
