#include <memory>
#include <vector>
#include "auto-wrap.h"
#include "patch.h"
#include <emscripten/bind.h>
#include <emscripten/val.h>

using std::vector;

template <>
inline Patch const *emscripten::val::as<Patch const *>(void) const {
  using namespace emscripten;
  using namespace internal;

  EM_DESTRUCTORS destructors;
  EM_GENERIC_WIRE_TYPE result = _emval_as(
    handle,
    TypeID<AllowedRawPointer<Patch const>>::get(),
    &destructors
  );
  DestructorsRunner destructors_runner(destructors);

  return fromGenericWireType<Patch *>(result);
}

Patch *constructor(emscripten::val value) {
  bool merge_adjacent_hunks = false;
  if (value.as<bool>() && value["mergeAdjacentHunks"].as<bool>()) {
    merge_adjacent_hunks = true;
  }
  return new Patch(merge_adjacent_hunks);
}

vector<uint8_t> serialize(Patch const &patch) {
  vector<uint8_t> output;
  Serializer serializer(output);
  patch.serialize(serializer);
  return output;
}

Patch *compose(vector<Patch const *> const &patches) {
  return new Patch(patches);
}

Patch *deserialize(const vector<uint8_t> &bytes) {
  Deserializer deserializer(bytes);
  return new Patch(deserializer);
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
