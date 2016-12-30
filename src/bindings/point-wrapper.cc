#include "point-wrapper.h"
#include <cmath>
#include "nan.h"

using namespace v8;

static Nan::Persistent<String> row_string;
static Nan::Persistent<String> column_string;
static Nan::Persistent<v8::Function> constructor;

optional<Point> PointWrapper::PointFromJS(Local<Value> value) {
  Nan::MaybeLocal<Object> maybe_object = Nan::To<Object>(value);
  Local<Object> object;
  if (!maybe_object.ToLocal(&object)) {
    Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
    return optional<Point>{};
  }

  Nan::MaybeLocal<Integer> maybe_row = Nan::To<Integer>(object->Get(Nan::New(row_string)));
  Local<Integer> js_row;
  if (!maybe_row.ToLocal(&js_row)) {
    Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
    return optional<Point>{};
  }

  Nan::MaybeLocal<Integer> maybe_column = Nan::To<Integer>(object->Get(Nan::New(column_string)));
  Local<Integer> js_column;
  if (!maybe_column.ToLocal(&js_column)) {
    Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
    return optional<Point>{};
  }

  double row = js_row->NumberValue();
  double column = js_column->NumberValue();
  return Point(
    std::isfinite(row) ? row : UINT32_MAX,
    std::isfinite(column) ? column : UINT32_MAX
  );
}

void PointWrapper::Init() {
  row_string.Reset(Nan::Persistent<String>(Nan::New("row").ToLocalChecked()));
  column_string.Reset(Nan::Persistent<String>(Nan::New("column").ToLocalChecked()));

  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);
  constructor_template->SetClassName(Nan::New<String>("Point").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  Nan::SetAccessor(constructor_template->InstanceTemplate(), Nan::New(row_string), GetRow);
  Nan::SetAccessor(constructor_template->InstanceTemplate(), Nan::New(column_string), GetColumn);
  constructor.Reset(constructor_template->GetFunction());
}

Local<Value> PointWrapper::FromPoint(Point point) {
  Local<Object> result;
  if (Nan::New(constructor)->NewInstance(Nan::GetCurrentContext()).ToLocal(&result)) {
    (new PointWrapper(point))->Wrap(result);
    return result;
  } else {
    return Nan::Null();
  }
}

PointWrapper::PointWrapper(Point point) : point(point) {}

void PointWrapper::New(const Nan::FunctionCallbackInfo<Value> &info) {}

void PointWrapper::GetRow(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
  PointWrapper *wrapper = Nan::ObjectWrap::Unwrap<PointWrapper>(info.This());
  Point &point = wrapper->point;
  info.GetReturnValue().Set(Nan::New(point.row));
}

void PointWrapper::GetColumn(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
  PointWrapper *wrapper = Nan::ObjectWrap::Unwrap<PointWrapper>(info.This());
  Point &point = wrapper->point;
  info.GetReturnValue().Set(Nan::New(point.column));
}
