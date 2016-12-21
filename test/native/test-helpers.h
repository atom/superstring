#ifndef SUPERSTRING_TEST_HELPERS_H
#define SUPERSTRING_TEST_HELPERS_H

#include "patch.h"
#include <catch.hpp>
#include <memory>
#include <ostream>
#include <vector>

using std::vector;

bool text_eq(const Text *left, const Text *right) {
  if (left == right)
    return true;
  if (!left && right)
    return false;
  if (left && !right)
    return false;
  return *left == *right;
}

bool operator==(const Hunk &left, const Hunk &right) {
  return left.old_start == right.old_start &&
         left.new_start == right.new_start && left.old_end == right.old_end &&
         left.new_end == right.new_end &&
         text_eq(left.old_text, right.old_text) &&
         text_eq(left.new_text, right.new_text);
}

std::unique_ptr<Text> GetText(const char *string) {
  size_t length = strlen(string);
  vector<uint16_t> content;
  content.reserve(length);
  for (size_t i = 0; i < length; i++) {
    content.push_back(static_cast<uint16_t>(string[i]));
  }
  return std::unique_ptr<Text>(new Text(move(content)));
}

#endif // SUPERSTRING_TEST_HELPERS_H
