#include "text-wrapper.h"
#include "text.h"

using namespace v8;
using std::u16string;

optional<u16string> TextWrapper::string_from_js(Local<Value> value) {
  Local<String> string;
  if (!Nan::To<String>(value).ToLocal(&string)) {
    Nan::ThrowTypeError("Expected a string.");
    return optional<u16string>{};
  }

  u16string result;
  result.resize(string->Length());
  string->Write(
    reinterpret_cast<uint16_t *>(&result[0]),
    0,
    -1,
    String::WriteOptions::NO_NULL_TERMINATION
  );
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

Local<String> TextWrapper::string_to_js(const u16string &text) {
  Local<String> result;
  if (Nan::New<String>(reinterpret_cast<const uint16_t *>(text.data()), text.size()).ToLocal(&result)) {
    return result;
  } else {
    Nan::ThrowError("Couldn't convert text to a String");
    return Nan::New<String>("").ToLocalChecked();
  }
}

Local<String> TextWrapper::text_to_js(const Text &text) {
  return string_to_js(text.content);
}
