#include "nan.h"
#include "marker-index.h"
#include "range.h"

class MarkerIndexWrapper : public Nan::ObjectWrap {
public:
  static void Init(v8::Local<v8::Object> exports);

private:
  static void New(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GenerateRandomNumber(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static Nan::Maybe<Point> PointFromJS(Nan::MaybeLocal<v8::Object> maybe_object);
  static bool IsFinite(v8::Local<v8::Integer> number);
  static v8::Local<v8::Object> PointToJS(const Point &point);
  static v8::Local<v8::Set> MarkerIdsToJS(const std::unordered_set<MarkerId> &marker_ids);
  static v8::Local<v8::Object> SnapshotToJS(const std::unordered_map<MarkerId, Range> &snapshot);
  static Nan::Maybe<MarkerId> MarkerIdFromJS(Nan::MaybeLocal<Integer> maybe_id);
  static Nan::Maybe<bool> BoolFromJS(Nan::MaybeLocal<Boolean> maybe_boolean);
  static void Insert(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void SetExclusive(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Delete(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Splice(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GetStart(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GetEnd(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Compare(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindIntersecting(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindContaining(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindContainedIn(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindStartingIn(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindEndingIn(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Dump(const Nan::FunctionCallbackInfo<v8::Value> &info);
  MarkerIndexWrapper(v8::Local<v8::Number> seed);
  MarkerIndex marker_index;
};
