#include "text-buffer.h"
#include "text-buffer-wrapper.h"
#include "text-buffer-snapshot-wrapper.h"

using namespace v8;

static Nan::Persistent<v8::Function> snapshot_wrapper_constructor;

void TextBufferSnapshotWrapper::init() {
  auto class_name = Nan::New("Snapshot").ToLocalChecked();

  auto constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(class_name);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  const auto &prototype_template = constructor_template->PrototypeTemplate();
  prototype_template->Set(Nan::New("destroy").ToLocalChecked(), Nan::New<FunctionTemplate>(destroy));

  snapshot_wrapper_constructor.Reset(constructor_template->GetFunction());
}

TextBufferSnapshotWrapper::TextBufferSnapshotWrapper(Local<Object> js_buffer, void *snapshot) :
  snapshot{snapshot} {
  slices_ = reinterpret_cast<TextBuffer::Snapshot *>(snapshot)->primitive_chunks();
  js_text_buffer.Reset(Isolate::GetCurrent(), js_buffer);
}

TextBufferSnapshotWrapper::~TextBufferSnapshotWrapper() {
  if (snapshot) {
    delete reinterpret_cast<TextBuffer::Snapshot *>(snapshot);
  }
}

Local<Value> TextBufferSnapshotWrapper::new_instance(Local<Object> js_buffer, void *snapshot) {
  Local<Object> result;
  if (Nan::NewInstance(Nan::New(snapshot_wrapper_constructor)).ToLocal(&result)) {
    (new TextBufferSnapshotWrapper(js_buffer, snapshot))->Wrap(result);
    return result;
  } else {
    return Nan::Null();
  }
}

void TextBufferSnapshotWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  info.GetReturnValue().Set(Nan::Null());
}

void TextBufferSnapshotWrapper::destroy(const Nan::FunctionCallbackInfo<Value> &info) {
  auto reader = Nan::ObjectWrap::Unwrap<TextBufferSnapshotWrapper>(info.This()->ToObject());
  if (reader->snapshot) {
    delete reinterpret_cast<TextBuffer::Snapshot *>(reader->snapshot);
    reader->snapshot = nullptr;
  }
}
