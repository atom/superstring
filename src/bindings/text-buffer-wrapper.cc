#include "text-buffer-wrapper.h"
#include <fstream>
#include "point-wrapper.h"
#include "range-wrapper.h"
#include "text-wrapper.h"
#include "noop.h"

using namespace v8;
using std::move;
using std::vector;

void TextBufferWrapper::init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("TextBuffer").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template->PrototypeTemplate();
  prototype_template->Set(Nan::New("delete").ToLocalChecked(), Nan::New<FunctionTemplate>(noop));
  prototype_template->Set(Nan::New("getTextInRange").ToLocalChecked(), Nan::New<FunctionTemplate>(get_text_in_range));
  prototype_template->Set(Nan::New("getText").ToLocalChecked(), Nan::New<FunctionTemplate>(get_text));
  prototype_template->Set(Nan::New("load").ToLocalChecked(), Nan::New<FunctionTemplate>(load));
  exports->Set(Nan::New("TextBuffer").ToLocalChecked(), constructor_template->GetFunction());
}

TextBufferWrapper::TextBufferWrapper(TextBuffer &&text_buffer) : text_buffer{std::move(text_buffer)} {}

void TextBufferWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  TextBufferWrapper *wrapper = new TextBufferWrapper(TextBuffer{});
  wrapper->Wrap(info.This());
}

void TextBufferWrapper::get_text_in_range(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto range = RangeWrapper::range_from_js(info[0]);
  if (range) {
    Local<String> result;
    auto text = text_buffer.text_in_range(*range);
    if (Nan::New<String>(text.content.data(), text.content.size()).ToLocal(&result)) {
      info.GetReturnValue().Set(result);
    }
  }
}

void TextBufferWrapper::get_text(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  Local<String> result;
  auto text = text_buffer.text();
  if (Nan::New<String>(text.content.data(), text.content.size()).ToLocal(&result)) {
    info.GetReturnValue().Set(result);
  }
}

void TextBufferWrapper::set_text_in_range(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto range = RangeWrapper::range_from_js(info[0]);
  auto text = TextWrapper::text_from_js(info[1]);
  if (range && text) {
    text_buffer.set_text_in_range(*range, move(*text));
  }
}

class TextBufferLoader : public Nan::AsyncProgressWorkerBase<size_t> {
  Nan::Callback *progress;
  TextBuffer *buffer;
  std::string file_name;

public:
  TextBufferLoader(Nan::Callback *completion, Nan::Callback *progress,
                   TextBuffer *buffer, std::string file_name) :
    AsyncProgressWorkerBase(completion),
    progress{progress},
    buffer{buffer},
    file_name{file_name} {}

  void Execute(const Nan::AsyncProgressWorkerBase<size_t>::ExecutionProgress &progress) {
    static size_t CHUNK_SIZE = 10 * 1024;
    std::ifstream file{file_name};
    auto beginning = file.tellg();
    file.seekg(0, std::ios::end);
    auto end = file.tellg();
    file.seekg(0);
    buffer->load(file, end - beginning, "UTF8", CHUNK_SIZE, [&progress](size_t percent_done) {
      progress.Send(&percent_done, 1);
    });
  }

  void HandleProgressCallback(const size_t *percent_done, size_t count) {
    Nan::HandleScope scope;
    v8::Local<v8::Value> argv[] = {Nan::New<Number>(static_cast<uint32_t>(*percent_done))};
    progress->Call(1, argv);
  }

  void HandleOKCallback() {
    callback->Call(0, nullptr);
  }
};

void TextBufferWrapper::load(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (Nan::To<String>(info[0]).ToLocal(&js_file_path)) {
    String::Utf8Value file_path(js_file_path);
    Nan::Callback *completion = new Nan::Callback(info[1].As<Function>());
    Nan::Callback *progress = new Nan::Callback(info[2].As<Function>());

    Nan::AsyncQueueWorker(new TextBufferLoader(
      completion,
      progress,
      &text_buffer,
      *file_path
    ));
  }
}
