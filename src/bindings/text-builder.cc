#include "text-builder.h"

using std::string;
using namespace v8;

void TextBuilder::init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("TextBuilder").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template->PrototypeTemplate();
  prototype_template->Set(Nan::New("write").ToLocalChecked(), Nan::New<FunctionTemplate>(write));
  prototype_template->Set(Nan::New("end").ToLocalChecked(), Nan::New<FunctionTemplate>(end));
  exports->Set(Nan::New("TextBuilder").ToLocalChecked(), constructor_template->GetFunction());
}

TextBuilder::TextBuilder(Text::EncodingConversion conversion) : conversion{conversion} {}

void TextBuilder::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[0]).ToLocal(&js_encoding_name)) return;
  String::Utf8Value encoding_name(js_encoding_name);
  auto conversion = Text::transcoding_from(*encoding_name);
  if (!conversion) {
    Nan::ThrowError((string("Invalid encoding name: ") + *encoding_name).c_str());
    return;
  }

  TextBuilder *wrapper = new TextBuilder(*conversion);
  wrapper->Wrap(info.This());
}

void TextBuilder::write(const Nan::FunctionCallbackInfo<Value> &info) {
  auto builder = Nan::ObjectWrap::Unwrap<TextBuilder>(info.This());

  if (info[0]->IsUint8Array()) {
    auto *data = node::Buffer::Data(info[0]);
    size_t length = node::Buffer::Length(info[0]);
    if (!builder->leftover_bytes.empty()) {
      builder->leftover_bytes.insert(
        builder->leftover_bytes.end(),
        data,
        data + length
      );
      data = builder->leftover_bytes.data();
      length = builder->leftover_bytes.size();
    }
    size_t bytes_written = builder->text.append(builder->conversion, data, length);
    if (bytes_written < length) {
      builder->leftover_bytes.assign(data + bytes_written, data + length);
    } else {
      builder->leftover_bytes.clear();
    }
  }
}

void TextBuilder::end(const Nan::FunctionCallbackInfo<Value> &info) {
  auto builder = Nan::ObjectWrap::Unwrap<TextBuilder>(info.This());
  if (!builder->leftover_bytes.empty()) {
    builder->text.append(
      builder->conversion,
      builder->leftover_bytes.data(),
      builder->leftover_bytes.size(),
      true
    );
  }
}