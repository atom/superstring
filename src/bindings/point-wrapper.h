#ifndef SUPERSTRING_POINT_WRAPPER_H
#define SUPERSTRING_POINT_WRAPPER_H

#include "nan.h"
#include "optional.h"
#include "point.h"

class PointWrapper : public Nan::ObjectWrap {
public:
  static void Init();
  static v8::Local<v8::Value> FromPoint(Point point);
  static optional<Point> PointFromJS(v8::Local<v8::Value>);

private:
  PointWrapper(Point point);

  static void New(const Nan::FunctionCallbackInfo<v8::Value> &info);

  static void GetRow(v8::Local<v8::String> property,
                     const Nan::PropertyCallbackInfo<v8::Value> &info);

  static void GetColumn(v8::Local<v8::String> property,
                        const Nan::PropertyCallbackInfo<v8::Value> &info);

  Point point;
};

#endif // SUPERSTRING_POINT_WRAPPER_H
