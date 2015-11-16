#include <unordered_map>
#include "nan.h"
#include "marker-index.h"
#include "range.h"
#include "splice-result.h"

using namespace v8;
using std::unordered_set;
using std::unordered_map;

class MarkerIndexWrapper : public Nan::ObjectWrap {
public:
  static void Init(Local<Object> exports, Local<Object> module) {
    Local<FunctionTemplate> constructorTemplate =
        Nan::New<FunctionTemplate>(New);
    constructorTemplate->SetClassName(
        Nan::New<String>("MarkerIndex").ToLocalChecked());
    constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("generateRandomNumber").ToLocalChecked(), Nan::New<FunctionTemplate>(GenerateRandomNumber)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("insert").ToLocalChecked(), Nan::New<FunctionTemplate>(Insert)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("setExclusive").ToLocalChecked(), Nan::New<FunctionTemplate>(SetExclusive)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("delete").ToLocalChecked(), Nan::New<FunctionTemplate>(Delete)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("_splice").ToLocalChecked(), Nan::New<FunctionTemplate>(Splice)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("getStart").ToLocalChecked(), Nan::New<FunctionTemplate>(GetStart)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("getEnd").ToLocalChecked(), Nan::New<FunctionTemplate>(GetEnd)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("_findIntersecting").ToLocalChecked(), Nan::New<FunctionTemplate>(FindIntersecting)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("_findContaining").ToLocalChecked(), Nan::New<FunctionTemplate>(FindContaining)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("_findContainedIn").ToLocalChecked(), Nan::New<FunctionTemplate>(FindContainedIn)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("_findStartingIn").ToLocalChecked(), Nan::New<FunctionTemplate>(FindStartingIn)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("_findEndingIn").ToLocalChecked(), Nan::New<FunctionTemplate>(FindEndingIn)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("dump").ToLocalChecked(), Nan::New<FunctionTemplate>(Dump)->GetFunction());

    row_key.Reset(Nan::Persistent<String>(Nan::New("row").ToLocalChecked()));
    column_key.Reset(Nan::Persistent<String>(Nan::New("column").ToLocalChecked()));
    start_key.Reset(Nan::Persistent<String>(Nan::New("start").ToLocalChecked()));
    end_key.Reset(Nan::Persistent<String>(Nan::New("end").ToLocalChecked()));
    touch_key.Reset(Nan::Persistent<String>(Nan::New("touch").ToLocalChecked()));
    inside_key.Reset(Nan::Persistent<String>(Nan::New("inside").ToLocalChecked()));
    overlap_key.Reset(Nan::Persistent<String>(Nan::New("overlap").ToLocalChecked()));
    surround_key.Reset(Nan::Persistent<String>(Nan::New("surround").ToLocalChecked()));

    module->Set(Nan::New("exports").ToLocalChecked(),
                constructorTemplate->GetFunction());
  }

private:
  static Nan::Persistent<String> row_key;
  static Nan::Persistent<String> column_key;
  static Nan::Persistent<String> start_key;
  static Nan::Persistent<String> end_key;
  static Nan::Persistent<String> touch_key;
  static Nan::Persistent<String> inside_key;
  static Nan::Persistent<String> overlap_key;
  static Nan::Persistent<String> surround_key;

  static void New(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *marker_index = new MarkerIndexWrapper(Local<Number>::Cast(info[0]));
    marker_index->Wrap(info.This());
  }

  static void GenerateRandomNumber(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
    int random = wrapper->marker_index.GenerateRandomNumber();
    info.GetReturnValue().Set(Nan::New<v8::Number>(random));
  }

  static Nan::Maybe<Point> PointFromJS(Nan::MaybeLocal<Object> maybe_object) {
    Local<Object> object;
    if (!maybe_object.ToLocal(&object)) {
      Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
      return Nan::Nothing<Point>();
    }

    Nan::MaybeLocal<Integer> maybe_row = Nan::To<Integer>(object->Get(Nan::New(row_key)));
    Local<Integer> row;
    if (!maybe_row.ToLocal(&row)) {
      Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
      return Nan::Nothing<Point>();
    }

    Nan::MaybeLocal<Integer> maybe_column = Nan::To<Integer>(object->Get(Nan::New(column_key)));
    Local<Integer> column;
    if (!maybe_column.ToLocal(&column)) {
      Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
      return Nan::Nothing<Point>();
    }

    return Nan::Just(Point(
      static_cast<unsigned>(row->Int32Value()),
      static_cast<unsigned>(column->Int32Value())
    ));
  }

  static Local<Object> PointToJS(const Point &point) {
    Local<Object> result = Nan::New<Object>();
    result->Set(Nan::New(row_key), Nan::New<Integer>(point.row));
    result->Set(Nan::New(column_key), Nan::New<Integer>(point.column));
    return result;
  }

  static Local<Array> MarkerIdsToJS(const unordered_set<MarkerId> &marker_ids) {
    Local<Array> result_array = Nan::New<Array>(marker_ids.size());
    uint32_t index = 0;
    for (const MarkerId &id : marker_ids) {
      result_array->Set(index++, Nan::New<Integer>(id));
    }
    return result_array;
  }

  static Local<Object> SnapshotToJS(const unordered_map<MarkerId, Range> &snapshot) {
    Local<Object> result_object = Nan::New<Object>();
    for (auto &pair : snapshot) {
      Local<Object> range = Nan::New<Object>();
      range->Set(Nan::New(start_key), PointToJS(pair.second.start));
      range->Set(Nan::New(end_key), PointToJS(pair.second.end));
      result_object->Set(Nan::New<Integer>(pair.first), range);
    }
    return result_object;
  }

  static Nan::Maybe<MarkerId> MarkerIdFromJS(Nan::MaybeLocal<Integer> maybe_id) {
    Local<Integer> id;
    if (!maybe_id.ToLocal(&id)) {
      Nan::ThrowTypeError("Expected an integer marker id.");
      return Nan::Nothing<MarkerId>();
    }

    return Nan::Just<MarkerId>(static_cast<MarkerId>(id->Uint32Value()));
  }

  static Nan::Maybe<bool> BoolFromJS(Nan::MaybeLocal<Boolean> maybe_boolean) {
    Local<Boolean> boolean;
    if (!maybe_boolean.ToLocal(&boolean)) {
      Nan::ThrowTypeError("Expected an boolean.");
      return Nan::Nothing<bool>();
    }

    return Nan::Just<bool>(boolean->Value());
  }

  static void Insert(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Nan::Maybe<MarkerId> id = MarkerIdFromJS(Nan::To<Integer>(info[0]));
    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[1]));
    Nan::Maybe<Point> end = PointFromJS(Nan::To<Object>(info[2]));

    if (id.IsJust() && start.IsJust() && end.IsJust()) {
      wrapper->marker_index.Insert(id.FromJust(), start.FromJust(), end.FromJust());
    }
  }

  static void SetExclusive(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Nan::Maybe<MarkerId> id = MarkerIdFromJS(Nan::To<Integer>(info[0]));
    Nan::Maybe<bool> exclusive = BoolFromJS(Nan::To<Boolean>(info[1]));

    if (id.IsJust() && exclusive.IsJust()) {
      wrapper->marker_index.SetExclusive(id.FromJust(), exclusive.FromJust());
    }
  }

  static void Delete(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Nan::Maybe<MarkerId> id = MarkerIdFromJS(Nan::To<Integer>(info[0]));
    if (id.IsJust()) {
      wrapper->marker_index.Delete(id.FromJust());
    }
  }

  static void Splice(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Nan::Maybe<Point> old_extent = PointFromJS(Nan::To<Object>(info[1]));
    Nan::Maybe<Point> new_extent = PointFromJS(Nan::To<Object>(info[2]));
    if (start.IsJust() && old_extent.IsJust() && new_extent.IsJust()) {
      SpliceResult result = wrapper->marker_index.Splice(start.FromJust(), old_extent.FromJust(), new_extent.FromJust());

      Local<Object> invalidated = Nan::New<Object>();
      invalidated->Set(Nan::New(touch_key), MarkerIdsToJS(result.touch));
      invalidated->Set(Nan::New(inside_key), MarkerIdsToJS(result.inside));
      invalidated->Set(Nan::New(inside_key), MarkerIdsToJS(result.inside));
      invalidated->Set(Nan::New(overlap_key), MarkerIdsToJS(result.overlap));
      invalidated->Set(Nan::New(surround_key), MarkerIdsToJS(result.surround));
      info.GetReturnValue().Set(invalidated);
    }
  }

  static void GetStart(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Nan::Maybe<MarkerId> id = MarkerIdFromJS(Nan::To<Integer>(info[0]));
    if (id.IsJust()) {
      Point result = wrapper->marker_index.GetStart(id.FromJust());
      info.GetReturnValue().Set(PointToJS(result));
    }
  }

  static void GetEnd(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Nan::Maybe<MarkerId> id = MarkerIdFromJS(Nan::To<Integer>(info[0]));
    if (id.IsJust()) {
      Point result = wrapper->marker_index.GetEnd(id.FromJust());
      info.GetReturnValue().Set(PointToJS(result));
    }
  }

  static void FindIntersecting(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Nan::Maybe<Point> end = PointFromJS(Nan::To<Object>(info[1]));

    if (start.IsJust() && end.IsJust()) {
      unordered_set<MarkerId> result = wrapper->marker_index.FindIntersecting(start.FromJust(), end.FromJust());
      info.GetReturnValue().Set(MarkerIdsToJS(result));
    }
  }

  static void FindContaining(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Nan::Maybe<Point> end = PointFromJS(Nan::To<Object>(info[1]));

    if (start.IsJust() && end.IsJust()) {
      unordered_set<MarkerId> result = wrapper->marker_index.FindContaining(start.FromJust(), end.FromJust());
      info.GetReturnValue().Set(MarkerIdsToJS(result));
    }
  }

  static void FindContainedIn(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Nan::Maybe<Point> end = PointFromJS(Nan::To<Object>(info[1]));

    if (start.IsJust() && end.IsJust()) {
      unordered_set<MarkerId> result = wrapper->marker_index.FindContainedIn(start.FromJust(), end.FromJust());
      info.GetReturnValue().Set(MarkerIdsToJS(result));
    }
  }

  static void FindStartingIn(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Nan::Maybe<Point> end = PointFromJS(Nan::To<Object>(info[1]));

    if (start.IsJust() && end.IsJust()) {
      unordered_set<MarkerId> result = wrapper->marker_index.FindStartingIn(start.FromJust(), end.FromJust());
      info.GetReturnValue().Set(MarkerIdsToJS(result));
    }
  }

  static void FindEndingIn(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Nan::Maybe<Point> end = PointFromJS(Nan::To<Object>(info[1]));

    if (start.IsJust() && end.IsJust()) {
      unordered_set<MarkerId> result = wrapper->marker_index.FindEndingIn(start.FromJust(), end.FromJust());
      info.GetReturnValue().Set(MarkerIdsToJS(result));
    }
  }

  static void Dump(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
    unordered_map<MarkerId, Range> snapshot = wrapper->marker_index.Dump();
    info.GetReturnValue().Set(SnapshotToJS(snapshot));
  }

  MarkerIndexWrapper(v8::Local<v8::Number> seed) :
    marker_index{static_cast<unsigned>(seed->Int32Value())} {}

  MarkerIndex marker_index;
};

Nan::Persistent<String> MarkerIndexWrapper::row_key;
Nan::Persistent<String> MarkerIndexWrapper::column_key;
Nan::Persistent<String> MarkerIndexWrapper::start_key;
Nan::Persistent<String> MarkerIndexWrapper::end_key;
Nan::Persistent<String> MarkerIndexWrapper::touch_key;
Nan::Persistent<String> MarkerIndexWrapper::inside_key;
Nan::Persistent<String> MarkerIndexWrapper::overlap_key;
Nan::Persistent<String> MarkerIndexWrapper::surround_key;

NODE_MODULE(marker_index, MarkerIndexWrapper::Init)
