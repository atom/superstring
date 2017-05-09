#ifndef SUPERSTRING_TEXT_WRITER_H
#define SUPERSTRING_TEXT_WRITER_H

#include "nan.h"
#include "text.h"

class TextWriter : public Nan::ObjectWrap {
public:
  static void init(v8::Local<v8::Object> exports);
  TextWriter(Text::EncodingConversion conversion);
  Text text;

private:
  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void write(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void end(const Nan::FunctionCallbackInfo<v8::Value> &info);

  Text::EncodingConversion conversion;
  std::vector<char> leftover_bytes;
};

#endif // SUPERSTRING_TEXT_WRITER_H
