#ifndef SUPERSTRING_TEXT_BUFFER_WRAPPER_H
#define SUPERSTRING_TEXT_BUFFER_WRAPPER_H

#include "nan.h"
#include "text-buffer.h"

class TextBufferWrapper : public Nan::ObjectWrap {
public:
  static void init(v8::Local<v8::Object> exports);

private:
  TextBufferWrapper(TextBuffer &&text_buffer);

  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_text(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_text_in_range(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void set_text(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void set_text_in_range(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void line_length_for_row(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void load(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void save(const Nan::FunctionCallbackInfo<v8::Value> &info);

  TextBuffer text_buffer;
};

#endif // SUPERSTRING_TEXT_BUFFER_WRAPPER_H
