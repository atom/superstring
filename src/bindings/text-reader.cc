#include "text-slice.h"
#include "text-reader.h"
#include "text-buffer-wrapper.h"

using std::string;
using namespace v8;

void TextReader::init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("TextBuilder").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template->PrototypeTemplate();
  prototype_template->Set(Nan::New("read").ToLocalChecked(), Nan::New<FunctionTemplate>(read));
  exports->Set(Nan::New("TextReader").ToLocalChecked(), constructor_template->GetFunction());
}

TextReader::TextReader(TextBuffer::Snapshot *snapshot,
                       Text::EncodingConversion conversion) :
  snapshot{snapshot},
  slices{snapshot->chunks()},
  slice_index{0},
  slice_offset{0},
  conversion{conversion} {}

TextReader::~TextReader() {
  delete snapshot;
}

void TextReader::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info[0]->ToObject())->text_buffer;
  auto snapshot = text_buffer.create_snapshot();

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  String::Utf8Value encoding_name(js_encoding_name);
  auto conversion = Text::transcoding_to(*encoding_name);
  if (!conversion) {
    Nan::ThrowError((string("Invalid encoding name: ") + *encoding_name).c_str());
    return;
  }

  TextReader *reader = new TextReader(snapshot, *conversion);
  reader->Wrap(info.This());
}

void TextReader::read(const Nan::FunctionCallbackInfo<Value> &info) {
  TextReader *reader = Nan::ObjectWrap::Unwrap<TextReader>(info.This()->ToObject());

  if (!info[0]->IsUint8Array()) {
    Nan::ThrowError("Expected a buffer");
    return;
  }

  char *buffer = node::Buffer::Data(info[0]);
  size_t buffer_length = node::Buffer::Length(info[0]);
  size_t total_bytes_written = 0;

  for (;;) {
    if (reader->slice_index == reader->slices.size()) break;
    TextSlice &slice = reader->slices[reader->slice_index];
    size_t bytes_written = Text::write(
      slice,
      reader->conversion,
      &reader->slice_offset,
      buffer + total_bytes_written,
      buffer_length - total_bytes_written
    );
    if (bytes_written == 0) break;
    total_bytes_written += bytes_written;
    if (reader->slice_offset == slice.size()) {
      reader->slice_index++;
      reader->slice_offset = 0;
    }
  }

  info.GetReturnValue().Set(Nan::New<Number>(total_bytes_written));
}