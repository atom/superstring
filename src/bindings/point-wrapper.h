#ifndef SUPERSTRING_POINT_WRAPPER_H
#define SUPERSTRING_POINT_WRAPPER_H

#include "nan.h"
#include <optional>
using std::optional;
#include "point.h"

class PointWrapper : public Nan::ObjectWrap {
public:
  static void init();
  static v8::Local<v8::Value> from_point(Point point);
  static optional<Point> point_from_js(v8::Local<v8::Value>);

private:
  PointWrapper(Point point);

  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info);

  static void get_row(v8::Local<v8::String> property,
                     const Nan::PropertyCallbackInfo<v8::Value> &info);

  static void get_column(v8::Local<v8::String> property,
                        const Nan::PropertyCallbackInfo<v8::Value> &info);

  Point point;
};

#endif // SUPERSTRING_POINT_WRAPPER_H
