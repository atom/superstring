#include "text-wrapper.h"
#include "text.h"

using namespace v8;
using std::vector;

optional<Text> TextWrapper::text_from_js(Local<Value> value) {
  Local<String> string;
  if (!Nan::To<String>(value).ToLocal(&string)) {
    Nan::ThrowTypeError("Expected a string.");
    return optional<Text> {};
  }

  vector<uint16_t> content(string->Length());
  string->Write(content.data(), 0, -1, String::WriteOptions::NO_NULL_TERMINATION);
  return optional<Text> {move(content)};
}

Local<String> TextWrapper::text_to_js(const Text &text) {
  Local<String> result;
  if (Nan::New<String>(text.data(), text.size()).ToLocal(&result)) {
    return result;
  } else {
    Nan::ThrowError("Couldn't convert text to a String");
    return Nan::New<String>("").ToLocalChecked();
  }
}
