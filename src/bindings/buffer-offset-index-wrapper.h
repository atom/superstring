#include <nan.h>
#include "buffer-offset-index.h"

class BufferOffsetIndexWrapper : public Nan::ObjectWrap {
public:
  static void Init(v8::Local<v8::Object> exports);

private:
  static void New(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void Splice(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void CharacterIndexForPosition(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void PositionForCharacterIndex(const Nan::FunctionCallbackInfo<v8::Value> &info);

  BufferOffsetIndex buffer_offset_index;
};
