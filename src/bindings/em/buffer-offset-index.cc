#include "auto-wrap.h"
#include "buffer-offset-index.h"
#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(BufferOffsetIndex) {
  emscripten::class_<BufferOffsetIndex>("BufferOffsetIndex")
    .constructor<>()
    .function("splice", WRAP(&BufferOffsetIndex::splice))
    .function("characterIndexForPosition", WRAP(&BufferOffsetIndex::character_index_for_position))
    .function("positionForCharacterIndex", WRAP(&BufferOffsetIndex::position_for_character_index));
}
