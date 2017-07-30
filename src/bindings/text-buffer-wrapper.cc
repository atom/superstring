#include "text-buffer-wrapper.h"
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include "point-wrapper.h"
#include "range-wrapper.h"
#include "string-conversion.h"
#include "patch-wrapper.h"
#include "text-writer.h"
#include "text-slice.h"
#include "text-diff.h"
#include "noop.h"
#include <sys/stat.h>

using namespace v8;
using std::move;
using std::pair;
using std::string;
using std::u16string;
using std::vector;
using std::wstring;

#ifdef WIN32

static wstring ToUTF16(string input) {
  wstring result;
  int length = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.length(), NULL, 0);
  if (length > 0) {
    result.resize(length);
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.length(), &result[0], length);
  }
  return result;
}

static size_t get_file_size(const string &name) {
  struct _stat file_stats;
  if (_wstat(ToUTF16(name).c_str(), &file_stats) != 0) return -1;
  return file_stats.st_size;
}

static FILE *open_file(const string &name, const char *flags) {
  wchar_t wide_flags[6] = {0, 0, 0, 0, 0, 0};
  size_t flag_count = strlen(flags);
  MultiByteToWideChar(CP_UTF8, 0, flags, flag_count, wide_flags, flag_count);
  return _wfopen(ToUTF16(name).c_str(), wide_flags);
}

#else

static size_t get_file_size(const std::string &name) {
  struct stat file_stats;
  if (stat(name.c_str(), &file_stats) != 0) return -1;
  return file_stats.st_size;
}

static FILE *open_file(const std::string &name, const char *flags) {
  return fopen(name.c_str(), flags);
}

#endif

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
    optional<u16string> pattern = string_conversion::string_from_js(js_pattern);
    Regex regex(*pattern, &error_message);
    if (!error_message.empty()) {
      Nan::ThrowError(string_conversion::string_to_js(error_message));
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
  prototype_template->Set(Nan::New("getLineCount").ToLocalChecked(), Nan::New<FunctionTemplate>(get_line_count));
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
  prototype_template->Set(Nan::New("baseTextMatchesFile").ToLocalChecked(), Nan::New<FunctionTemplate>(base_text_matches_file));
  prototype_template->Set(Nan::New("save").ToLocalChecked(), Nan::New<FunctionTemplate>(save));
  prototype_template->Set(Nan::New("loadSync").ToLocalChecked(), Nan::New<FunctionTemplate>(load_sync));
  prototype_template->Set(Nan::New("saveSync").ToLocalChecked(), Nan::New<FunctionTemplate>(save_sync));
  prototype_template->Set(Nan::New("serializeChanges").ToLocalChecked(), Nan::New<FunctionTemplate>(serialize_changes));
  prototype_template->Set(Nan::New("deserializeChanges").ToLocalChecked(), Nan::New<FunctionTemplate>(deserialize_changes));
  prototype_template->Set(Nan::New("reset").ToLocalChecked(), Nan::New<FunctionTemplate>(reset));
  prototype_template->Set(Nan::New("baseTextDigest").ToLocalChecked(), Nan::New<FunctionTemplate>(base_text_digest));
  prototype_template->Set(Nan::New("find").ToLocalChecked(), Nan::New<FunctionTemplate>(find));
  prototype_template->Set(Nan::New("findSync").ToLocalChecked(), Nan::New<FunctionTemplate>(find_sync));
  prototype_template->Set(Nan::New("findAllSync").ToLocalChecked(), Nan::New<FunctionTemplate>(find_all_sync));
  prototype_template->Set(Nan::New("getDotGraph").ToLocalChecked(), Nan::New<FunctionTemplate>(dot_graph));
  RegexWrapper::init();
  exports->Set(Nan::New("TextBuffer").ToLocalChecked(), constructor_template->GetFunction());
}

void TextBufferWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  TextBufferWrapper *wrapper = new TextBufferWrapper();
  if (info.Length() > 0 && info[0]->IsString()) {
    auto text = string_conversion::string_from_js(info[0]);
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

void TextBufferWrapper::get_line_count(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(Nan::New(text_buffer.extent().row + 1));
}

void TextBufferWrapper::get_text_in_range(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto range = RangeWrapper::range_from_js(info[0]);
  if (range) {
    Local<String> result;
    info.GetReturnValue().Set(string_conversion::string_to_js(text_buffer.text_in_range(*range)));
  }
}

void TextBufferWrapper::get_text(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(string_conversion::string_to_js(text_buffer.text()));
}

void TextBufferWrapper::set_text_in_range(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto range = RangeWrapper::range_from_js(info[0]);
  auto text = string_conversion::string_from_js(info[1]);
  if (range && text) {
    text_buffer.set_text_in_range(*range, move(*text));
  }
}

void TextBufferWrapper::set_text(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto text = string_conversion::string_from_js(info[0]);
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
      text_buffer.with_line_for_row(row, [&info](const char16_t *data, uint32_t size) {
        Local<String> result;
        if (Nan::New<String>(reinterpret_cast<const uint16_t *>(data), size).ToLocal(&result)) {
          info.GetReturnValue().Set(result);
        }
      });
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
    auto text = text_buffer.text_in_range({{row, 0}, {row, UINT32_MAX}});
    result->Set(row, string_conversion::string_to_js(text));
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

void TextBufferWrapper::find_sync(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    auto result = text_buffer.find(*regex);
    if (result) {
      info.GetReturnValue().Set(RangeWrapper::from_range(*result));
    } else {
      info.GetReturnValue().Set(Nan::Null());
    }
  }
}

void TextBufferWrapper::find_all_sync(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    auto matches = text_buffer.find_all(*regex);
    auto length = matches.size() * 4;
    auto buffer = v8::ArrayBuffer::New(v8::Isolate::GetCurrent(), length * sizeof(uint32_t));
    auto result = v8::Uint32Array::New(buffer, 0, length);
    auto data = buffer->GetContents().Data();
    memcpy(data, matches.data(), length * sizeof(uint32_t));
    info.GetReturnValue().Set(result);
  }
}

void TextBufferWrapper::find(const Nan::FunctionCallbackInfo<Value> &info) {
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
      result = snapshot->find(*regex);
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

static const int INVALID_ENCODING = -1;

static Local<Value> error_for_number(int error_number, string encoding_name, string file_name) {
  if (error_number == INVALID_ENCODING) {
    return Nan::Error(("Invalid encoding name: " + encoding_name).c_str());
  } else {
    return node::ErrnoException(v8::Isolate::GetCurrent(), error_number, nullptr, nullptr, file_name.c_str());
  }
}

template <typename Callback>
static u16string load_file(const string &file_name, const string &encoding_name, optional<int> *error_number, const Callback &callback) {
  auto conversion = transcoding_from(encoding_name.c_str());
  if (!conversion) {
    *error_number = INVALID_ENCODING;
    return u"";
  }

  size_t file_size = get_file_size(file_name);
  if (file_size == static_cast<size_t>(-1)) {
    *error_number = errno;
    return u"";
  }

  FILE *file = open_file(file_name, "rb");
  if (!file) {
    *error_number = errno;
    return u"";
  }

  u16string loaded_string;
  vector<char> input_buffer(CHUNK_SIZE);
  loaded_string.reserve(file_size);
  if (!conversion->decode(
    loaded_string,
    file,
    input_buffer,
    [&callback, file_size](size_t bytes_read) {
      size_t percent_done = file_size > 0 ? 100 * bytes_read / file_size : 100;
      callback(percent_done);
    }
  )) {
    *error_number = errno;
  }

  fclose(file);
  return loaded_string;
}

class LoadWorker : public Nan::AsyncProgressWorkerBase<size_t> {
  Nan::Callback *progress_callback;
  TextBuffer *buffer;
  TextBuffer::Snapshot *snapshot;
  string file_name;
  string encoding_name;
  optional<Text> loaded_text;
  optional<int> error_number;
  Patch patch;
  bool force;
  bool compute_patch;
  bool cancelled;

 public:
  LoadWorker(Nan::Callback *completion_callback, Nan::Callback *progress_callback,
         TextBuffer *buffer, TextBuffer::Snapshot *snapshot, string &&file_name,
         string &&encoding_name, bool force, bool compute_patch) :
    AsyncProgressWorkerBase(completion_callback),
    progress_callback{progress_callback},
    buffer{buffer},
    snapshot{snapshot},
    file_name{file_name},
    encoding_name(encoding_name),
    force{force},
    compute_patch{compute_patch},
    cancelled{false} {}

  LoadWorker(Nan::Callback *completion_callback, Nan::Callback *progress_callback,
         TextBuffer *buffer, TextBuffer::Snapshot *snapshot, Text &&text,
         bool force, bool compute_patch) :
    AsyncProgressWorkerBase(completion_callback),
    progress_callback{progress_callback},
    buffer{buffer},
    snapshot{snapshot},
    loaded_text{move(text)},
    force{force},
    compute_patch{compute_patch},
    cancelled{false} {}

  void Execute(const Nan::AsyncProgressWorkerBase<size_t>::ExecutionProgress &progress) {
    if (!loaded_text) {
      loaded_text = Text{
        load_file(file_name, encoding_name, &error_number, [&progress](size_t percent_done) {
          progress.Send(&percent_done, 1);
        })
      };
    }
    if (!error_number && compute_patch) patch = text_diff(snapshot->base_text(), *loaded_text);
  }

  void ExecuteSync() {
    loaded_text = Text{
      load_file(file_name, encoding_name, &error_number, [this](size_t percent_done) {
        HandleProgressCallback(&percent_done, 1);
      })
    };
    if (!error_number && compute_patch) patch = text_diff(snapshot->base_text(), *loaded_text);
  }

  void HandleProgressCallback(const size_t *percent_done, size_t count) {
    if (!cancelled && progress_callback && percent_done) {
      Nan::HandleScope scope;
      Local<Value> argv[] = {Nan::New<Number>(static_cast<uint32_t>(*percent_done))};
      auto progress_result = progress_callback->Call(1, argv);
      if (progress_result->IsFalse()) cancelled = true;
    }
  }

  pair<Local<Value>, Local<Value>> Finish() {
    if (error_number) {
      delete snapshot;
      return {error_for_number(*error_number, encoding_name, file_name), Nan::Undefined()};
    }

    if (cancelled || (!force && buffer->is_modified())) {
      delete snapshot;
      return {Nan::Null(), Nan::Null()};
    }

    Patch inverted_changes = buffer->get_inverted_changes(snapshot);
    delete snapshot;

    if (compute_patch && inverted_changes.get_change_count() > 0) {
      inverted_changes.combine(patch);
      patch = move(inverted_changes);
    }

    bool has_changed;
    Local<Value> patch_wrapper;
    if (compute_patch) {
      has_changed = !compute_patch || patch.get_change_count() > 0;
      patch_wrapper = PatchWrapper::from_patch(move(patch));
    } else {
      has_changed = true;
      patch_wrapper = Nan::Null();
    }

    if (progress_callback) {
      Local<Value> argv[] = {Nan::New<Number>(100), patch_wrapper};
      auto progress_result = progress_callback->Call(2, argv);
      if (progress_result->IsFalse()) {
        return {Nan::Null(), Nan::Null()};
      }
    }

    if (has_changed) {
      buffer->reset(move(*loaded_text));
    } else {
      buffer->flush_changes();
    }

    return {Nan::Null(), patch_wrapper};
  }

  void HandleOKCallback() {
    auto results = Finish();
    Local<Value> argv[] = {results.first, results.second};
    callback->Call(2, argv);
  }
};

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
  string encoding_name = *String::Utf8Value(js_encoding_name);

  Nan::Callback *progress_callback = nullptr;
  if (info[2]->IsFunction()) {
    progress_callback = new Nan::Callback(info[2].As<Function>());
  }

  Nan::HandleScope scope;

  LoadWorker worker(
    nullptr,
    progress_callback,
    &text_buffer,
    text_buffer.create_snapshot(),
    move(file_path),
    move(encoding_name),
    false,
    true
  );

  worker.ExecuteSync();
  auto results = worker.Finish();
  if (results.first->IsNull()) {
    info.GetReturnValue().Set(results.second);
  } else {
    Nan::ThrowError(results.first);
  }
}

void TextBufferWrapper::load(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Nan::Callback *completion_callback = new Nan::Callback(info[0].As<Function>());

  Nan::Callback *progress_callback = nullptr;
  if (info[1]->IsFunction()) {
    progress_callback = new Nan::Callback(info[1].As<Function>());
  }

  bool force = false;
  if (info[2]->IsTrue()) force = true;

  bool compute_patch = true;
  if (info[3]->IsFalse()) compute_patch = false;

  if (!force && text_buffer.is_modified()) {
    Local<Value> argv[] = {Nan::Null(), Nan::Null()};
    completion_callback->Call(2, argv);
    return;
  }

  LoadWorker *worker;
  if (info[4]->IsString()) {
    Local<String> js_file_path;
    if (!Nan::To<String>(info[4]).ToLocal(&js_file_path)) return;
    string file_path = *String::Utf8Value(js_file_path);

    Local<String> js_encoding_name;
    if (!Nan::To<String>(info[5]).ToLocal(&js_encoding_name)) return;
    string encoding_name = *String::Utf8Value(info[5].As<String>());

    worker = new LoadWorker(
      completion_callback,
      progress_callback,
      &text_buffer,
      text_buffer.create_snapshot(),
      move(file_path),
      move(encoding_name),
      force,
      compute_patch
    );
  } else {
    auto text_writer = Nan::ObjectWrap::Unwrap<TextWriter>(info[4]->ToObject());
    worker = new LoadWorker(
      completion_callback,
      progress_callback,
      &text_buffer,
      text_buffer.create_snapshot(),
      text_writer->get_text(),
      force,
      compute_patch
    );
  }

  Nan::AsyncQueueWorker(worker);
}

class BaseTextComparisonWorker : public Nan::AsyncWorker {
  TextBuffer::Snapshot *snapshot;
  string file_name;
  string encoding_name;
  optional<int> error_number;
  bool result;

 public:
  BaseTextComparisonWorker(Nan::Callback *completion_callback, TextBuffer::Snapshot *snapshot,
                       string &&file_name, string &&encoding_name) :
    AsyncWorker(completion_callback),
    snapshot{snapshot},
    file_name{move(file_name)},
    encoding_name{move(encoding_name)},
    result{false} {}

  void Execute() {
    u16string file_contents = load_file(file_name, encoding_name, &error_number, [](size_t progress) {});
    result = std::equal(file_contents.begin(), file_contents.end(), snapshot->base_text().begin());
  }

  void HandleOKCallback() {
    delete snapshot;
    if (error_number) {
      Local<Value> argv[] = {error_for_number(*error_number, encoding_name, file_name)};
      callback->Call(1, argv);
    } else {
      Local<Value> argv[] = {Nan::Null(), Nan::New<Boolean>(result)};
      callback->Call(2, argv);
    }
  }
};

void TextBufferWrapper::base_text_matches_file(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  Nan::Callback *completion_callback = new Nan::Callback(info[0].As<Function>());

  if (info[1]->IsString()) {
    Local<String> js_file_path;
    if (!Nan::To<String>(info[1]).ToLocal(&js_file_path)) return;
    string file_path = *String::Utf8Value(js_file_path);

    Local<String> js_encoding_name;
    if (!Nan::To<String>(info[2]).ToLocal(&js_encoding_name)) return;
    string encoding_name = *String::Utf8Value(info[2].As<String>());

    Nan::AsyncQueueWorker(new BaseTextComparisonWorker(
      completion_callback,
      text_buffer.create_snapshot(),
      move(file_path),
      move(encoding_name)
    ));
  } else {
    auto file_contents = Nan::ObjectWrap::Unwrap<TextWriter>(info[1]->ToObject())->get_text();
    bool result = std::equal(file_contents.begin(), file_contents.end(), text_buffer.base_text().begin());
    Local<Value> argv[] = {Nan::Null(), Nan::New<Boolean>(result)};
    completion_callback->Call(2, argv);
  }
}

class SaveWorker : public Nan::AsyncWorker {
  TextBuffer::Snapshot *snapshot;
  string file_name;
  string encoding_name;
  optional<int> error_number;

 public:
  SaveWorker(Nan::Callback *completion_callback, TextBuffer::Snapshot *snapshot,
             string &&file_name, string &&encoding_name) :
    AsyncWorker(completion_callback),
    snapshot{snapshot},
    file_name{file_name},
    encoding_name(encoding_name) {}

  void Execute() {
    auto conversion = transcoding_to(encoding_name.c_str());
    if (!conversion) {
      error_number = INVALID_ENCODING;
      return;
    }

    FILE *file = open_file(file_name, "wb+");
    if (!file) {
      error_number = errno;
      return;
    }

    vector<char> output_buffer(CHUNK_SIZE);
    for (TextSlice &chunk : snapshot->chunks()) {
      if (!conversion->encode(
        chunk.text->content,
        chunk.start_offset(),
        chunk.end_offset(),
        file,
        output_buffer
      )) {
        error_number = errno;
        fclose(file);
        return;
      }
    }

    fclose(file);
  }

  Local<Value> Finish() {
    snapshot->flush_preceding_changes();
    delete snapshot;
    if (error_number) {
      return error_for_number(*error_number, encoding_name, file_name);
    } else {
      return Nan::Null();
    }
  }

  void HandleOKCallback() {
    Local<Value> argv[] = {Finish()};
    callback->Call(1, argv);
  }
};

void TextBufferWrapper::save_sync(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  string file_path = *String::Utf8Value(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  string encoding_name = *String::Utf8Value(info[1].As<String>());

  SaveWorker worker(
    nullptr,
    text_buffer.create_snapshot(),
    move(file_path),
    move(encoding_name)
  );

  worker.Execute();
  auto error = worker.Finish();
  if (!error->IsNull()) {
    Nan::ThrowError(error);
  }
}

void TextBufferWrapper::save(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  string file_path = *String::Utf8Value(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  string encoding_name = *String::Utf8Value(info[1].As<String>());

  Nan::Callback *completion_callback = new Nan::Callback(info[2].As<Function>());
  Nan::AsyncQueueWorker(new SaveWorker(
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

void TextBufferWrapper::reset(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto text = string_conversion::string_from_js(info[0]);
  if (text) {
    text_buffer.reset(move(*text));
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
