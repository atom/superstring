#ifndef SUPERSTRING_POINT_WRAPPER_H
#define SUPERSTRING_POINT_WRAPPER_H

#include "nan.h"
#include "point.h"

class PointWrapper : public Nan::ObjectWrap {
public:
  static void Init();
  static v8::Local<v8::Value> FromPoint(Point point);
  static Nan::Maybe<Point> PointFromJS(Nan::MaybeLocal<v8::Object> maybe_object);

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
