#include "point-wrapper.h"
#include <cmath>
#include <nan.h>

using namespace v8;

static Nan::Persistent<String> row_string;
static Nan::Persistent<String> column_string;
static Nan::Persistent<v8::Function> constructor;

static uint32_t number_from_js(Local<Integer> js_number) {
  double number = Nan::To<double>(js_number).FromMaybe(0);
  if (number > 0 && !std::isfinite(number)) {
    return UINT32_MAX;
  } else {
    return std::max(0.0, number);
  }
}

optional<Point> PointWrapper::point_from_js(Local<Value> value) {
  Nan::MaybeLocal<Object> maybe_object = Nan::To<Object>(value);
  Local<Object> object;
  if (!maybe_object.ToLocal(&object)) {
    Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
    return optional<Point>{};
  }

  Nan::MaybeLocal<Integer> maybe_row = Nan::To<Integer>(Nan::Get(object, Nan::New(row_string)).ToLocalChecked());
  Local<Integer> js_row;
  if (!maybe_row.ToLocal(&js_row)) {
    Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
    return optional<Point>{};
  }

  Nan::MaybeLocal<Integer> maybe_column = Nan::To<Integer>(Nan::Get(object, Nan::New(column_string)).ToLocalChecked());
  Local<Integer> js_column;
  if (!maybe_column.ToLocal(&js_column)) {
    Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
    return optional<Point>{};
  }

  return Point(number_from_js(js_row), number_from_js(js_column));
}

void PointWrapper::init() {
  row_string.Reset(Nan::Persistent<String>(Nan::New("row").ToLocalChecked()));
  column_string.Reset(Nan::Persistent<String>(Nan::New("column").ToLocalChecked()));

  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("Point").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  Nan::SetAccessor(constructor_template->InstanceTemplate(), Nan::New(row_string), get_row);
  Nan::SetAccessor(constructor_template->InstanceTemplate(), Nan::New(column_string), get_column);
  constructor.Reset(Nan::GetFunction(constructor_template).ToLocalChecked());
}

Local<Value> PointWrapper::from_point(Point point) {
  Local<Object> result;
  if (Nan::New(constructor)->NewInstance(Nan::GetCurrentContext()).ToLocal(&result)) {
    (new PointWrapper(point))->Wrap(result);
    return result;
  } else {
    return Nan::Null();
  }
}

PointWrapper::PointWrapper(Point point) : point(point) {}

void PointWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {}

void PointWrapper::get_row(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
  PointWrapper *wrapper = Nan::ObjectWrap::Unwrap<PointWrapper>(info.This());
  Point &point = wrapper->point;
  info.GetReturnValue().Set(Nan::New(point.row));
}

void PointWrapper::get_column(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
  PointWrapper *wrapper = Nan::ObjectWrap::Unwrap<PointWrapper>(info.This());
  Point &point = wrapper->point;
  info.GetReturnValue().Set(Nan::New(point.column));
}
