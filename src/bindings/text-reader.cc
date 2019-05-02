#include "text-slice.h"
#include "text-reader.h"
#include "encoding-conversion.h"
#include "text-buffer-wrapper.h"

using std::move;
using std::string;
using namespace v8;

void TextReader::init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("TextReader").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template->PrototypeTemplate();
  prototype_template->Set(Nan::New("read").ToLocalChecked(), Nan::New<FunctionTemplate>(read));
  prototype_template->Set(Nan::New("end").ToLocalChecked(), Nan::New<FunctionTemplate>(end));
  prototype_template->Set(Nan::New("destroy").ToLocalChecked(), Nan::New<FunctionTemplate>(destroy));
  exports->Set(Nan::New("TextReader").ToLocalChecked(), Nan::GetFunction(constructor_template).ToLocalChecked());
}

TextReader::TextReader(Local<Object> js_buffer,
                       TextBuffer::Snapshot *snapshot,
                       EncodingConversion &&conversion) :
  snapshot{snapshot},
  slices{snapshot->chunks()},
  slice_index{0},
  text_offset{slices[0].start_offset()},
  conversion{move(conversion)} {
  js_text_buffer.Reset(Isolate::GetCurrent(), js_buffer);
}

TextReader::~TextReader() {
  if (snapshot) delete snapshot;
}

void TextReader::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  Local<Object> js_text_buffer;
  if (!Nan::To<Object>(info[0]).ToLocal(&js_text_buffer)) return;
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(js_text_buffer)->text_buffer;
  auto snapshot = text_buffer.create_snapshot();

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  Nan::Utf8String encoding_name(js_encoding_name);
  auto conversion = transcoding_to(*encoding_name);
  if (!conversion) {
    Nan::ThrowError((string("Invalid encoding name: ") + *encoding_name).c_str());
    return;
  }

  TextReader *reader = new TextReader(js_text_buffer, snapshot, move(*conversion));
  reader->Wrap(info.This());
}

void TextReader::read(const Nan::FunctionCallbackInfo<Value> &info) {
  TextReader *reader = Nan::ObjectWrap::Unwrap<TextReader>(Nan::To<Object>(info.This()).ToLocalChecked());

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
    size_t end_offset = slice.end_offset();
    size_t bytes_written = reader->conversion.encode(
      slice.text->content,
      &reader->text_offset,
      end_offset,
      buffer + total_bytes_written,
      buffer_length - total_bytes_written
    );
    if (bytes_written == 0) break;
    total_bytes_written += bytes_written;
    if (reader->text_offset == end_offset) {
      reader->slice_index++;
      if (reader->slice_index == reader->slices.size()) break;
      reader->text_offset = reader->slices[reader->slice_index].start_offset();
    }
  }

  info.GetReturnValue().Set(Nan::New<Number>(total_bytes_written));
}

void TextReader::end(const Nan::FunctionCallbackInfo<Value> &info) {
  TextReader *reader = Nan::ObjectWrap::Unwrap<TextReader>(Nan::To<Object>(info.This()).ToLocalChecked());
  if (reader->snapshot) {
    reader->snapshot->flush_preceding_changes();
    delete reader->snapshot;
    reader->snapshot = nullptr;
  }
}

void TextReader::destroy(const Nan::FunctionCallbackInfo<Value> &info) {
  TextReader *reader = Nan::ObjectWrap::Unwrap<TextReader>(Nan::To<Object>(info.This()).ToLocalChecked());
  if (reader->snapshot) {
    delete reader->snapshot;
    reader->snapshot = nullptr;
  }
}
