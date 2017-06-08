#include "text-wrapper.h"
#include "text.h"

using namespace v8;
using std::vector;

optional<Text::String> TextWrapper::string_from_js(Local<Value> value) {
  Local<String> string;
  if (!Nan::To<String>(value).ToLocal(&string)) {
    Nan::ThrowTypeError("Expected a string.");
    return optional<Text::String>{};
  }

  Text::String result(string->Length());
  string->Write(result.data(), 0, -1, String::WriteOptions::NO_NULL_TERMINATION);
  return result;
}

optional<Text> TextWrapper::text_from_js(Local<Value> value) {
  auto result = string_from_js(value);
  if (result) {
    return Text{move(*result)};
  } else {
    return optional<Text>{};
  }
}

Local<String> TextWrapper::string_to_js(const Text::String &text) {
  Local<String> result;
  if (Nan::New<String>(text.data(), text.size()).ToLocal(&result)) {
    return result;
  } else {
    Nan::ThrowError("Couldn't convert text to a String");
    return Nan::New<String>("").ToLocalChecked();
  }
}

Local<String> TextWrapper::text_to_js(const Text &text) {
  return string_to_js(text.content);
}
