#include "text-buffer-wrapper.h"
#include <fstream>
#include "point-wrapper.h"
#include "range-wrapper.h"
#include "text-wrapper.h"
#include "patch-wrapper.h"
#include "text-writer.h"
#include "text-slice.h"
#include "text-diff.h"
#include "noop.h"
#include <sstream>
#include <iomanip>

using namespace v8;
using std::move;
using std::string;
using std::u16string;
using std::vector;

static size_t CHUNK_SIZE = 10 * 1024;

class RegexWrapper : public Nan::ObjectWrap {
public:
  Regex regex;
  static Nan::Persistent<Function> constructor;
  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info) {}

  RegexWrapper(Regex &&regex) : regex{move(regex)} {}

  static const Regex *regex_from_js(const Local<Value> &value) {
    Local<String> js_pattern;
    Local<RegExp> js_regex;
    Local<String> cache_key = Nan::New("__textBufferRegex").ToLocalChecked();

    if (value->IsString()) {
      js_pattern = Local<String>::Cast(value);
    } else if (value->IsRegExp()) {
      js_regex = Local<RegExp>::Cast(value);
      Local<Value> stored_regex = js_regex->Get(cache_key);
      if (!stored_regex->IsUndefined()) {
        return &Nan::ObjectWrap::Unwrap<RegexWrapper>(stored_regex->ToObject())->regex;
      }
      js_pattern = js_regex->GetSource();
    } else {
      Nan::ThrowTypeError("Argument must be a RegExp");
      return nullptr;
    }

    u16string error_message;
    vector<uint16_t> pattern(js_pattern->Length());
    js_pattern->Write(pattern.data(), 0, -1, String::WriteOptions::NO_NULL_TERMINATION);
    Regex regex(pattern.data(), pattern.size(), &error_message);
    if (!error_message.empty()) {
      Nan::ThrowError(Nan::New<String>(
        reinterpret_cast<const uint16_t *>(error_message.c_str()),
        error_message.size()
      ).ToLocalChecked());
      return nullptr;
    }

    Local<Object> result;
    if (!Nan::New(constructor)->NewInstance(Nan::GetCurrentContext()).ToLocal(&result)) {
      Nan::ThrowError("Could not create regex wrapper");
      return nullptr;
    }

    auto regex_wrapper = new RegexWrapper(move(regex));
    regex_wrapper->Wrap(result);
    if (!js_regex.IsEmpty()) js_regex->Set(cache_key, result);
    return &regex_wrapper->regex;
  }

  static void init() {
    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
    constructor_template->SetClassName(Nan::New<String>("TextBufferRegex").ToLocalChecked());
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    constructor.Reset(constructor_template->GetFunction());
  }
};

Nan::Persistent<Function> RegexWrapper::constructor;

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
  prototype_template->Set(Nan::New("reload").ToLocalChecked(), Nan::New<FunctionTemplate>(reload));
  prototype_template->Set(Nan::New("save").ToLocalChecked(), Nan::New<FunctionTemplate>(save));
  prototype_template->Set(Nan::New("loadSync").ToLocalChecked(), Nan::New<FunctionTemplate>(load_sync));
  prototype_template->Set(Nan::New("saveSync").ToLocalChecked(), Nan::New<FunctionTemplate>(save_sync));
  prototype_template->Set(Nan::New("serializeChanges").ToLocalChecked(), Nan::New<FunctionTemplate>(serialize_changes));
  prototype_template->Set(Nan::New("deserializeChanges").ToLocalChecked(), Nan::New<FunctionTemplate>(deserialize_changes));
  prototype_template->Set(Nan::New("baseTextDigest").ToLocalChecked(), Nan::New<FunctionTemplate>(base_text_digest));
  prototype_template->Set(Nan::New("search").ToLocalChecked(), Nan::New<FunctionTemplate>(search));
  prototype_template->Set(Nan::New("searchSync").ToLocalChecked(), Nan::New<FunctionTemplate>(search_sync));
  prototype_template->Set(Nan::New("getDotGraph").ToLocalChecked(), Nan::New<FunctionTemplate>(dot_graph));
  RegexWrapper::init();
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
    auto result = text_buffer.line_length_for_row(maybe_row.FromJust());
    if (result) {
      info.GetReturnValue().Set(Nan::New<Number>(*result));
    }
  }
}

void TextBufferWrapper::line_ending_for_row(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto maybe_row = Nan::To<uint32_t>(info[0]);
  if (maybe_row.IsJust()) {
    auto result = text_buffer.line_ending_for_row(maybe_row.FromJust());
    if (result) {
      info.GetReturnValue().Set(Nan::New<String>(result).ToLocalChecked());
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
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    auto result = text_buffer.search(*regex);
    if (result) {
      info.GetReturnValue().Set(RangeWrapper::from_range(*result));
    } else {
      info.GetReturnValue().Set(Nan::Null());
    }
  }
}

void TextBufferWrapper::search(const Nan::FunctionCallbackInfo<Value> &info) {
  class TextBufferSearcher : public Nan::AsyncWorker {
    const TextBuffer::Snapshot *snapshot;
    const Regex *regex;
    optional<Range> result;
    Nan::Persistent<Value> argument;

  public:
    TextBufferSearcher(Nan::Callback *completion_callback,
                       const TextBuffer::Snapshot *snapshot,
                       const Regex *regex,
                       Local<Value> arg) :
      AsyncWorker(completion_callback),
      snapshot{snapshot},
      regex{regex} {
      argument.Reset(arg);
    }

    void Execute() {
      result = snapshot->search(*regex);
    }

    void HandleOKCallback() {
      delete snapshot;
      if (result) {
        Local<Value> argv[] = {Nan::Null(), RangeWrapper::from_range(*result)};
        callback->Call(2, argv);
      } else {
        Local<Value> argv[] = {Nan::Null(), Nan::Null()};
        callback->Call(2, argv);
      }
    }
  };

  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto callback = new Nan::Callback(info[1].As<Function>());
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    Nan::AsyncQueueWorker(new TextBufferSearcher(
      callback,
      text_buffer.create_snapshot(),
      regex,
      info[0]
    ));
  }
}

void TextBufferWrapper::is_modified(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(Nan::New<Boolean>(text_buffer.is_modified()));
}

void TextBufferWrapper::load_sync(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  if (text_buffer.is_modified()) {
    info.GetReturnValue().Set(Nan::Null());
    return;
  }

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  string file_path = *String::Utf8Value(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  String::Utf8Value encoding_name(js_encoding_name);
  auto conversion = Text::transcoding_from(*encoding_name);
  if (!conversion) {
    Nan::ThrowError((string("Invalid encoding name: ") + *encoding_name).c_str());
    return;
  }

  Nan::Callback *progress_callback = nullptr;
  if (info[2]->IsFunction()) {
    progress_callback = new Nan::Callback(info[2].As<Function>());
  }

  Nan::HandleScope scope;

  std::ifstream file(file_path, std::ios_base::binary);
  auto beginning = file.tellg();
  file.seekg(0, std::ios::end);
  auto end = file.tellg();
  file.seekg(0);
  size_t file_size = end - beginning;

  vector<char> input_buffer(CHUNK_SIZE);
  Text loaded_text;
  loaded_text.reserve(file_size);
  loaded_text.decode(
    *conversion,
    file,
    input_buffer,
    [&progress_callback, file_size](size_t bytes_read) {
      if (progress_callback) {
        size_t percent_done = file_size > 0 ? 100 * bytes_read / file_size : 100;
        Local<Value> argv[] = {Nan::New<Number>(static_cast<uint32_t>(percent_done))};
        progress_callback->Call(1, argv);
      }
    }
  );

  bool has_changed = text_buffer.base_text() != loaded_text;
  if (progress_callback) {
    Local<Value> argv[] = {Nan::New<Number>(100), Nan::New<Boolean>(has_changed)};
    progress_callback->Call(2, argv);
  }

  Patch patch = text_diff(text_buffer.base_text(), loaded_text);
  if (has_changed) text_buffer.reset(move(loaded_text));

  info.GetReturnValue().Set(PatchWrapper::from_patch(move(patch)));
}

static const int INVALID_ENCODING = -1;

static Local<Value> error_for_number(int error_number, string encoding_name, string file_name) {
  if (error_number == INVALID_ENCODING) {
    return Nan::Error(("Invalid encoding name: " + encoding_name).c_str());
  } else {
    return node::ErrnoException(v8::Isolate::GetCurrent(), error_number, nullptr, nullptr, file_name.c_str());
  }
}

void TextBufferWrapper::load_(const Nan::FunctionCallbackInfo<Value> &info, bool force) {
  class Worker : public Nan::AsyncProgressWorkerBase<size_t> {
    Nan::Callback *progress_callback;
    TextBuffer *buffer;
    TextBuffer::Snapshot *snapshot;
    string file_name;
    string encoding_name;
    optional<Text> loaded_text;
    optional<int> error_number;
    Patch patch;
    bool force;

  public:
    Worker(Nan::Callback *completion_callback, Nan::Callback *progress_callback,
           TextBuffer *buffer, TextBuffer::Snapshot *snapshot, string &&file_name,
           string &&encoding_name, bool force) :
      AsyncProgressWorkerBase(completion_callback),
      progress_callback{progress_callback},
      buffer{buffer},
      snapshot{snapshot},
      file_name{file_name},
      encoding_name(encoding_name),
      force{force} {}

    Worker(Nan::Callback *completion_callback, Nan::Callback *progress_callback,
           TextBuffer *buffer, TextBuffer::Snapshot *snapshot, Text &&text, bool force) :
      AsyncProgressWorkerBase(completion_callback),
      progress_callback{progress_callback},
      buffer{buffer},
      snapshot{snapshot},
      loaded_text{move(text)},
      force{force} {}

    void Execute(const Nan::AsyncProgressWorkerBase<size_t>::ExecutionProgress &progress) {
      if (!loaded_text) {
        std::ifstream file(file_name, std::ios_base::binary);
        auto beginning = file.tellg();
        file.seekg(0, std::ios::end);
        auto end = file.tellg();
        file.seekg(0);
        size_t file_size = end - beginning;
        if (!file) {
          error_number = errno;
          return;
        }

        auto conversion = Text::transcoding_from(encoding_name.c_str());
        if (!conversion) {
          error_number = INVALID_ENCODING;
          return;
        }

        loaded_text = Text();
        vector<char> input_buffer(CHUNK_SIZE);
        loaded_text->reserve(file_size);
        if (!loaded_text->decode(
          *conversion,
          file,
          input_buffer,
          [&progress, file_size](size_t bytes_read) {
            size_t percent_done = file_size > 0 ? 100 * bytes_read / file_size : 100;
            progress.Send(&percent_done, 1);
          }
        )) {
          error_number = errno;
          return;
        }
      }

      patch = text_diff(snapshot->base_text(), *loaded_text);
    }

    void HandleProgressCallback(const size_t *percent_done, size_t count) {
      if (progress_callback && percent_done) {
        Nan::HandleScope scope;
        Local<Value> argv[] = {Nan::New<Number>(static_cast<uint32_t>(*percent_done))};
        progress_callback->Call(1, argv);
      }
    }

    void HandleOKCallback() {
      if (error_number) {
        Local<Value> argv[] = {error_for_number(*error_number, encoding_name, file_name)};
        callback->Call(1, argv);
        delete snapshot;
        return;
      }

      bool is_modified = buffer->is_modified();
      if (is_modified) {
        if (force) {
          Patch inverted_changes = buffer->get_inverted_changes(snapshot);
          inverted_changes.combine(patch);
          patch = move(inverted_changes);
        } else {
          Local<Value> argv[] = {Nan::Null(), Nan::Null()};
          callback->Call(2, argv);
          delete snapshot;
          return;
        }
      }

      delete snapshot;

      bool has_changed = is_modified || buffer->base_text() != *loaded_text;
      if (progress_callback) {
        Local<Value> argv[] = {Nan::New<Number>(100), Nan::New<Boolean>(has_changed)};
        progress_callback->Call(2, argv);
      }

      if (has_changed) buffer->reset(move(*loaded_text));

      Local<Value> argv[] = {Nan::Null(), PatchWrapper::from_patch(move(patch))};
      callback->Call(2, argv);
    }
  };

  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Nan::Callback *completion_callback = new Nan::Callback(info[0].As<Function>());

  Nan::Callback *progress_callback = nullptr;
  if (info[1]->IsFunction()) {
    progress_callback = new Nan::Callback(info[1].As<Function>());
  }

  if (text_buffer.is_modified() && !force) {
    Local<Value> argv[] = {Nan::Null(), Nan::Null()};
    completion_callback->Call(2, argv);
    return;
  }

  Worker *worker;
  if (info[2]->IsString()) {
    Local<String> js_file_path;
    if (!Nan::To<String>(info[2]).ToLocal(&js_file_path)) return;
    string file_path = *String::Utf8Value(js_file_path);

    Local<String> js_encoding_name;
    if (!Nan::To<String>(info[3]).ToLocal(&js_encoding_name)) return;
    string encoding_name = *String::Utf8Value(info[3].As<String>());

    worker = new Worker(
      completion_callback,
      progress_callback,
      &text_buffer,
      text_buffer.create_snapshot(),
      move(file_path),
      move(encoding_name),
      force
    );
  } else {
    auto text_writer = Nan::ObjectWrap::Unwrap<TextWriter>(info[2]->ToObject());
    worker = new Worker(
      completion_callback,
      progress_callback,
      &text_buffer,
      text_buffer.create_snapshot(),
      text_writer->get_text(),
      force
    );
  }

  Nan::AsyncQueueWorker(worker);
}

void TextBufferWrapper::load(const Nan::FunctionCallbackInfo<Value> &info) {
  load_(info, false);
}

void TextBufferWrapper::reload(const Nan::FunctionCallbackInfo<Value> &info) {
  load_(info, true);
}

void TextBufferWrapper::save_sync(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  string file_path = *String::Utf8Value(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  string encoding_name = *String::Utf8Value(info[1].As<String>());

  auto conversion = Text::transcoding_to(encoding_name.c_str());
  if (!conversion) {
    info.GetReturnValue().Set(Nan::False());
    return;
  }

  std::ofstream file(file_path, std::ios_base::binary);
  vector<char> output_buffer(CHUNK_SIZE);
  for (TextSlice &chunk : text_buffer.chunks()) {
    if (!chunk.text->encode(*conversion, chunk.start_offset(), chunk.end_offset(), file, output_buffer)) {
      info.GetReturnValue().Set(Nan::False());
      return;
    }
  }

  text_buffer.flush_changes();
  info.GetReturnValue().Set(Nan::True());
}

void TextBufferWrapper::save(const Nan::FunctionCallbackInfo<Value> &info) {
  class Worker : public Nan::AsyncWorker {
    TextBuffer::Snapshot *snapshot;
    string file_name;
    string encoding_name;
    optional<int> error_number;

  public:
    Worker(Nan::Callback *completion_callback, TextBuffer::Snapshot *snapshot,
           string &&file_name, string &&encoding_name) :
      AsyncWorker(completion_callback),
      snapshot{snapshot},
      file_name{file_name},
      encoding_name(encoding_name) {}

    void Execute() {
      auto conversion = Text::transcoding_to(encoding_name.c_str());
      if (!conversion) {
        error_number = INVALID_ENCODING;
        return;
      }

      std::ofstream file(file_name, std::ios_base::binary);
      if (!file) {
        error_number = errno;
        return;
      }

      vector<char> output_buffer(CHUNK_SIZE);
      for (TextSlice &chunk : snapshot->chunks()) {
        if (!chunk.text->encode(*conversion, chunk.start_offset(), chunk.end_offset(), file, output_buffer)) {
          error_number = errno;
          return;
        }
      }
    }

    void HandleOKCallback() {
      snapshot->flush_preceding_changes();
      delete snapshot;
      if (error_number) {
        Local<Value> argv[] = {error_for_number(*error_number, encoding_name, file_name)};
        callback->Call(1, argv);
      } else {
        Local<Value> argv[] = {Nan::Null()};
        callback->Call(1, argv);
      }
    }
  };

  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  string file_path = *String::Utf8Value(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  string encoding_name = *String::Utf8Value(info[1].As<String>());

  Nan::Callback *completion_callback = new Nan::Callback(info[2].As<Function>());
  Nan::AsyncQueueWorker(new Worker(
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
    text_buffer.base_text().digest();
  Local<String> result;
  if (Nan::New(stream.str()).ToLocal(&result)) {
    info.GetReturnValue().Set(result);
  }
}

void TextBufferWrapper::dot_graph(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(Nan::New<String>(text_buffer.get_dot_graph()).ToLocalChecked());
}