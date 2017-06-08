#ifndef SUPERSTRING_TEXT_DIFF_H
#define SUPERSTRING_TEXT_DIFF_H

#include "patch.h"
#include "text.h"

Patch text_diff(const Text &old_text, const Text &new_text);

#endif  // SUPERSTRING_TEXT_DIFF_H