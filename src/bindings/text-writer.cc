#include "text-writer.h"

using std::string;
using namespace v8;

void TextWriter::init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("TextWriter").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template->PrototypeTemplate();
  prototype_template->Set(Nan::New("write").ToLocalChecked(), Nan::New<FunctionTemplate>(write));
  prototype_template->Set(Nan::New("end").ToLocalChecked(), Nan::New<FunctionTemplate>(end));
  exports->Set(Nan::New("TextWriter").ToLocalChecked(), constructor_template->GetFunction());
}

TextWriter::TextWriter(Text::EncodingConversion conversion) : conversion{conversion} {}

void TextWriter::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[0]).ToLocal(&js_encoding_name)) return;
  String::Utf8Value encoding_name(js_encoding_name);
  auto conversion = Text::transcoding_from(*encoding_name);
  if (!conversion) {
    Nan::ThrowError((string("Invalid encoding name: ") + *encoding_name).c_str());
    return;
  }

  TextWriter *wrapper = new TextWriter(*conversion);
  wrapper->Wrap(info.This());
}

void TextWriter::write(const Nan::FunctionCallbackInfo<Value> &info) {
  auto writer = Nan::ObjectWrap::Unwrap<TextWriter>(info.This());

  if (info[0]->IsUint8Array()) {
    auto *data = node::Buffer::Data(info[0]);
    size_t length = node::Buffer::Length(info[0]);
    if (!writer->leftover_bytes.empty()) {
      writer->leftover_bytes.insert(
        writer->leftover_bytes.end(),
        data,
        data + length
      );
      data = writer->leftover_bytes.data();
      length = writer->leftover_bytes.size();
    }
    size_t bytes_written = writer->text.decode(
      writer->conversion,
      data,
      length
    );
    if (bytes_written < length) {
      writer->leftover_bytes.assign(data + bytes_written, data + length);
    } else {
      writer->leftover_bytes.clear();
    }
  }
}

void TextWriter::end(const Nan::FunctionCallbackInfo<Value> &info) {
  auto writer = Nan::ObjectWrap::Unwrap<TextWriter>(info.This());
  if (!writer->leftover_bytes.empty()) {
    writer->text.decode(
      writer->conversion,
      writer->leftover_bytes.data(),
      writer->leftover_bytes.size(),
      true
    );
  }
}