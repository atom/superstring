#include "marker-index-wrapper.h"
#include <unordered_map>
#include "marker-index.h"
#include "nan.h"
#include "optional.h"
#include "point-wrapper.h"
#include "range.h"

using namespace v8;
using std::unordered_map;

static Nan::Persistent<String> start_string;
static Nan::Persistent<String> end_string;
static Nan::Persistent<String> touch_string;
static Nan::Persistent<String> inside_string;
static Nan::Persistent<String> overlap_string;
static Nan::Persistent<String> surround_string;

void MarkerIndexWrapper::Init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);
  constructor_template->SetClassName(Nan::New<String>("MarkerIndex").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  const auto &prototype_template = constructor_template->PrototypeTemplate();

  prototype_template->Set(Nan::New<String>("insert").ToLocalChecked(), Nan::New<FunctionTemplate>(Insert));
  prototype_template->Set(Nan::New<String>("setExclusive").ToLocalChecked(), Nan::New<FunctionTemplate>(SetExclusive));
  prototype_template->Set(Nan::New<String>("delete").ToLocalChecked(), Nan::New<FunctionTemplate>(Delete));
  prototype_template->Set(Nan::New<String>("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(Splice));
  prototype_template->Set(Nan::New<String>("getStart").ToLocalChecked(), Nan::New<FunctionTemplate>(GetStart));
  prototype_template->Set(Nan::New<String>("getEnd").ToLocalChecked(), Nan::New<FunctionTemplate>(GetEnd));
  prototype_template->Set(Nan::New<String>("getRange").ToLocalChecked(), Nan::New<FunctionTemplate>(GetRange));
  prototype_template->Set(Nan::New<String>("compare").ToLocalChecked(), Nan::New<FunctionTemplate>(Compare));
  prototype_template->Set(Nan::New<String>("findIntersecting").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(FindIntersecting));
  prototype_template->Set(Nan::New<String>("findContaining").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(FindContaining));
  prototype_template->Set(Nan::New<String>("findContainedIn").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(FindContainedIn));
  prototype_template->Set(Nan::New<String>("findStartingIn").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(FindStartingIn));
  prototype_template->Set(Nan::New<String>("findStartingAt").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(FindStartingAt));
  prototype_template->Set(Nan::New<String>("findEndingIn").ToLocalChecked(), Nan::New<FunctionTemplate>(FindEndingIn));
  prototype_template->Set(Nan::New<String>("findEndingAt").ToLocalChecked(), Nan::New<FunctionTemplate>(FindEndingAt));
  prototype_template->Set(Nan::New<String>("dump").ToLocalChecked(), Nan::New<FunctionTemplate>(Dump));
  prototype_template->Set(Nan::New<String>("getDotGraph").ToLocalChecked(), Nan::New<FunctionTemplate>(GetDotGraph));

  start_string.Reset(Nan::Persistent<String>(Nan::New("start").ToLocalChecked()));
  end_string.Reset(Nan::Persistent<String>(Nan::New("end").ToLocalChecked()));
  touch_string.Reset(Nan::Persistent<String>(Nan::New("touch").ToLocalChecked()));
  inside_string.Reset(Nan::Persistent<String>(Nan::New("inside").ToLocalChecked()));
  overlap_string.Reset(Nan::Persistent<String>(Nan::New("overlap").ToLocalChecked()));
  surround_string.Reset(Nan::Persistent<String>(Nan::New("surround").ToLocalChecked()));

  exports->Set(Nan::New("MarkerIndex").ToLocalChecked(), constructor_template->GetFunction());
}

void MarkerIndexWrapper::New(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *marker_index = new MarkerIndexWrapper();
  marker_index->Wrap(info.This());
}

Local<Set> MarkerIndexWrapper::MarkerIdsToJS(const MarkerIndex::MarkerIdSet &marker_ids) {
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

Local<Object> MarkerIndexWrapper::SnapshotToJS(const unordered_map<MarkerIndex::MarkerId, Range> &snapshot) {
  Local<Object> result_object = Nan::New<Object>();
  for (auto &pair : snapshot) {
    Local<Object> range = Nan::New<Object>();
    range->Set(Nan::New(start_string), PointWrapper::FromPoint(pair.second.start));
    range->Set(Nan::New(end_string), PointWrapper::FromPoint(pair.second.end));
    result_object->Set(Nan::New<Integer>(pair.first), range);
  }
  return result_object;
}

optional<MarkerIndex::MarkerId> MarkerIndexWrapper::MarkerIdFromJS(Local<Value> value) {
  auto result = UnsignedFromJS(value);
  if (result) {
    return *result;
  } else {
    return optional<MarkerIndex::MarkerId>{};
  }
}

optional<unsigned> MarkerIndexWrapper::UnsignedFromJS(Local<Value> value) {
  Nan::Maybe<unsigned> result = Nan::To<unsigned>(value);
  if (!result.IsJust()) {
    Nan::ThrowTypeError("Expected an non-negative integer value.");
    return optional<unsigned>{};
  }
  return result.FromJust();
}

optional<bool> MarkerIndexWrapper::BoolFromJS(Local<Value> value) {
  Nan::MaybeLocal<Boolean> maybe_boolean = Nan::To<Boolean>(value);
  Local<Boolean> boolean;
  if (!maybe_boolean.ToLocal(&boolean)) {
    Nan::ThrowTypeError("Expected an boolean.");
    return optional<bool>{};
  }

  return boolean->Value();
}

void MarkerIndexWrapper::Insert(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = MarkerIdFromJS(info[0]);
  optional<Point> start = PointWrapper::PointFromJS(info[1]);
  optional<Point> end = PointWrapper::PointFromJS(info[2]);

  if (id && start && end) {
    wrapper->marker_index.Insert(*id, *start, *end);
  }
}

void MarkerIndexWrapper::SetExclusive(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = MarkerIdFromJS(info[0]);
  optional<bool> exclusive = BoolFromJS(info[1]);

  if (id && exclusive) {
    wrapper->marker_index.SetExclusive(*id, *exclusive);
  }
}

void MarkerIndexWrapper::Delete(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = MarkerIdFromJS(info[0]);
  if (id) {
    wrapper->marker_index.Delete(*id);
  }
}

void MarkerIndexWrapper::Splice(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  optional<Point> old_extent = PointWrapper::PointFromJS(info[1]);
  optional<Point> new_extent = PointWrapper::PointFromJS(info[2]);
  if (start && old_extent && new_extent) {
    MarkerIndex::SpliceResult result = wrapper->marker_index.Splice(*start, *old_extent, *new_extent);

    Local<Object> invalidated = Nan::New<Object>();
    invalidated->Set(Nan::New(touch_string), MarkerIdsToJS(result.touch));
    invalidated->Set(Nan::New(inside_string), MarkerIdsToJS(result.inside));
    invalidated->Set(Nan::New(inside_string), MarkerIdsToJS(result.inside));
    invalidated->Set(Nan::New(overlap_string), MarkerIdsToJS(result.overlap));
    invalidated->Set(Nan::New(surround_string), MarkerIdsToJS(result.surround));
    info.GetReturnValue().Set(invalidated);
  }
}

void MarkerIndexWrapper::GetStart(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = MarkerIdFromJS(info[0]);
  if (id) {
    Point result = wrapper->marker_index.GetStart(*id);
    info.GetReturnValue().Set(PointWrapper::FromPoint(result));
  }
}

void MarkerIndexWrapper::GetEnd(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = MarkerIdFromJS(info[0]);
  if (id) {
    Point result = wrapper->marker_index.GetEnd(*id);
    info.GetReturnValue().Set(PointWrapper::FromPoint(result));
  }
}

void MarkerIndexWrapper::GetRange(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<MarkerIndex::MarkerId> id = MarkerIdFromJS(info[0]);
  if (id) {
    Range range = wrapper->marker_index.GetRange(*id);
    auto result = Nan::New<Array>(2);
    result->Set(0, PointWrapper::FromPoint(range.start));
    result->Set(1, PointWrapper::FromPoint(range.end));
    info.GetReturnValue().Set(result);
  }
}

void MarkerIndexWrapper::Compare(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
  optional<MarkerIndex::MarkerId> id1 = MarkerIdFromJS(info[0]);
  optional<MarkerIndex::MarkerId> id2 = MarkerIdFromJS(info[1]);
  if (id1 && id2) {
    info.GetReturnValue().Set(wrapper->marker_index.Compare(*id1, *id2));
  }
}

void MarkerIndexWrapper::FindIntersecting(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  optional<Point> end = PointWrapper::PointFromJS(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.FindIntersecting(*start, *end);
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindContaining(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  optional<Point> end = PointWrapper::PointFromJS(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.FindContaining(*start, *end);
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindContainedIn(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  optional<Point> end = PointWrapper::PointFromJS(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.FindContainedIn(*start, *end);
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindStartingIn(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  optional<Point> end = PointWrapper::PointFromJS(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.FindStartingIn(*start, *end);
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindStartingAt(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> position = PointWrapper::PointFromJS(info[0]);

  if (position) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.FindStartingAt(*position);
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindEndingIn(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  optional<Point> end = PointWrapper::PointFromJS(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.FindEndingIn(*start, *end);
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindEndingAt(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  optional<Point> position = PointWrapper::PointFromJS(info[0]);

  if (position) {
    MarkerIndex::MarkerIdSet result = wrapper->marker_index.FindEndingAt(*position);
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::Dump(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
  unordered_map<MarkerIndex::MarkerId, Range> snapshot = wrapper->marker_index.Dump();
  info.GetReturnValue().Set(SnapshotToJS(snapshot));
}

void MarkerIndexWrapper::GetDotGraph(const Nan::FunctionCallbackInfo<v8::Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
  std::string dot_graph = wrapper->marker_index.GetDotGraph();
  info.GetReturnValue().Set(Nan::New(dot_graph).ToLocalChecked());
}
