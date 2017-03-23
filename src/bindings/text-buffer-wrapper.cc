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
  prototype_template->Set(Nan::New("setTextInRange").ToLocalChecked(), Nan::New<FunctionTemplate>(set_text_in_range));
  prototype_template->Set(Nan::New("getText").ToLocalChecked(), Nan::New<FunctionTemplate>(get_text));
  prototype_template->Set(Nan::New("setText").ToLocalChecked(), Nan::New<FunctionTemplate>(set_text));
  prototype_template->Set(Nan::New("lineLengthForRow").ToLocalChecked(), Nan::New<FunctionTemplate>(line_length_for_row));
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

void TextBufferWrapper::set_text(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto text = TextWrapper::text_from_js(info[0]);
  if (text) {
    text_buffer.set_text(move(*text));
  }
}

void TextBufferWrapper::line_length_for_row(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto row = Nan::To<uint32_t>(info[0]);
  if (row.IsJust()) {
    info.GetReturnValue().Set(Nan::New<Number>(text_buffer.line_length_for_row(row.FromJust())));
  }
}

class TextBufferLoader : public Nan::AsyncProgressWorkerBase<size_t> {
  Nan::Callback *progress_callback;
  TextBuffer *buffer;
  std::string file_name;
  std::string encoding_name;
  bool result;

public:
  TextBufferLoader(Nan::Callback *completion, Nan::Callback *progress_callback,
                   TextBuffer *buffer, std::string &&file_name,
                   std::string &&encoding_name) :
    AsyncProgressWorkerBase(completion),
    progress_callback{progress_callback},
    buffer{buffer},
    file_name{file_name},
    encoding_name(encoding_name),
    result{false} {}

  void Execute(const Nan::AsyncProgressWorkerBase<size_t>::ExecutionProgress &progress) {
    static size_t CHUNK_SIZE = 10 * 1024;
    std::ifstream file{file_name};
    auto beginning = file.tellg();
    file.seekg(0, std::ios::end);
    auto end = file.tellg();
    file.seekg(0);
    result = buffer->load(
      file,
      end - beginning,
      encoding_name.c_str(),
      CHUNK_SIZE,
      [&progress](size_t percent_done) { progress.Send(&percent_done, 1); }
    );
  }

  void HandleProgressCallback(const size_t *percent_done, size_t count) {
    if (!progress_callback) return;
    Nan::HandleScope scope;
    v8::Local<v8::Value> argv[] = {Nan::New<Number>(static_cast<uint32_t>(*percent_done))};
    progress_callback->Call(1, argv);
  }

  void HandleOKCallback() {
    v8::Local<v8::Value> argv[] = {Nan::New<Boolean>(result)};
    callback->Call(1, argv);
  }
};

void TextBufferWrapper::load(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  std::string file_path = *String::Utf8Value(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  std::string encoding_name = *String::Utf8Value(info[1].As<String>());

  Nan::Callback *completion_callback = new Nan::Callback(info[2].As<Function>());

  Nan::Callback *progress_callback = nullptr;
  if (info[3]->IsFunction()) {
    progress_callback = new Nan::Callback(info[3].As<Function>());
  }

  Nan::AsyncQueueWorker(new TextBufferLoader(
    completion_callback,
    progress_callback,
    &text_buffer,
    move(file_path),
    move(encoding_name)
  ));
}
