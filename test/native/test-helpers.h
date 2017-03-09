#ifndef SUPERSTRING_TEST_HELPERS_H
#define SUPERSTRING_TEST_HELPERS_H

#include "patch.h"
#include <catch.hpp>
#include <cstring>
#include <memory>
#include <ostream>
#include <vector>
#include "range.h"

using std::vector;
using std::u16string;

class TextBuffer;

bool operator==(const Patch::Change &left, const Patch::Change &right);
std::unique_ptr<Text> get_text(const u16string content);
std::u16string get_random_string();
Text get_random_text();
Range get_random_range(TextBuffer &buffer);

#endif // SUPERSTRING_TEST_HELPERS_H
