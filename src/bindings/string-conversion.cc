#include "string-conversion.h"
#include "text.h"

using namespace v8;
using std::u16string;

optional<u16string> string_conversion::string_from_js(Local<Value> value) {
  Local<String> string;
  if (!Nan::To<String>(value).ToLocal(&string)) {
    Nan::ThrowTypeError("Expected a string.");
    return optional<u16string>{};
  }

  u16string result;
  result.resize(string->Length());
  string->Write(

    // Nan doesn't wrap this functionality
    #if NODE_MAJOR_VERSION >= 12
          Isolate::GetCurrent(),
    #endif

    reinterpret_cast<uint16_t *>(&result[0]),
    0,
    -1,
    String::WriteOptions::NO_NULL_TERMINATION
  );
  return result;
}

Local<String> string_conversion::string_to_js(const u16string &text, const char *failure_message) {
  Local<String> result;
  if (Nan::New<String>(
    reinterpret_cast<const uint16_t *>(text.data()),
    text.size()
  ).ToLocal(&result)) {
    return result;
  } else {
    if (failure_message == nullptr) failure_message = "Couldn't convert text to a String";
    Nan::ThrowError(failure_message);
    return Nan::New<String>("").ToLocalChecked();
  }
}

Local<String> string_conversion::char_to_js(const uint16_t c, const char *failure_message) {
  Local<String> result;
  if (Nan::New<String>(&c, 1).ToLocal(&result)) {
    return result;
  } else {
    if (failure_message == nullptr) failure_message = "Couldn't convert character to a String";
    Nan::ThrowError(failure_message);
    return Nan::New<String>("").ToLocalChecked();
  }
}
