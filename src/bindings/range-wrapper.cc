#include "range-wrapper.h"
#include "point-wrapper.h"
#include <nan.h>

using namespace v8;

static Nan::Persistent<String> start_string;
static Nan::Persistent<String> end_string;
static Nan::Persistent<v8::Function> constructor;

optional<Range> RangeWrapper::range_from_js(Local<Value> value) {
  Local<Object> object;
  if (!Nan::To<Object>(value).ToLocal(&object)) {
    Nan::ThrowTypeError("Expected an object with 'start' and 'end' properties.");
    return optional<Range>{};
  }

  auto start = PointWrapper::point_from_js(Nan::Get(object, Nan::New(start_string)).ToLocalChecked());
  auto end = PointWrapper::point_from_js(Nan::Get(object, Nan::New(end_string)).ToLocalChecked());
  if (start && end) {
    return Range{*start, *end};
  } else {
    Nan::ThrowTypeError("Expected an object with 'start' and 'end' properties.");
    return optional<Range>{};
  }
}

void RangeWrapper::init() {
  start_string.Reset(Nan::Persistent<String>(Nan::New("start").ToLocalChecked()));
  end_string.Reset(Nan::Persistent<String>(Nan::New("end").ToLocalChecked()));

  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("Range").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  Nan::SetAccessor(constructor_template->InstanceTemplate(), Nan::New(start_string), get_start);
  Nan::SetAccessor(constructor_template->InstanceTemplate(), Nan::New(end_string), get_end);
  constructor.Reset(Nan::GetFunction(constructor_template).ToLocalChecked());
}

Local<Value> RangeWrapper::from_range(Range range) {
  Local<Object> result;
  if (Nan::New(constructor)->NewInstance(Nan::GetCurrentContext()).ToLocal(&result)) {
    (new RangeWrapper(range))->Wrap(result);
    return result;
  } else {
    return Nan::Null();
  }
}

RangeWrapper::RangeWrapper(Range range) : range(range) {}

void RangeWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {}

void RangeWrapper::get_start(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
  RangeWrapper *wrapper = Nan::ObjectWrap::Unwrap<RangeWrapper>(info.This());
  Range &range = wrapper->range;
  info.GetReturnValue().Set(PointWrapper::from_point(range.start));
}

void RangeWrapper::get_end(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
  RangeWrapper *wrapper = Nan::ObjectWrap::Unwrap<RangeWrapper>(info.This());
  Range &range = wrapper->range;
  info.GetReturnValue().Set(PointWrapper::from_point(range.end));
}
