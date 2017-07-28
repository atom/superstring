#ifndef SUPERSTRING_TEXT_WRAPPER_H
#define SUPERSTRING_TEXT_WRAPPER_H

#include <string>
#include "nan.h"
#include "optional.h"
#include "text.h"

class TextWrapper : public Nan::ObjectWrap {
 public:
  static v8::Local<v8::String> string_to_js(const std::u16string &);
  static v8::Local<v8::String> text_to_js(const Text &);
  static optional<std::u16string> string_from_js(v8::Local<v8::Value>);
  static optional<Text> text_from_js(v8::Local<v8::Value>);
};

#endif // SUPERSTRING_TEXT_WRAPPER_H
