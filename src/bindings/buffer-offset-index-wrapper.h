#include <nan.h>
#include "buffer-offset-index.h"

class BufferOffsetIndexWrapper : public Nan::ObjectWrap {
public:
  static void init(v8::Local<v8::Object> exports);

private:
  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void splice(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void character_index_for_position(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void position_for_character_index(const Nan::FunctionCallbackInfo<v8::Value> &info);

  BufferOffsetIndex buffer_offset_index;
};
