#ifndef SUPERSTRING_TEST_HELPERS_H
#define SUPERSTRING_TEST_HELPERS_H

#include "patch.h"
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

bool operator==(const Patch::Hunk &left, const Patch::Hunk &right) {
  return left.old_start == right.old_start &&
         left.new_start == right.new_start && left.old_end == right.old_end &&
         left.new_end == right.new_end &&
         text_eq(left.old_text, right.old_text) &&
         text_eq(left.new_text, right.new_text);
}

std::unique_ptr<Text> get_text(const vector<u16string> strings) {
  std::unique_ptr<Text> result { new Text };
  result->lines.pop_back();
  for (const u16string &string : strings) {
    result->lines.push_back({ string, LineEnding::LF });
  }
  result->lines.back().ending = LineEnding::NONE;
  return result;
}

#endif // SUPERSTRING_TEST_HELPERS_H
