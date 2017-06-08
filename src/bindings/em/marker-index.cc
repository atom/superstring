#include "auto-wrap.h"
#include "marker-index.h"
#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(MarkerIndex) {
  emscripten::class_<MarkerIndex>("MarkerIndex")
    .constructor<>()
    .constructor<unsigned>()
    .function("generateRandomNumber", WRAP(&MarkerIndex::generate_random_number))
    .function("insert", WRAP(&MarkerIndex::insert))
    .function("setExclusive", WRAP(&MarkerIndex::set_exclusive))
    .function("remove", WRAP(&MarkerIndex::remove))
    .function("splice", WRAP(&MarkerIndex::splice))
    .function("has", WRAP(&MarkerIndex::has))
    .function("getStart", WRAP(&MarkerIndex::get_start))
    .function("getEnd", WRAP(&MarkerIndex::get_end))
    .function("getRange", WRAP(&MarkerIndex::get_range))
    .function("compare", WRAP(&MarkerIndex::compare))
    .function("findIntersecting", WRAP(&MarkerIndex::find_intersecting))
    .function("findContaining", WRAP(&MarkerIndex::find_containing))
    .function("findContainedIn", WRAP(&MarkerIndex::find_contained_in))
    .function("findStartingIn", WRAP(&MarkerIndex::find_starting_in))
    .function("findStartingAt", WRAP(&MarkerIndex::find_starting_at))
    .function("findEndingIn", WRAP(&MarkerIndex::find_ending_in))
    .function("findEndingAt", WRAP(&MarkerIndex::find_ending_at))
    .function("findBoundariesAfter", WRAP(&MarkerIndex::find_boundaries_after))
    .function("dump", WRAP(&MarkerIndex::dump));

  emscripten::value_object<MarkerIndex::SpliceResult>("SpliceResult")
    .field("touch", WRAP_FIELD(MarkerIndex::SpliceResult, touch))
    .field("inside", WRAP_FIELD(MarkerIndex::SpliceResult, inside))
    .field("overlap", WRAP_FIELD(MarkerIndex::SpliceResult, overlap))
    .field("surround", WRAP_FIELD(MarkerIndex::SpliceResult, surround));

  emscripten::value_object<MarkerIndex::BoundaryQueryResult>("BoundaryQueryResult")
    .field("containing_start", &MarkerIndex::BoundaryQueryResult::containing_start)
    .field("boundaries", &MarkerIndex::BoundaryQueryResult::boundaries);

  emscripten::value_object<MarkerIndex::Boundary>("Boundary")
    .field("position", &MarkerIndex::Boundary::position)
    .field("starting", &MarkerIndex::Boundary::starting)
    .field("ending", &MarkerIndex::Boundary::ending);
}
