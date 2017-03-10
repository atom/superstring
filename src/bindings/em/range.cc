#include "auto-wrap.h"
#include "range.h"
#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(Range) {
  emscripten::value_object<Range>("Range")
    .field("start", WRAP_FIELD(Range, start))
    .field("end", WRAP_FIELD(Range, end));
}
