#include <nan.h>
#include "patch.h"

class PatchWrapper : public Nan::ObjectWrap {
 public:
  static void Init(v8::Local<v8::Object> exports);

 private:
  PatchWrapper(Patch &&patch);
  static void New(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Splice(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void SpliceOld(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Invert(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GetHunks(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GetHunksInOldRange(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GetHunksInNewRange(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void HunkForOldPosition(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void HunkForNewPosition(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Serialize(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Deserialize(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Compose(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void PrintDotGraph(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void GetHunkCount(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Rebalance(const Nan::FunctionCallbackInfo<v8::Value> &info);

  Patch patch;
};
