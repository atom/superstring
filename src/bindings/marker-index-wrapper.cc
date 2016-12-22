#include "marker-index-wrapper.h"
#include <unordered_map>
#include "marker-index.h"
#include "nan.h"
#include "point-wrapper.h"
#include "range.h"

#include <iostream>

using namespace v8;
using std::unordered_set;
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

  prototype_template->Set(Nan::New<String>("generateRandomNumber").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(GenerateRandomNumber));
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

  start_string.Reset(Nan::Persistent<String>(Nan::New("start").ToLocalChecked()));
  end_string.Reset(Nan::Persistent<String>(Nan::New("end").ToLocalChecked()));
  touch_string.Reset(Nan::Persistent<String>(Nan::New("touch").ToLocalChecked()));
  inside_string.Reset(Nan::Persistent<String>(Nan::New("inside").ToLocalChecked()));
  overlap_string.Reset(Nan::Persistent<String>(Nan::New("overlap").ToLocalChecked()));
  surround_string.Reset(Nan::Persistent<String>(Nan::New("surround").ToLocalChecked()));

  exports->Set(Nan::New("MarkerIndex").ToLocalChecked(), constructor_template->GetFunction());
}

void MarkerIndexWrapper::New(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *marker_index = new MarkerIndexWrapper(Local<Number>::Cast(info[0]));
  marker_index->Wrap(info.This());
}

void MarkerIndexWrapper::GenerateRandomNumber(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
  int random = wrapper->marker_index.GenerateRandomNumber();
  info.GetReturnValue().Set(Nan::New<v8::Number>(random));
}

Local<Set> MarkerIndexWrapper::MarkerIdsToJS(const unordered_set<MarkerIndex::MarkerId> &marker_ids) {
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

Nan::Maybe<MarkerIndex::MarkerId> MarkerIndexWrapper::MarkerIdFromJS(Nan::MaybeLocal<Integer> maybe_id) {
  Local<Integer> id;
  if (!maybe_id.ToLocal(&id)) {
    Nan::ThrowTypeError("Expected an integer marker id.");
    return Nan::Nothing<MarkerIndex::MarkerId>();
  }

  return Nan::Just<MarkerIndex::MarkerId>(static_cast<MarkerIndex::MarkerId>(id->Uint32Value()));
}

Nan::Maybe<bool> MarkerIndexWrapper::BoolFromJS(Nan::MaybeLocal<Boolean> maybe_boolean) {
  Local<Boolean> boolean;
  if (!maybe_boolean.ToLocal(&boolean)) {
    Nan::ThrowTypeError("Expected an boolean.");
    return Nan::Nothing<bool>();
  }

  return Nan::Just<bool>(boolean->Value());
}

void MarkerIndexWrapper::Insert(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<MarkerIndex::MarkerId> id = MarkerIdFromJS(Nan::To<Integer>(info[0]));
  Nan::Maybe<Point> start = PointWrapper::PointFromJS(Nan::To<Object>(info[1]));
  Nan::Maybe<Point> end = PointWrapper::PointFromJS(Nan::To<Object>(info[2]));

  if (id.IsJust() && start.IsJust() && end.IsJust()) {
    wrapper->marker_index.Insert(id.FromJust(), start.FromJust(), end.FromJust());
  }
}

void MarkerIndexWrapper::SetExclusive(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<MarkerIndex::MarkerId> id = MarkerIdFromJS(Nan::To<Integer>(info[0]));
  Nan::Maybe<bool> exclusive = BoolFromJS(Nan::To<Boolean>(info[1]));

  if (id.IsJust() && exclusive.IsJust()) {
    wrapper->marker_index.SetExclusive(id.FromJust(), exclusive.FromJust());
  }
}

void MarkerIndexWrapper::Delete(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<MarkerIndex::MarkerId> id = MarkerIdFromJS(Nan::To<Integer>(info[0]));
  if (id.IsJust()) {
    wrapper->marker_index.Delete(id.FromJust());
  }
}

void MarkerIndexWrapper::Splice(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<Point> start = PointWrapper::PointFromJS(Nan::To<Object>(info[0]));
  Nan::Maybe<Point> old_extent = PointWrapper::PointFromJS(Nan::To<Object>(info[1]));
  Nan::Maybe<Point> new_extent = PointWrapper::PointFromJS(Nan::To<Object>(info[2]));
  if (start.IsJust() && old_extent.IsJust() && new_extent.IsJust()) {
    MarkerIndex::SpliceResult result = wrapper->marker_index.Splice(start.FromJust(), old_extent.FromJust(), new_extent.FromJust());

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

  Nan::Maybe<MarkerIndex::MarkerId> id = MarkerIdFromJS(Nan::To<Integer>(info[0]));
  if (id.IsJust()) {
    Point result = wrapper->marker_index.GetStart(id.FromJust());
    info.GetReturnValue().Set(PointWrapper::FromPoint(result));
  }
}

void MarkerIndexWrapper::GetEnd(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<MarkerIndex::MarkerId> id = MarkerIdFromJS(Nan::To<Integer>(info[0]));
  if (id.IsJust()) {
    Point result = wrapper->marker_index.GetEnd(id.FromJust());
    info.GetReturnValue().Set(PointWrapper::FromPoint(result));
  }
}

void MarkerIndexWrapper::GetRange(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<MarkerIndex::MarkerId> id = MarkerIdFromJS(Nan::To<Integer>(info[0]));
  if (id.IsJust()) {
    Range range = wrapper->marker_index.GetRange(id.FromJust());
    auto result = Nan::New<Array>(2);
    result->Set(0, PointWrapper::FromPoint(range.start));
    result->Set(1, PointWrapper::FromPoint(range.end));
    info.GetReturnValue().Set(result);
  }
}

void MarkerIndexWrapper::Compare(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
  Nan::Maybe<MarkerIndex::MarkerId> id1 = MarkerIdFromJS(Nan::To<Integer>(info[0]));
  Nan::Maybe<MarkerIndex::MarkerId> id2 = MarkerIdFromJS(Nan::To<Integer>(info[1]));
  if (id1.IsJust() && id2.IsJust()) {
    info.GetReturnValue().Set(wrapper->marker_index.Compare(id1.FromJust(), id2.FromJust()));
  }
}

void MarkerIndexWrapper::FindIntersecting(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<Point> start = PointWrapper::PointFromJS(Nan::To<Object>(info[0]));
  Nan::Maybe<Point> end = PointWrapper::PointFromJS(Nan::To<Object>(info[1]));

  if (start.IsJust() && end.IsJust()) {
    unordered_set<MarkerIndex::MarkerId> result = wrapper->marker_index.FindIntersecting(start.FromJust(), end.FromJust());
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindContaining(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<Point> start = PointWrapper::PointFromJS(Nan::To<Object>(info[0]));
  Nan::Maybe<Point> end = PointWrapper::PointFromJS(Nan::To<Object>(info[1]));

  if (start.IsJust() && end.IsJust()) {
    unordered_set<MarkerIndex::MarkerId> result = wrapper->marker_index.FindContaining(start.FromJust(), end.FromJust());
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindContainedIn(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<Point> start = PointWrapper::PointFromJS(Nan::To<Object>(info[0]));
  Nan::Maybe<Point> end = PointWrapper::PointFromJS(Nan::To<Object>(info[1]));

  if (start.IsJust() && end.IsJust()) {
    unordered_set<MarkerIndex::MarkerId> result = wrapper->marker_index.FindContainedIn(start.FromJust(), end.FromJust());
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindStartingIn(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<Point> start = PointWrapper::PointFromJS(Nan::To<Object>(info[0]));
  Nan::Maybe<Point> end = PointWrapper::PointFromJS(Nan::To<Object>(info[1]));

  if (start.IsJust() && end.IsJust()) {
    unordered_set<MarkerIndex::MarkerId> result = wrapper->marker_index.FindStartingIn(start.FromJust(), end.FromJust());
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindStartingAt(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<Point> position = PointWrapper::PointFromJS(Nan::To<Object>(info[0]));

  if (position.IsJust()) {
    unordered_set<MarkerIndex::MarkerId> result = wrapper->marker_index.FindStartingAt(position.FromJust());
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindEndingIn(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<Point> start = PointWrapper::PointFromJS(Nan::To<Object>(info[0]));
  Nan::Maybe<Point> end = PointWrapper::PointFromJS(Nan::To<Object>(info[1]));

  if (start.IsJust() && end.IsJust()) {
    unordered_set<MarkerIndex::MarkerId> result = wrapper->marker_index.FindEndingIn(start.FromJust(), end.FromJust());
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::FindEndingAt(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

  Nan::Maybe<Point> position = PointWrapper::PointFromJS(Nan::To<Object>(info[0]));

  if (position.IsJust()) {
    unordered_set<MarkerIndex::MarkerId> result = wrapper->marker_index.FindEndingAt(position.FromJust());
    info.GetReturnValue().Set(MarkerIdsToJS(result));
  }
}

void MarkerIndexWrapper::Dump(const Nan::FunctionCallbackInfo<Value> &info) {
  MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
  unordered_map<MarkerIndex::MarkerId, Range> snapshot = wrapper->marker_index.Dump();
  info.GetReturnValue().Set(SnapshotToJS(snapshot));
}

MarkerIndexWrapper::MarkerIndexWrapper(v8::Local<v8::Number> seed)
    : marker_index{static_cast<unsigned>(seed->Int32Value())} {}
