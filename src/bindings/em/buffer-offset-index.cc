#include "auto-wrap.h"
#include "buffer-offset-index.h"

#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(BufferOffsetIndex) {

    emscripten::class_<BufferOffsetIndex>("BufferOffsetIndex")

        .constructor<>()

        .function("splice", WRAP(&BufferOffsetIndex::splice))

        .function("character_index_for_position", WRAP(&BufferOffsetIndex::character_index_for_position))
        .function("position_for_character_index", WRAP(&BufferOffsetIndex::position_for_character_index))

        ;

}
