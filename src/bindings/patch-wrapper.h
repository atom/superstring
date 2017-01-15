#include <nan.h>
#include "patch.h"

class PatchWrapper : public Nan::ObjectWrap {
 public:
  static void init(v8::Local<v8::Object> exports);

 private:
  PatchWrapper(Patch &&patch);
  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void splice(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void splice_old(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void copy(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void invert(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_hunks(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_hunks_in_old_range(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_hunks_in_new_range(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void hunk_for_old_position(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void hunk_for_new_position(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void serialize(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void deserialize(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void compose(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_dot_graph(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_json(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_hunk_count(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void rebalance(const Nan::FunctionCallbackInfo<v8::Value> &info);

  Patch patch;
};
