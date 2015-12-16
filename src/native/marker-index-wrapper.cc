#include <unordered_map>
#include "nan.h"
#include "marker-index.h"
#include "range.h"
#include "splice-result.h"

#include <iostream>

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
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(Splice)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("getStart").ToLocalChecked(), Nan::New<FunctionTemplate>(GetStart)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("getEnd").ToLocalChecked(), Nan::New<FunctionTemplate>(GetEnd)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("compare").ToLocalChecked(), Nan::New<FunctionTemplate>(Compare)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("findIntersecting").ToLocalChecked(), Nan::New<FunctionTemplate>(FindIntersecting)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("findContaining").ToLocalChecked(), Nan::New<FunctionTemplate>(FindContaining)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("findContainedIn").ToLocalChecked(), Nan::New<FunctionTemplate>(FindContainedIn)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("findStartingIn").ToLocalChecked(), Nan::New<FunctionTemplate>(FindStartingIn)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("findEndingIn").ToLocalChecked(), Nan::New<FunctionTemplate>(FindEndingIn)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("dump").ToLocalChecked(), Nan::New<FunctionTemplate>(Dump)->GetFunction());

    // assign Number.isFinite for use from C++
    Local<String> number_string = Nan::New("Number").ToLocalChecked();
    Local<String> is_finite_string = Nan::New("isFinite").ToLocalChecked();
    Local<Object> number_constructor_local = Nan::To<Object>(Nan::GetCurrentContext()->Global()->Get(number_string)).ToLocalChecked();
    is_finite_function.Reset(Nan::Persistent<Object>(Nan::To<Object>(number_constructor_local->Get(is_finite_string)).ToLocalChecked()));

    // assign Set constructor and Set.add method for use from C++
    Local<String> set_string = Nan::New("Set").ToLocalChecked();
    Local<String> add_string = Nan::New("add").ToLocalChecked();
    Local<String> prototype_string = Nan::New("prototype").ToLocalChecked();
    Local<Object> set_constructor_local = Nan::To<Object>(Nan::GetCurrentContext()->Global()->Get(set_string)).ToLocalChecked();
    Local<Object> set_prototype_local = Nan::To<Object>(set_constructor_local->Get(prototype_string)).ToLocalChecked();
    Local<Object> set_add_method_local = Nan::To<Object>(set_prototype_local->Get(add_string)).ToLocalChecked();
    set_constructor.Reset(Nan::Persistent<Object>(set_constructor_local));
    set_add_method.Reset(Nan::Persistent<Object>(set_add_method_local));

    row_string.Reset(Nan::Persistent<String>(Nan::New("row").ToLocalChecked()));
    column_string.Reset(Nan::Persistent<String>(Nan::New("column").ToLocalChecked()));
    start_string.Reset(Nan::Persistent<String>(Nan::New("start").ToLocalChecked()));
    end_string.Reset(Nan::Persistent<String>(Nan::New("end").ToLocalChecked()));
    touch_string.Reset(Nan::Persistent<String>(Nan::New("touch").ToLocalChecked()));
    inside_string.Reset(Nan::Persistent<String>(Nan::New("inside").ToLocalChecked()));
    overlap_string.Reset(Nan::Persistent<String>(Nan::New("overlap").ToLocalChecked()));
    surround_string.Reset(Nan::Persistent<String>(Nan::New("surround").ToLocalChecked()));

    module->Set(Nan::New("exports").ToLocalChecked(),
                constructorTemplate->GetFunction());
  }

private:
  static Nan::Persistent<Object> is_finite_function;
  static Nan::Persistent<Object> set_constructor;
  static Nan::Persistent<Object> set_add_method;
  static Nan::Persistent<String> row_string;
  static Nan::Persistent<String> column_string;
  static Nan::Persistent<String> start_string;
  static Nan::Persistent<String> end_string;
  static Nan::Persistent<String> touch_string;
  static Nan::Persistent<String> inside_string;
  static Nan::Persistent<String> overlap_string;
  static Nan::Persistent<String> surround_string;

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

    Nan::MaybeLocal<Integer> maybe_row = Nan::To<Integer>(object->Get(Nan::New(row_string)));
    Local<Integer> js_row;
    if (!maybe_row.ToLocal(&js_row)) {
      Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
      return Nan::Nothing<Point>();
    }

    Nan::MaybeLocal<Integer> maybe_column = Nan::To<Integer>(object->Get(Nan::New(column_string)));
    Local<Integer> js_column;
    if (!maybe_column.ToLocal(&js_column)) {
      Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
      return Nan::Nothing<Point>();
    }

    unsigned row, column;
    if (IsFinite(js_row)) {
      row = static_cast<unsigned>(js_row->Int32Value());
    } else {
      row = UINT_MAX;
    }

    if (IsFinite(js_column)) {
      column = static_cast<unsigned>(js_column->Int32Value());
    } else {
      column = UINT_MAX;
    }

    return Nan::Just(Point(row, column));
  }

  static bool IsFinite(Local<Integer> number) {
    Local<Value> argv[] = {number};
    Local<Value> result = Nan::New(is_finite_function)->CallAsFunction(Nan::Null(), 1, argv);
    return result->BooleanValue();
  }

  static Local<Object> PointToJS(const Point &point) {
    Local<Object> result = Nan::New<Object>();
    result->Set(Nan::New(row_string), Nan::New<Integer>(point.row));
    result->Set(Nan::New(column_string), Nan::New<Integer>(point.column));
    return result;
  }

  static Local<Object> MarkerIdsToJS(const unordered_set<MarkerId> &marker_ids) {
    Local<Object> js_set = Nan::To<Object>(Nan::New(set_constructor)->CallAsConstructor(0, nullptr)).ToLocalChecked();
    Local<Object> set_add_method_local = Nan::New(set_add_method);

    for (MarkerId id : marker_ids) {
      Local<Value> argv[] {Nan::New<Integer>(id)};
      set_add_method_local->CallAsFunction(js_set, 1, argv);
    }
    return js_set;
  }

  static Local<Object> SnapshotToJS(const unordered_map<MarkerId, Range> &snapshot) {
    Local<Object> result_object = Nan::New<Object>();
    for (auto &pair : snapshot) {
      Local<Object> range = Nan::New<Object>();
      range->Set(Nan::New(start_string), PointToJS(pair.second.start));
      range->Set(Nan::New(end_string), PointToJS(pair.second.end));
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
      invalidated->Set(Nan::New(touch_string), MarkerIdsToJS(result.touch));
      invalidated->Set(Nan::New(inside_string), MarkerIdsToJS(result.inside));
      invalidated->Set(Nan::New(inside_string), MarkerIdsToJS(result.inside));
      invalidated->Set(Nan::New(overlap_string), MarkerIdsToJS(result.overlap));
      invalidated->Set(Nan::New(surround_string), MarkerIdsToJS(result.surround));
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

  static void Compare(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
    Nan::Maybe<MarkerId> id1 = MarkerIdFromJS(Nan::To<Integer>(info[0]));
    Nan::Maybe<MarkerId> id2 = MarkerIdFromJS(Nan::To<Integer>(info[1]));
    if (id1.IsJust() && id2.IsJust()) {
      info.GetReturnValue().Set(wrapper->marker_index.Compare(id1.FromJust(), id2.FromJust()));
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

Nan::Persistent<Object> MarkerIndexWrapper::is_finite_function;
Nan::Persistent<Object> MarkerIndexWrapper::set_constructor;
Nan::Persistent<Object> MarkerIndexWrapper::set_add_method;
Nan::Persistent<String> MarkerIndexWrapper::row_string;
Nan::Persistent<String> MarkerIndexWrapper::column_string;
Nan::Persistent<String> MarkerIndexWrapper::start_string;
Nan::Persistent<String> MarkerIndexWrapper::end_string;
Nan::Persistent<String> MarkerIndexWrapper::touch_string;
Nan::Persistent<String> MarkerIndexWrapper::inside_string;
Nan::Persistent<String> MarkerIndexWrapper::overlap_string;
Nan::Persistent<String> MarkerIndexWrapper::surround_string;

NODE_MODULE(marker_index, MarkerIndexWrapper::Init)
