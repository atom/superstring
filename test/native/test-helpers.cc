#ifndef SUPERSTRING_TEST_HELPERS_H
#define SUPERSTRING_TEST_HELPERS_H

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

Text get_random_text() {
  uint count = rand() % 20;
  vector<uint16_t> content;
  content.reserve(count);
  for (uint i = 0; i < count; i++) {
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
  return Text {move(content)};
}

Range get_random_range(TextBuffer &buffer) {
  uint32_t start_row = rand() % (buffer.extent().row + 1);
  uint32_t max_column = buffer.line_length_for_row(start_row);
  uint32_t start_column = 0;
  if (max_column > 0) start_column = rand() % max_column;
  Point start {start_row, start_column};
  Point end {start};
  while (rand() % 10 < 2) {
    end = buffer.clip_position(end.traverse(Point(rand() % 3, rand() % 10)));
  }
  return {start, end};
}

#endif // SUPERSTRING_TEST_HELPERS_H
