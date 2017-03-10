#pragma once

#include <emscripten/val.h>
#include "patch.h"

template <>
inline Patch const * emscripten::val::as<Patch const *>(void) const {
  using namespace emscripten;
  using namespace internal;

  EM_DESTRUCTORS destructors;
  EM_GENERIC_WIRE_TYPE result = _emval_as(
    handle,
    TypeID<AllowedRawPointer<Patch const>>::get(), &destructors
  );
  DestructorsRunner destructors_runner(destructors);

  return fromGenericWireType<Patch *>(result);
}
