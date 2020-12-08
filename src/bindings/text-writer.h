#ifndef SUPERSTRING_TEXT_WRITER_H
#define SUPERSTRING_TEXT_WRITER_H

#include <nan.h>
#include "text.h"
#include "encoding-conversion.h"

class TextWriter : public Nan::ObjectWrap {
public:
  static void init(v8::Local<v8::Object> exports);
  TextWriter(EncodingConversion &&conversion);
  std::u16string get_text();

private:
  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void write(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void end(const Nan::FunctionCallbackInfo<v8::Value> &info);

  EncodingConversion conversion;
  std::vector<char> leftover_bytes;
  std::u16string content;
};

#endif // SUPERSTRING_TEXT_WRITER_H
