#include "auto-wrap.h"
#include "point-wrapper.h"
#include "point.h"
#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(Point) {
  emscripten::value_object<PointWrapper>("Point")
    .field("row", WRAP_FIELD(PointWrapper, row))
    .field("column", WRAP_FIELD(PointWrapper, column));
}
