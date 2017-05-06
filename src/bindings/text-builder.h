#ifndef SUPERSTRING_TEXT_BUILDER_H
#define SUPERSTRING_TEXT_BUILDER_H

#include "nan.h"
#include "text.h"

class TextBuilder : public Nan::ObjectWrap {
public:
  static void init(v8::Local<v8::Object> exports);
  TextBuilder(Text::EncodingConversion conversion);
  Text text;

private:
  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void write(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void end(const Nan::FunctionCallbackInfo<v8::Value> &info);

  Text::EncodingConversion conversion;
  std::vector<char> leftover_bytes;
};

#endif // SUPERSTRING_TEXT_BUILDER_H
