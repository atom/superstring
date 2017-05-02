#include "text-buffer-wrapper.h"
#include <fstream>
#include "point-wrapper.h"
#include "range-wrapper.h"
#include "text-wrapper.h"
#include "patch-wrapper.h"
#include "text-slice.h"
#include "text-diff.h"
#include "noop.h"
#include <sstream>
#include <iomanip>

using namespace v8;
using std::move;
using std::vector;

void TextBufferWrapper::init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("TextBuffer").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template->PrototypeTemplate();
  prototype_template->Set(Nan::New("delete").ToLocalChecked(), Nan::New<FunctionTemplate>(noop));
  prototype_template->Set(Nan::New("getLength").ToLocalChecked(), Nan::New<FunctionTemplate>(get_length));
  prototype_template->Set(Nan::New("getExtent").ToLocalChecked(), Nan::New<FunctionTemplate>(get_extent));
  prototype_template->Set(Nan::New("getTextInRange").ToLocalChecked(), Nan::New<FunctionTemplate>(get_text_in_range));
  prototype_template->Set(Nan::New("setTextInRange").ToLocalChecked(), Nan::New<FunctionTemplate>(set_text_in_range));
  prototype_template->Set(Nan::New("getText").ToLocalChecked(), Nan::New<FunctionTemplate>(get_text));
  prototype_template->Set(Nan::New("setText").ToLocalChecked(), Nan::New<FunctionTemplate>(set_text));
  prototype_template->Set(Nan::New("lineForRow").ToLocalChecked(), Nan::New<FunctionTemplate>(line_for_row));
  prototype_template->Set(Nan::New("lineLengthForRow").ToLocalChecked(), Nan::New<FunctionTemplate>(line_length_for_row));
  prototype_template->Set(Nan::New("lineEndingForRow").ToLocalChecked(), Nan::New<FunctionTemplate>(line_ending_for_row));
  prototype_template->Set(Nan::New("getLines").ToLocalChecked(), Nan::New<FunctionTemplate>(get_lines));
  prototype_template->Set(Nan::New("characterIndexForPosition").ToLocalChecked(), Nan::New<FunctionTemplate>(character_index_for_position));
  prototype_template->Set(Nan::New("positionForCharacterIndex").ToLocalChecked(), Nan::New<FunctionTemplate>(position_for_character_index));
  prototype_template->Set(Nan::New("isModified").ToLocalChecked(), Nan::New<FunctionTemplate>(is_modified));
  prototype_template->Set(Nan::New("load").ToLocalChecked(), Nan::New<FunctionTemplate>(load));
  prototype_template->Set(Nan::New("save").ToLocalChecked(), Nan::New<FunctionTemplate>(save));
  prototype_template->Set(Nan::New("loadSync").ToLocalChecked(), Nan::New<FunctionTemplate>(load_sync));
  prototype_template->Set(Nan::New("saveSync").ToLocalChecked(), Nan::New<FunctionTemplate>(save_sync));
  prototype_template->Set(Nan::New("serializeChanges").ToLocalChecked(), Nan::New<FunctionTemplate>(serialize_changes));
  prototype_template->Set(Nan::New("deserializeChanges").ToLocalChecked(), Nan::New<FunctionTemplate>(deserialize_changes));
  prototype_template->Set(Nan::New("baseTextDigest").ToLocalChecked(), Nan::New<FunctionTemplate>(base_text_digest));
  prototype_template->Set(Nan::New("search").ToLocalChecked(), Nan::New<FunctionTemplate>(search));
  prototype_template->Set(Nan::New("searchSync").ToLocalChecked(), Nan::New<FunctionTemplate>(search_sync));
  prototype_template->Set(Nan::New("getDotGraph").ToLocalChecked(), Nan::New<FunctionTemplate>(dot_graph));
  exports->Set(Nan::New("TextBuffer").ToLocalChecked(), constructor_template->GetFunction());
}

void TextBufferWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  TextBufferWrapper *wrapper = new TextBufferWrapper();
  if (info.Length() > 0 && info[0]->IsString()) {
    auto text = TextWrapper::text_from_js(info[0]);
    if (text) {
      wrapper->text_buffer.reset(move(*text));
    }
  }
  wrapper->Wrap(info.This());
}

void TextBufferWrapper::get_length(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(Nan::New<Number>(text_buffer.size()));
}

void TextBufferWrapper::get_extent(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(PointWrapper::from_point(text_buffer.extent()));
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

void TextBufferWrapper::line_for_row(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto maybe_row = Nan::To<uint32_t>(info[0]);

  if (maybe_row.IsJust()) {
    uint32_t row = maybe_row.FromJust();
    if (row <= text_buffer.extent().row) {
      Local<String> result;
      auto text = text_buffer.text_in_range({{row, 0}, {row, UINT32_MAX}});
      if (Nan::New<String>(text.content.data(), text.content.size()).ToLocal(&result)) {
        info.GetReturnValue().Set(result);
      }
    }
  }
}

void TextBufferWrapper::line_length_for_row(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto maybe_row = Nan::To<uint32_t>(info[0]);
  if (maybe_row.IsJust()) {
    uint32_t row = maybe_row.FromJust();
    if (row <= text_buffer.extent().row) {
      info.GetReturnValue().Set(Nan::New<Number>(text_buffer.line_length_for_row(row)));
    }
  }
}

void TextBufferWrapper::line_ending_for_row(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto maybe_row = Nan::To<uint32_t>(info[0]);
  if (maybe_row.IsJust()) {
    uint32_t row = maybe_row.FromJust();
    if (row <= text_buffer.extent().row) {
      info.GetReturnValue().Set(Nan::New<String>(text_buffer.line_ending_for_row(row)).ToLocalChecked());
    }
  }
}

void TextBufferWrapper::get_lines(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto result = Nan::New<Array>();

  for (uint32_t row = 0, row_count = text_buffer.extent().row + 1; row < row_count; row++) {
    Local<String> line;
    auto text = text_buffer.text_in_range({{row, 0}, {row, UINT32_MAX}});
    if (!Nan::New<String>(text.content.data(), text.content.size()).ToLocal(&line)) return;
    result->Set(row, line);
  }

  info.GetReturnValue().Set(result);
}

void TextBufferWrapper::character_index_for_position(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto position = PointWrapper::point_from_js(info[0]);
  if (position) {
    info.GetReturnValue().Set(
      Nan::New<Number>(text_buffer.clip_position(*position).offset)
    );
  }
}

void TextBufferWrapper::position_for_character_index(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto maybe_offset = Nan::To<int64_t>(info[0]);
  if (maybe_offset.IsJust()) {
    int64_t offset = maybe_offset.FromJust();
    info.GetReturnValue().Set(
      PointWrapper::from_point(text_buffer.position_for_offset(
        std::max<int64_t>(0, offset)
      ))
    );
  }
}

void TextBufferWrapper::search_sync(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  Local<String> js_pattern;
  if (Nan::To<String>(info[0]).ToLocal(&js_pattern)) {
    vector<uint16_t> pattern(js_pattern->Length());
    js_pattern->Write(pattern.data(), 0, -1, String::WriteOptions::NO_NULL_TERMINATION);
    auto result = text_buffer.search(pattern.data(), pattern.size());
    if (!result.error_message.empty()) {
      Nan::ThrowError(Nan::New<String>(
        reinterpret_cast<const uint16_t *>(result.error_message.c_str()),
        result.error_message.size()).ToLocalChecked()
      );
    } else if (result.range) {
      info.GetReturnValue().Set(RangeWrapper::from_range(*result.range));
    } else {
      info.GetReturnValue().Set(Nan::Null());
    }
  }
}

void TextBufferWrapper::search(const Nan::FunctionCallbackInfo<Value> &info) {
  class TextBufferSearcher : public Nan::AsyncWorker {
    const TextBuffer::Snapshot *snapshot;
    vector<uint16_t> pattern;
    TextBuffer::SearchResult result;

  public:
    TextBufferSearcher(Nan::Callback *completion_callback,
                       const TextBuffer::Snapshot *snapshot,
                       vector<uint16_t> &&pattern) :
      AsyncWorker(completion_callback),
      snapshot{snapshot},
      pattern{pattern} {}

    void Execute() {
      result = snapshot->search(pattern.data(), pattern.size());
    }

    void HandleOKCallback() {
      delete snapshot;
      if (!result.error_message.empty()) {
        v8::Local<v8::Value> argv[] = {
          Nan::Error(Nan::New<String>(
            reinterpret_cast<const uint16_t *>(result.error_message.c_str()),
            result.error_message.size()).ToLocalChecked()
          )
        };
        callback->Call(1, argv);
      } else if (result.range) {
        v8::Local<v8::Value> argv[] = {Nan::Null(), RangeWrapper::from_range(*result.range)};
        callback->Call(2, argv);
      } else {
        v8::Local<v8::Value> argv[] = {Nan::Null(), Nan::Null()};
        callback->Call(2, argv);
      }
    }
  };

  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_pattern;
  if (Nan::To<String>(info[0]).ToLocal(&js_pattern)) {
    vector<uint16_t> pattern(js_pattern->Length());
    js_pattern->Write(pattern.data(), 0, -1, String::WriteOptions::NO_NULL_TERMINATION);
    Nan::AsyncQueueWorker(new TextBufferSearcher(
      new Nan::Callback(info[1].As<Function>()),
      text_buffer.create_snapshot(),
      move(pattern)
    ));
  }
}

void TextBufferWrapper::is_modified(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(Nan::New<Boolean>(text_buffer.is_modified()));
}

void TextBufferWrapper::load_sync(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  std::string file_path = *String::Utf8Value(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  std::string encoding_name = *String::Utf8Value(info[1].As<String>());

  static size_t CHUNK_SIZE = 10 * 1024;
  std::ifstream file{file_path};
  auto beginning = file.tellg();
  file.seekg(0, std::ios::end);
  auto end = file.tellg();
  file.seekg(0);
  auto text = Text::build(
    file,
    end - beginning,
    encoding_name.c_str(),
    CHUNK_SIZE,
    [](size_t percent_done) {}
  );

  if (text) {
    text_buffer.flush_changes();
    Patch patch = text_diff(text_buffer.base_text(), *text);
    text_buffer.reset(move(*text));
    info.GetReturnValue().Set(PatchWrapper::from_patch(move(patch)));
  } else {
    info.GetReturnValue().Set(Nan::False());
  }
}

void TextBufferWrapper::load(const Nan::FunctionCallbackInfo<Value> &info) {
  class Worker : public Nan::AsyncProgressWorkerBase<size_t> {
    Nan::Callback *progress_callback;
    TextBuffer *buffer;
    std::string file_name;
    std::string encoding_name;
    optional<Text> loaded_text;

  public:
    Worker(Nan::Callback *completion_callback, Nan::Callback *progress_callback,
           TextBuffer *buffer, std::string &&file_name, std::string &&encoding_name) :
      AsyncProgressWorkerBase(completion_callback),
      progress_callback{progress_callback},
      buffer{buffer},
      file_name{file_name},
      encoding_name(encoding_name) {}

    void Execute(const Nan::AsyncProgressWorkerBase<size_t>::ExecutionProgress &progress) {
      static size_t CHUNK_SIZE = 10 * 1024;
      std::ifstream file{file_name};
      auto beginning = file.tellg();
      file.seekg(0, std::ios::end);
      auto end = file.tellg();
      file.seekg(0);
      loaded_text = Text::build(
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
      if (loaded_text) {
        buffer->flush_changes();
        Patch patch = text_diff(buffer->base_text(), *loaded_text);
        buffer->reset(move(*loaded_text));
        v8::Local<v8::Value> argv[] = {Nan::Null(), PatchWrapper::from_patch(move(patch))};
        callback->Call(2, argv);
      } else {
        v8::Local<v8::Value> argv[] = {Nan::Error(("Invalid encoding name: " + encoding_name).c_str())};
        callback->Call(1, argv);
      }
    }
  };

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

  Nan::AsyncQueueWorker(new Worker(
    completion_callback,
    progress_callback,
    &text_buffer,
    move(file_path),
    move(encoding_name)
  ));
}

void TextBufferWrapper::save_sync(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  std::string file_path = *String::Utf8Value(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  std::string encoding_name = *String::Utf8Value(info[1].As<String>());

  static size_t CHUNK_SIZE = 10 * 1024;
  std::ofstream file{file_path};
  for (TextSlice &chunk : text_buffer.chunks()) {
    if (!Text::write(file, encoding_name.c_str(), CHUNK_SIZE, chunk)) {
      info.GetReturnValue().Set(Nan::False());
      return;
    }
  }

  text_buffer.flush_changes();
  info.GetReturnValue().Set(Nan::True());
}

class TextBufferSaver : public Nan::AsyncWorker {
  TextBuffer::Snapshot *snapshot;
  std::string file_name;
  std::string encoding_name;
  bool result;

public:
  TextBufferSaver(Nan::Callback *completion_callback, TextBuffer::Snapshot *snapshot,
                  std::string &&file_name, std::string &&encoding_name) :
    AsyncWorker(completion_callback),
    snapshot{snapshot},
    file_name{file_name},
    encoding_name(encoding_name),
    result{true} {}

  void Execute() {
    static size_t CHUNK_SIZE = 10 * 1024;
    std::ofstream file{file_name};
    for (TextSlice &chunk : snapshot->chunks()) {
      if (!Text::write(file, encoding_name.c_str(), CHUNK_SIZE, chunk)) {
        result = false;
        return;
      }
    }
  }

  void HandleOKCallback() {
    snapshot->flush_preceding_changes();
    delete snapshot;
    v8::Local<v8::Value> argv[] = {Nan::New<Boolean>(result)};
    callback->Call(1, argv);
  }
};

void TextBufferWrapper::save(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  std::string file_path = *String::Utf8Value(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  std::string encoding_name = *String::Utf8Value(info[1].As<String>());

  Nan::Callback *completion_callback = new Nan::Callback(info[2].As<Function>());
  Nan::AsyncQueueWorker(new TextBufferSaver(
    completion_callback,
    text_buffer.create_snapshot(),
    move(file_path),
    move(encoding_name)
  ));
}

void TextBufferWrapper::serialize_changes(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  static vector<uint8_t> output;
  output.clear();
  Serializer serializer(output);
  text_buffer.serialize_changes(serializer);
  Local<Object> result;
  if (Nan::CopyBuffer(reinterpret_cast<char *>(output.data()), output.size()).ToLocal(&result)) {
    info.GetReturnValue().Set(result);
  }
}

void TextBufferWrapper::deserialize_changes(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  if (info[0]->IsUint8Array()) {
    auto *data = node::Buffer::Data(info[0]);
    static vector<uint8_t> input;
    input.assign(data, data + node::Buffer::Length(info[0]));
    Deserializer deserializer(input);
    text_buffer.deserialize_changes(deserializer);
  }
}

void TextBufferWrapper::base_text_digest(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  std::stringstream stream;
  stream <<
    std::setfill('0') <<
    std::setw(2 * sizeof(size_t)) <<
    std::hex <<
    text_buffer.base_text_digest();
  Local<String> result;
  if (Nan::New(stream.str()).ToLocal(&result)) {
    info.GetReturnValue().Set(result);
  }
}

void TextBufferWrapper::dot_graph(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(Nan::New<String>(text_buffer.get_dot_graph()).ToLocalChecked());
}