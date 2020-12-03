#include "marker-index-wrapper.h"
#include <unordered_map>
#include "marker-index.h"
#include <nan.h>
#include "noop.h"
#include <optional>
using std::optional;
#include "point-wrapper.h"
#include "range.h"

using namespace v8;
using std::unordered_map;

static Nan::Persistent<v8::FunctionTemplate> marker_index_constructor_template;
static Nan::Persistent<String> start_string;
static Nan::Persistent<String> end_string;
static Nan::Persistent<String> touch_string;
static Nan::Persistent<String> inside_string;
static Nan::Persistent<String> overlap_string;
static Nan::Persistent<String> surround_string;
static Nan::Persistent<String> containing_start_string;
static Nan::Persistent<String> boundaries_string;
static Nan::Persistent<String> position_string;
static Nan::Persistent<String> starting_string;
static Nan::Persistent<String> ending_string;

void MarkerIndexWrapper::init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("MarkerIndex").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  const auto &prototype_template = constructor_template->PrototypeTemplate();

  Nan::SetTemplate(prototype_template, Nan::New<String>("delete").ToLocalChecked(), Nan::New<FunctionTemplate>(noop), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("generateRandomNumber").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(generate_random_number), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("insert").ToLocalChecked(), Nan::New<FunctionTemplate>(insert), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("setExclusive").ToLocalChecked(), Nan::New<FunctionTemplate>(set_exclusive), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("remove").ToLocalChecked(), Nan::New<FunctionTemplate>(remove), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("has").ToLocalChecked(), Nan::New<FunctionTemplate>(has), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(splice), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("getStart").ToLocalChecked(), Nan::New<FunctionTemplate>(get_start), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("getEnd").ToLocalChecked(), Nan::New<FunctionTemplate>(get_end), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("getRange").ToLocalChecked(), Nan::New<FunctionTemplate>(get_range), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("compare").ToLocalChecked(), Nan::New<FunctionTemplate>(compare), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("findIntersecting").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(find_intersecting), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("findContaining").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(find_containing), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("findContainedIn").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(find_contained_in), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("findStartingIn").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(find_starting_in), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("findStartingAt").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(find_starting_at), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("findEndingIn").ToLocalChecked(), Nan::New<FunctionTemplate>(find_ending_in), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("findEndingAt").ToLocalChecked(), Nan::New<FunctionTemplate>(find_ending_at), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("findBoundariesAfter").ToLocalChecked(), Nan::New<FunctionTemplate>(find_boundaries_after), None);
  Nan::SetTemplate(prototype_template, Nan::New<String>("dump").ToLocalChecked(), Nan::New<FunctionTemplate>(dump), None);

  start_string.Reset(Nan::Persistent<String>(Nan::New("start").ToLocalChecked()));
  end_string.Reset(Nan::Persistent<String>(Nan::New("end").ToLocalChecked()));
  touch_string.Reset(Nan::Persistent<String>(Nan::New("touch").ToLocalChecked()));
  inside_string.Reset(Nan::Persistent<String>(Nan::New("inside").ToLocalChecked()));
  overlap_string.Reset(Nan::Persistent<String>(Nan::New("overlap").ToLocalChecked()));
  surround_string.Reset(Nan::Persistent<String>(Nan::New("surround").ToLocalChecked()));
  containing_start_string.Reset(Nan::Persistent<String>(Nan::New("containingStart").ToLocalChecked()));
  boundaries_string.Reset(Nan::Persistent<String>(Nan::New("boundaries").ToLocalChecked()));
  position_string.Reset(Nan::Persistent<String>(Nan::New("position").ToLocalChecked()));
  starting_string.Reset(Nan::Persistent<String>(Nan::New("starting").ToLocalChecked()));
  ending_string.Reset(Nan::Persistent<String>(Nan::New("ending").ToLocalChecked()));

  marker_index_constructor_template.Reset(constructor_template);
  Nan::Set(exports, Nan::New("MarkerIndex").ToLocalChecked(), Nan::GetFunction(constructor_template).ToLocalChecked());
}

MarkerIndex *MarkerIndexWrapper::from_js(Local<Value> value) {
  auto js_marker_index = Local<Object>::Cast(value);
  if (!Nan::New(marker_index_constructor_template)->HasInstance(js_marker_index)) {
    return nullptr;
  }
  return &Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(js_marker_index)->marker_index;
}

void MarkerIndexWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  auto seed = Nan::To<unsigned>(info[0]);
  MarkerIndexWrapper *marker_index = new MarkerIndexWrapper(seed.IsJust() ? seed.FromJust() : 0u);
  marker_index->Wrap(info.This());
}

void MarkerIndexWrapper::generate_random_number(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
  int random = wrapper->marker_index.generate_random_number();
  info.GetReturnValue().Set(Nan::New<v8::Number>(random));
}

Local<Set> MarkerIndexWrapper::marker_ids_set_to_js(const MarkerIndex::MarkerIdSet &marker_ids) {
  Isolate *isolate = v8::Isolate::GetCurrent();
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Set> js_set = v8::Set::New(isolate);

  for (MarkerIndex::MarkerId id : marker_ids) {
    // Not sure why Set::Add warns if we don't use its return value, but
    // just doing it to avoid the warning.
    js_set = js_set->Add(context, Nan::New<Integer>(id)).ToLocalChecked();
  }

  return js_set;
}

Local<Array> MarkerIndexWrapper::marker_ids_vector_to_js(const std::vector<MarkerIndex::MarkerId> &marker_ids) {
  Local<Array> js_array = Nan::New<Array>(marker_ids.size());

  Isolate *isolate = v8::Isolate::GetCurrent();
  Local<Context> context = isolate->GetCurrentContext();
  for (size_t i = 0; i < marker_ids.size(); i++) {
    js_array->Set(context, i, Nan::New<Integer>(marker_ids[i]));
  }
  return js_array;
}

Local<Object> MarkerIndexWrapper::snapshot_to_js(const unordered_map<MarkerIndex::MarkerId, Range> &snapshot) {
  Local<Object> result_object = Nan::New<Object>();
  Isolate *isolate = v8::Isolate::GetCurrent();
  Local<Context> context = isolate->GetCurrentContext();
  for (auto &pair : snapshot) {
    Local<Object> range = Nan::New<Object>();
    range->Set(context, Nan::New(start_string), PointWrapper::from_point(pair.second.start));
    range->Set(context, Nan::New(end_string), PointWrapper::from_point(pair.second.end));
    result_object->Set(context, Nan::New<Integer>(pair.first), range);
  }
  return result_object;
}

optional<MarkerIndex::MarkerId> MarkerIndexWrapper::marker_id_from_js(Local<Value> value) {
  auto result = unsigned_from_js(value);
  if (result) {
    return *result;
  } else {
    return optional<MarkerIndex::MarkerId>{};
  }
}

optional<unsigned> MarkerIndexWrapper::unsigned_from_js(Local<Value> value) {
  Nan::Maybe<unsigned> result = Nan::To<unsigned>(value);
  if (!result.IsJust()) {
    Nan::ThrowTypeError("Expected an non-negative integer value.");
    return optional<unsigned>{};
  }
  return result.FromJust();
}

optional<bool> MarkerIndexWrapper::bool_from_js(Local<Value> value) {
  Nan::MaybeLocal<Boolean> maybe_boolean = Nan::To<Boolean>(value);
  Local<Boolean> boolean;
  if (!maybe_boolean.ToLocal(&boolean)) {
    Nan::ThrowTypeError("Expected an boolean.");
    return optional<bool>{};
  }

  return boolean->Value();
}

void MarkerIndexWrapper::insert(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  optional<Point> start = PointWrapper::point_from_js(info[1]);
  optional<Point> end = PointWrapper::point_from_js(info[2]);

  if (id && start && end) {
    wrapper->marker_index.insert(*id, *start, *end);
  }
}

void MarkerIndexWrapper::set_exclusive(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  optional<bool> exclusive = bool_from_js(info[1]);

  if (id && exclusive) {
    wrapper->marker_index.set_exclusive(*id, *exclusive);
  }
}

void MarkerIndexWrapper::remove(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    wrapper->marker_index.remove(*id);
  }
}

void MarkerIndexWrapper::has(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    bool result = wrapper->marker_index.has(*id);
    info.GetReturnValue().Set(Nan::New(result));
  }
}

void MarkerIndexWrapper::splice(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> old_extent = PointWrapper::point_from_js(info[1]);
  optional<Point> new_extent = PointWrapper::point_from_js(info[2]);
  if (start && old_extent && new_extent) {
    MarkerIndex::SpliceResult result = wrapper->marker_index.splice(*start, *old_extent, *new_extent);

    Local<Object> invalidated = Nan::New<Object>();
    Nan::Set(invalidated, Nan::New(touch_string), marker_ids_set_to_js(result.touch));
    Nan::Set(invalidated, Nan::New(inside_string), marker_ids_set_to_js(result.inside));
    Nan::Set(invalidated, Nan::New(inside_string), marker_ids_set_to_js(result.inside));
    Nan::Set(invalidated, Nan::New(overlap_string), marker_ids_set_to_js(result.overlap));
    Nan::Set(invalidated, Nan::New(surround_string), marker_ids_set_to_js(result.surround));
    info.GetReturnValue().Set(invalidated);
  }
}

void MarkerIndexWrapper::get_start(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    Point result = wrapper->marker_index.get_start(*id);
    info.GetReturnValue().Set(PointWrapper::from_point(result));
  }
}

void MarkerIndexWrapper::get_end(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    Point result = wrapper->marker_index.get_end(*id);
    info.GetReturnValue().Set(PointWrapper::from_point(result));
  }
}

void MarkerIndexWrapper::get_range(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    Range range = wrapper->marker_index.get_range(*id);
    auto result = Nan::New<Object>();
    Nan::Set(result, Nan::New(start_string), PointWrapper::from_point(range.start));
    Nan::Set(result, Nan::New(end_string), PointWrapper::from_point(range.end));
    info.GetReturnValue().Set(result);
  }
}

void MarkerIndexWrapper::compare(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
  optional<MarkerIndex::MarkerId> id1 = marker_id_from_js(info[0]);
  optional<MarkerIndex::MarkerId> id2 = marker_id_from_js(info[1]);
  if (id1 && id2) {
    info.GetReturnValue().Set(wrapper->marker_index.compare(*id1, *id2));
  }
}

void MarkerIndexWrapper::find_intersecting(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_intersecting(*start, *end);
    info.GetReturnValue().Set(marker_ids_set_to_js(result));
  }
}

void MarkerIndexWrapper::find_containing(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_containing(*start, *end);
    info.GetReturnValue().Set(marker_ids_set_to_js(result));
  }
}

void MarkerIndexWrapper::find_contained_in(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_contained_in(*start, *end);
    info.GetReturnValue().Set(marker_ids_set_to_js(result));
  }
}

void MarkerIndexWrapper::find_starting_in(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_starting_in(*start, *end);
    info.GetReturnValue().Set(marker_ids_set_to_js(result));
  }
}

void MarkerIndexWrapper::find_starting_at(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> position = PointWrapper::point_from_js(info[0]);

  if (position) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_starting_at(*position);
    info.GetReturnValue().Set(marker_ids_set_to_js(result));
  }
}

void MarkerIndexWrapper::find_ending_in(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_ending_in(*start, *end);
    info.GetReturnValue().Set(marker_ids_set_to_js(result));
  }
}

void MarkerIndexWrapper::find_ending_at(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> position = PointWrapper::point_from_js(info[0]);

  if (position) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_ending_at(*position);
    info.GetReturnValue().Set(marker_ids_set_to_js(result));
  }
}

void MarkerIndexWrapper::find_boundaries_after(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<size_t> max_count;
  Local<Integer> js_max_count;
  if (Nan::To<Integer>(info[1]).ToLocal(&js_max_count)) {
    max_count = Nan::To<uint32_t>(js_max_count).FromMaybe(0);
  }

  if (start && max_count) {
    MarkerIndex::BoundaryQueryResult result = wrapper->marker_index.find_boundaries_after(*start, *max_count);
    Local<Object> js_result = Nan::New<Object>();
    Nan::Set(js_result, Nan::New(containing_start_string), marker_ids_vector_to_js(result.containing_start));

    Local<Array> js_boundaries = Nan::New<Array>(result.boundaries.size());
    for (size_t i = 0; i < result.boundaries.size(); i++) {
      MarkerIndex::Boundary boundary = result.boundaries[i];
      Local<Object> js_boundary = Nan::New<Object>();
      Nan::Set(js_boundary, Nan::New(position_string), PointWrapper::from_point(boundary.position));
      Nan::Set(js_boundary, Nan::New(starting_string), marker_ids_set_to_js(boundary.starting));
      Nan::Set(js_boundary, Nan::New(ending_string), marker_ids_set_to_js(boundary.ending));
      Nan::Set(js_boundaries, i, js_boundary);
    }
    Nan::Set(js_result, Nan::New(boundaries_string), js_boundaries);

    info.GetReturnValue().Set(js_result);
  }
}

void MarkerIndexWrapper::dump(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
  unordered_map<MarkerIndex::MarkerId, Range> snapshot = wrapper->marker_index.dump();
  info.GetReturnValue().Set(snapshot_to_js(snapshot));
}

MarkerIndexWrapper::MarkerIndexWrapper(unsigned seed) : marker_index{seed} {}
