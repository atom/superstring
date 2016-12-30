#include "nan.h"
#include "marker-index.h"
#include "optional.h"
#include "range.h"

class MarkerIndexWrapper : public Nan::ObjectWrap {
public:
  static void Init(v8::Local<v8::Object> exports);

private:
  static void New(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GenerateRandomNumber(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static bool IsFinite(v8::Local<v8::Integer> number);
  static v8::Local<v8::Set> MarkerIdsToJS(const MarkerIndex::MarkerIdSet &marker_ids);
  static v8::Local<v8::Object> SnapshotToJS(const std::unordered_map<MarkerIndex::MarkerId, Range> &snapshot);
  static optional<MarkerIndex::MarkerId> MarkerIdFromJS(v8::Local<v8::Value> value);
  static optional<unsigned> UnsignedFromJS(v8::Local<v8::Value> value);
  static optional<bool> BoolFromJS(v8::Local<v8::Value> value);
  static void Insert(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void SetExclusive(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Delete(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Splice(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GetStart(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GetEnd(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GetRange(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Compare(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindIntersecting(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindContaining(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindContainedIn(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindStartingIn(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindStartingAt(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindEndingIn(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void FindEndingAt(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Dump(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GetDotGraph(const Nan::FunctionCallbackInfo<v8::Value> &info);
  MarkerIndex marker_index;
};
