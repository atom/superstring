#include <memory>
#include <vector>
#include "as.h"
#include "auto-wrap.h"
#include "patch.h"
#include <emscripten/bind.h>
#include <emscripten/val.h>

Patch *constructor(emscripten::val value) {
  bool merge_adjacent_hunks = false;
  if (value.as<bool>() && value["mergeAdjacentHunks"].as<bool>()) {
    merge_adjacent_hunks = true;
  }
  return new Patch(merge_adjacent_hunks);
}

std::vector<uint8_t> serialize(Patch const &patch) {
  std::vector<uint8_t> result;
  patch.serialize(&result);
  return result;
}

Patch *compose(std::vector<Patch const *> const &patches) {
  return new Patch(patches);
}

Patch * deserialize(std::vector<uint8_t> const &bytes) {
  return new Patch(bytes);
}

Point get_old_extent(Patch::Hunk const &hunk) {
  return hunk.old_end.traversal(hunk.old_start);
}

Point get_new_extent(Patch::Hunk const &hunk) {
  return hunk.new_end.traversal(hunk.new_start);
}

template <typename T>
void hunk_set_noop(Patch::Hunk & hunk, T const &) {}

EMSCRIPTEN_BINDINGS(Patch) {
  emscripten::class_<Patch>("Patch")
    .constructor<>()
    .constructor<emscripten::val>(WRAP_STATIC(&constructor), emscripten::allow_raw_pointers())
    .function("splice", WRAP_OVERLOAD(&Patch::splice, bool (Patch::*)(Point, Point, Point)))
    .function("splice", WRAP_OVERLOAD(&Patch::splice, bool (Patch::*)(Point, Point, Point, std::unique_ptr<Text>, std::unique_ptr<Text>)))
    .function("spliceOld", WRAP(&Patch::splice_old))
    .function("copy", WRAP(&Patch::copy))
    .function("invert", WRAP(&Patch::invert))
    .function("getHunks", WRAP(&Patch::get_hunks))
    .function("getHunksInNewRange", WRAP_OVERLOAD(&Patch::get_hunks_in_new_range, std::vector<Patch::Hunk> (Patch::*)(Point, Point)))
    .function("getHunksInNewRange", WRAP_OVERLOAD(&Patch::get_hunks_in_new_range, std::vector<Patch::Hunk> (Patch::*)(Point, Point, bool)))
    .function("getHunksInOldRange", WRAP(&Patch::get_hunks_in_old_range))
    .function("getHunkCount", WRAP(&Patch::get_hunk_count))
    .function("hunkForOldPosition", WRAP(&Patch::hunk_for_old_position))
    .function("hunkForNewPosition", WRAP(&Patch::hunk_for_new_position))
    .function("rebalance", WRAP(&Patch::rebalance))
    .function("serialize", WRAP(&serialize))
    .class_function("compose", WRAP_STATIC(&compose), emscripten::allow_raw_pointers())
    .class_function("deserialize", WRAP_STATIC(&deserialize), emscripten::allow_raw_pointers());

  emscripten::value_object<Patch::Hunk>("Hunk")
    .field("oldStart", WRAP_FIELD(Patch::Hunk, old_start))
    .field("oldEnd", WRAP_FIELD(Patch::Hunk, old_end))
    .field("newStart", WRAP_FIELD(Patch::Hunk, new_start))
    .field("newEnd", WRAP_FIELD(Patch::Hunk, new_end))
    .field("oldText", WRAP_FIELD(Patch::Hunk, old_text))
    .field("newText", WRAP_FIELD(Patch::Hunk, new_text));
}
