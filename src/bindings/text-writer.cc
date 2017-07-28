#include "text-writer.h"

using std::string;
using std::move;
using std::u16string;
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

TextWriter::TextWriter(EncodingConversion &&conversion) : conversion{move(conversion)} {}

void TextWriter::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[0]).ToLocal(&js_encoding_name)) return;
  String::Utf8Value encoding_name(js_encoding_name);
  auto conversion = transcoding_from(*encoding_name);
  if (!conversion) {
    Nan::ThrowError((string("Invalid encoding name: ") + *encoding_name).c_str());
    return;
  }

  TextWriter *wrapper = new TextWriter(move(*conversion));
  wrapper->Wrap(info.This());
}

void TextWriter::write(const Nan::FunctionCallbackInfo<Value> &info) {
  auto writer = Nan::ObjectWrap::Unwrap<TextWriter>(info.This());

  if (info[0]->IsString()) {
    Local<String> js_chunk = info[0]->ToString();
    size_t size = writer->content.size();
    writer->content.resize(size + js_chunk->Length());
    js_chunk->Write(
      reinterpret_cast<uint16_t *>(&writer->content[0]) + size,
      0,
      -1,
      String::WriteOptions::NO_NULL_TERMINATION
    );
  } else if (info[0]->IsUint8Array()) {
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
    size_t bytes_written = writer->conversion.decode(
      writer->content,
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
    writer->conversion.decode(
      writer->content,
      writer->leftover_bytes.data(),
      writer->leftover_bytes.size(),
      true
    );
  }
}

u16string TextWriter::get_text() {
  return move(content);
}
