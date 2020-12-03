#ifndef SUPERSTRING_RANGE_WRAPPER_H
#define SUPERSTRING_RANGE_WRAPPER_H

#include "nan.h"
#include "optional.h"
#include "point.h"
#include "range.h"

class RangeWrapper : public Nan::ObjectWrap {
public:
  static void init();
  static v8::Local<v8::Value> from_range(Range);
  static optional<Range> range_from_js(v8::Local<v8::Value>);

private:
  explicit RangeWrapper(Range);

  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &);
  static void get_start(v8::Local<v8::String>, const Nan::PropertyCallbackInfo<v8::Value> &);
  static void get_end(v8::Local<v8::String>, const Nan::PropertyCallbackInfo<v8::Value> &);

  Range range;
};

#endif // SUPERSTRING_RANGE_WRAPPER_H
