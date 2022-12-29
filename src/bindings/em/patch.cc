#include <memory>
#include <vector>
#include "auto-wrap.h"
#include "patch.h"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten/wire.h>

using std::runtime_error;
using std::string;
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
  bool merge_adjacent_changes = false;
  if (value.as<bool>() && value["mergeAdjacentChanges"].as<bool>()) {
    merge_adjacent_changes = true;
  }
  return new Patch(merge_adjacent_changes);
}

vector<uint8_t> serialize(Patch &patch) {
  vector<uint8_t> output;
  Serializer serializer(output);
  patch.serialize(serializer);
  return output;
}

Patch *compose(vector<Patch const *> const &patches) {
  auto result = new Patch();
  bool left_to_right = true;
  for (const Patch *patch : patches) {
    if (!result->combine(*patch, left_to_right)) {
      delete result;
      return nullptr;
    }
    left_to_right = !left_to_right;
  }
  return result;
}

Patch *deserialize(const vector<uint8_t> &bytes) {
  Deserializer deserializer(bytes);
  return new Patch(deserializer);
}

bool splice(Patch &patch, Point start, Point deleted_extent, Point inserted_extent) {
  return patch.splice(
    start,
    deleted_extent,
    inserted_extent
  );
}

bool splice_with_text(Patch &patch, Point start, Point deleted_extent, Point inserted_extent,
                      const string &deleted_text, const string &inserted_text) {
  return patch.splice(
    start,
    deleted_extent,
    inserted_extent,
    Text(deleted_text.begin(), deleted_text.end()),
    Text(inserted_text.begin(), inserted_text.end())
  );
}

template <typename T>
void change_set_noop(Patch::Change &change, T const &) {}

EMSCRIPTEN_BINDINGS(Patch) {
  emscripten::class_<Patch>("Patch")
    .constructor<>()
    .constructor(WRAP_STATIC(&constructor), emscripten::allow_raw_pointers())
    .function("splice", splice)
    .function("splice", splice_with_text)
    .function("spliceOld", WRAP(&Patch::splice_old))
    .function("copy", WRAP(&Patch::copy))
    .function("invert", WRAP(&Patch::invert))
    .function("getChanges", WRAP(&Patch::get_changes))
    .function("getChangesInNewRange", WRAP(&Patch::grab_changes_in_new_range))
    .function("getChangesInOldRange", WRAP(&Patch::grab_changes_in_old_range))
    .function("getChangeCount", WRAP(&Patch::get_change_count))
    .function("changeForOldPosition", WRAP(&Patch::grab_change_starting_before_old_position))
    .function("changeForNewPosition", WRAP(&Patch::grab_change_starting_before_new_position))
    .function("getBounds", WRAP(&Patch::get_bounds))
    .function("rebalance", WRAP(&Patch::rebalance))
    .function("serialize", WRAP(&serialize))
    .class_function("compose", WRAP_STATIC(&compose), emscripten::allow_raw_pointers())
    .class_function("deserialize", WRAP_STATIC(&deserialize), emscripten::allow_raw_pointers());

  emscripten::value_object<Patch::Change>("Change")
    .field("oldStart", WRAP_FIELD(Patch::Change, old_start))
    .field("oldEnd", WRAP_FIELD(Patch::Change, old_end))
    .field("newStart", WRAP_FIELD(Patch::Change, new_start))
    .field("newEnd", WRAP_FIELD(Patch::Change, new_end))
    .field("oldText", WRAP_FIELD(Patch::Change, old_text))
    .field("newText", WRAP_FIELD(Patch::Change, new_text));
}
