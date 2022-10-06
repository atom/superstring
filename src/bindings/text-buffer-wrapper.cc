#include "text-buffer-wrapper.h"
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include "number-conversion.h"
#include "point-wrapper.h"
#include "range-wrapper.h"
#include "marker-index-wrapper.h"
#include "string-conversion.h"
#include "patch-wrapper.h"
#include "text-buffer-snapshot-wrapper.h"
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

using SubsequenceMatch = TextBuffer::SubsequenceMatch;

#ifdef WIN32

#include <windows.h>
#include <io.h>

static wstring ToUTF16(string input) {
  wstring result;
  int length = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.length(), NULL, 0);
  if (length > 0) {
    result.resize(length);
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.length(), &result[0], length);
  }
  return result;
}

static size_t get_file_size(FILE *file) {
  LARGE_INTEGER result;
  if (!GetFileSizeEx((HANDLE)_get_osfhandle(fileno(file)), &result)) {
    errno = GetLastError();
    return -1;
  }
  return static_cast<size_t>(result.QuadPart);
}

static FILE *open_file(const string &name, const char *flags) {
  wchar_t wide_flags[6] = {0, 0, 0, 0, 0, 0};
  size_t flag_count = strlen(flags);
  MultiByteToWideChar(CP_UTF8, 0, flags, flag_count, wide_flags, flag_count);
  return _wfopen(ToUTF16(name).c_str(), wide_flags);
}

#else

static size_t get_file_size(FILE *file) {
  struct stat file_stats;
  if (fstat(fileno(file), &file_stats) != 0) return -1;
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
    bool ignore_case = false;
    bool unicode = false;

    if (value->IsString()) {
      js_pattern = Local<String>::Cast(value);
    } else if (value->IsRegExp()) {
      js_regex = Local<RegExp>::Cast(value);
      Local<Value> stored_regex = Nan::Get(js_regex, cache_key).ToLocalChecked();
      if (!stored_regex->IsUndefined()) {
        return &Nan::ObjectWrap::Unwrap<RegexWrapper>(Nan::To<Object>(stored_regex).ToLocalChecked())->regex;
      }
      js_pattern = js_regex->GetSource();
      if (js_regex->GetFlags() & RegExp::kIgnoreCase) ignore_case = true;
      if (js_regex->GetFlags() & RegExp::kUnicode) unicode = true;
    } else {
      Nan::ThrowTypeError("Argument must be a RegExp");
      return nullptr;
    }

    u16string error_message;
    optional<u16string> pattern = string_conversion::string_from_js(js_pattern);
    Regex regex(*pattern, &error_message, ignore_case, unicode);
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
    if (!js_regex.IsEmpty()) Nan::Set(js_regex, cache_key, result);
    return &regex_wrapper->regex;
  }

  static void init() {
    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
    constructor_template->SetClassName(Nan::New<String>("TextBufferRegex").ToLocalChecked());
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    constructor.Reset(Nan::GetFunction(constructor_template).ToLocalChecked());
  }
};

Nan::Persistent<Function> RegexWrapper::constructor;

class SubsequenceMatchWrapper : public Nan::ObjectWrap {
public:
  static Nan::Persistent<Function> constructor;

  SubsequenceMatchWrapper(SubsequenceMatch &&match) :
    match(std::move(match)) {}

  static void init() {
    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>();
    constructor_template->SetClassName(Nan::New<String>("SubsequenceMatch").ToLocalChecked());
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    const auto &instance_template = constructor_template->InstanceTemplate();

    Nan::SetAccessor(instance_template, Nan::New("word").ToLocalChecked(), get_word);
    Nan::SetAccessor(instance_template, Nan::New("matchIndices").ToLocalChecked(), get_match_indices);
    Nan::SetAccessor(instance_template, Nan::New("score").ToLocalChecked(), get_score);

    constructor.Reset(Nan::GetFunction(constructor_template).ToLocalChecked());
  }

  static Local<Value> from_subsequence_match(SubsequenceMatch match) {
    Local<Object> result;
    if (Nan::NewInstance(Nan::New(constructor)).ToLocal(&result)) {
      (new SubsequenceMatchWrapper(std::move(match)))->Wrap(result);
      return result;
    } else {
      return Nan::Null();
    }
  }

 private:
  static void get_word(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    SubsequenceMatch &match = Nan::ObjectWrap::Unwrap<SubsequenceMatchWrapper>(info.This())->match;
    info.GetReturnValue().Set(string_conversion::string_to_js(match.word));
  }

  static void get_match_indices(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    SubsequenceMatch &match = Nan::ObjectWrap::Unwrap<SubsequenceMatchWrapper>(info.This())->match;
    Local<Array> js_result = Nan::New<Array>();
    for (size_t i = 0; i < match.match_indices.size(); i++) {
      Nan::Set(js_result, i, Nan::New<Integer>(match.match_indices[i]));
    }
    info.GetReturnValue().Set(js_result);
  }

  static void get_score(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    SubsequenceMatch &match = Nan::ObjectWrap::Unwrap<SubsequenceMatchWrapper>(info.This())->match;
    info.GetReturnValue().Set(Nan::New<Integer>(match.score));
  }

  TextBuffer::SubsequenceMatch match;
};

Nan::Persistent<Function> SubsequenceMatchWrapper::constructor;

void TextBufferWrapper::init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("TextBuffer").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template->PrototypeTemplate();
  Nan::SetTemplate(prototype_template, Nan::New("delete").ToLocalChecked(), Nan::New<FunctionTemplate>(noop), None);
  Nan::SetTemplate(prototype_template, Nan::New("getLength").ToLocalChecked(), Nan::New<FunctionTemplate>(get_length), None);
  Nan::SetTemplate(prototype_template, Nan::New("getExtent").ToLocalChecked(), Nan::New<FunctionTemplate>(get_extent), None);
  Nan::SetTemplate(prototype_template, Nan::New("getLineCount").ToLocalChecked(), Nan::New<FunctionTemplate>(get_line_count), None);
  Nan::SetTemplate(prototype_template, Nan::New("hasAstral").ToLocalChecked(), Nan::New<FunctionTemplate>(has_astral), None);
  Nan::SetTemplate(prototype_template, Nan::New("getCharacterAtPosition").ToLocalChecked(), Nan::New<FunctionTemplate>(get_character_at_position), None);
  Nan::SetTemplate(prototype_template, Nan::New("getTextInRange").ToLocalChecked(), Nan::New<FunctionTemplate>(get_text_in_range), None);
  Nan::SetTemplate(prototype_template, Nan::New("setTextInRange").ToLocalChecked(), Nan::New<FunctionTemplate>(set_text_in_range), None);
  Nan::SetTemplate(prototype_template, Nan::New("getText").ToLocalChecked(), Nan::New<FunctionTemplate>(get_text), None);
  Nan::SetTemplate(prototype_template, Nan::New("setText").ToLocalChecked(), Nan::New<FunctionTemplate>(set_text), None);
  Nan::SetTemplate(prototype_template, Nan::New("lineForRow").ToLocalChecked(), Nan::New<FunctionTemplate>(line_for_row), None);
  Nan::SetTemplate(prototype_template, Nan::New("lineLengthForRow").ToLocalChecked(), Nan::New<FunctionTemplate>(line_length_for_row), None);
  Nan::SetTemplate(prototype_template, Nan::New("lineEndingForRow").ToLocalChecked(), Nan::New<FunctionTemplate>(line_ending_for_row), None);
  Nan::SetTemplate(prototype_template, Nan::New("getLines").ToLocalChecked(), Nan::New<FunctionTemplate>(get_lines), None);
  Nan::SetTemplate(prototype_template, Nan::New("characterIndexForPosition").ToLocalChecked(), Nan::New<FunctionTemplate>(character_index_for_position), None);
  Nan::SetTemplate(prototype_template, Nan::New("positionForCharacterIndex").ToLocalChecked(), Nan::New<FunctionTemplate>(position_for_character_index), None);
  Nan::SetTemplate(prototype_template, Nan::New("isModified").ToLocalChecked(), Nan::New<FunctionTemplate>(is_modified), None);
  Nan::SetTemplate(prototype_template, Nan::New("load").ToLocalChecked(), Nan::New<FunctionTemplate>(load), None);
  Nan::SetTemplate(prototype_template, Nan::New("baseTextMatchesFile").ToLocalChecked(), Nan::New<FunctionTemplate>(base_text_matches_file), None);
  Nan::SetTemplate(prototype_template, Nan::New("save").ToLocalChecked(), Nan::New<FunctionTemplate>(save), None);
  Nan::SetTemplate(prototype_template, Nan::New("loadSync").ToLocalChecked(), Nan::New<FunctionTemplate>(load_sync), None);
  Nan::SetTemplate(prototype_template, Nan::New("serializeChanges").ToLocalChecked(), Nan::New<FunctionTemplate>(serialize_changes), None);
  Nan::SetTemplate(prototype_template, Nan::New("deserializeChanges").ToLocalChecked(), Nan::New<FunctionTemplate>(deserialize_changes), None);
  Nan::SetTemplate(prototype_template, Nan::New("reset").ToLocalChecked(), Nan::New<FunctionTemplate>(reset), None);
  Nan::SetTemplate(prototype_template, Nan::New("baseTextDigest").ToLocalChecked(), Nan::New<FunctionTemplate>(base_text_digest), None);
  Nan::SetTemplate(prototype_template, Nan::New("find").ToLocalChecked(), Nan::New<FunctionTemplate>(find), None);
  Nan::SetTemplate(prototype_template, Nan::New("findSync").ToLocalChecked(), Nan::New<FunctionTemplate>(find_sync), None);
  Nan::SetTemplate(prototype_template, Nan::New("findAll").ToLocalChecked(), Nan::New<FunctionTemplate>(find_all), None);
  Nan::SetTemplate(prototype_template, Nan::New("findAllSync").ToLocalChecked(), Nan::New<FunctionTemplate>(find_all_sync), None);
  Nan::SetTemplate(prototype_template, Nan::New("findAndMarkAllSync").ToLocalChecked(), Nan::New<FunctionTemplate>(find_and_mark_all_sync), None);
  Nan::SetTemplate(prototype_template, Nan::New("findWordsWithSubsequenceInRange").ToLocalChecked(), Nan::New<FunctionTemplate>(find_words_with_subsequence_in_range), None);
  Nan::SetTemplate(prototype_template, Nan::New("getDotGraph").ToLocalChecked(), Nan::New<FunctionTemplate>(dot_graph), None);
  Nan::SetTemplate(prototype_template, Nan::New("getSnapshot").ToLocalChecked(), Nan::New<FunctionTemplate>(get_snapshot), None);
  RegexWrapper::init();
  SubsequenceMatchWrapper::init();
  Nan::Set(exports, Nan::New("TextBuffer").ToLocalChecked(), Nan::GetFunction(constructor_template).ToLocalChecked());
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

void TextBufferWrapper::has_astral(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(Nan::New(text_buffer.has_astral()));
}

void TextBufferWrapper::get_character_at_position(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto point = PointWrapper::point_from_js(info[0]);
  if (point) {
    info.GetReturnValue().Set(string_conversion::char_to_js(text_buffer.character_at(*point)));
  }
}

void TextBufferWrapper::get_text_in_range(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto range = RangeWrapper::range_from_js(info[0]);
  if (range) {
    info.GetReturnValue().Set(string_conversion::string_to_js(text_buffer.text_in_range(*range)));
  }
}

void TextBufferWrapper::get_text(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(string_conversion::string_to_js(
    text_buffer.text(),
    "This buffer's content is too large to fit into a string.\n"
    "\n"
    "Consider using APIs like `getTextInRange` to access the data you need."
  ));
}

void TextBufferWrapper::set_text_in_range(const Nan::FunctionCallbackInfo<Value> &info) {
  auto text_buffer_wrapper = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This());
  text_buffer_wrapper->cancel_queued_workers();
  auto &text_buffer = text_buffer_wrapper->text_buffer;
  auto range = RangeWrapper::range_from_js(info[0]);
  auto text = string_conversion::string_from_js(info[1]);
  if (range && text) {
    text_buffer.set_text_in_range(*range, move(*text));
  }
}

void TextBufferWrapper::set_text(const Nan::FunctionCallbackInfo<Value> &info) {
  auto text_buffer_wrapper = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This());
  text_buffer_wrapper->cancel_queued_workers();
  auto &text_buffer = text_buffer_wrapper->text_buffer;
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
    Nan::Set(result, row, string_conversion::string_to_js(text));
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

static Local<Value> encode_ranges(const vector<Range> &ranges) {
  auto length = ranges.size() * 4;
  auto buffer = v8::ArrayBuffer::New(v8::Isolate::GetCurrent(), length * sizeof(uint32_t));
  auto result = v8::Uint32Array::New(buffer, 0, length);
  #if (V8_MAJOR_VERSION < 8)
    auto data = buffer->GetContents().Data();
  #else
    auto data = buffer->GetBackingStore()->Data();
  #endif
  memcpy(data, ranges.data(), length * sizeof(uint32_t));
  return result;
}

template <bool single_result>
class TextBufferSearcher : public Nan::AsyncWorker {
  const TextBuffer::Snapshot *snapshot;
  const Regex *regex;
  Range search_range;
  vector<Range> matches;
  Nan::Persistent<Value> argument;

public:
  TextBufferSearcher(Nan::Callback *completion_callback,
                     const TextBuffer::Snapshot *snapshot,
                     const Regex *regex,
                     const Range &search_range,
                     Local<Value> arg) :
    AsyncWorker(completion_callback, "TextBuffer.find"),
    snapshot{snapshot},
    regex{regex},
    search_range(search_range) {
    argument.Reset(arg);
  }

  void Execute() {
    if (single_result) {
      auto find_result = snapshot->find(*regex, search_range);
      if (find_result) {
        matches.push_back(*find_result);
      }
    } else {
      matches = snapshot->find_all(*regex, search_range);
    }
  }

  void HandleOKCallback() {
    delete snapshot;
    Local<Value> argv[] = {Nan::Null(), encode_ranges(matches)};
    callback->Call(2, argv, async_resource);
  }
};

void TextBufferWrapper::find_sync(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    optional<Range> search_range;
    if (info[1]->IsObject()) {
      search_range = RangeWrapper::range_from_js(info[1]);
      if (!search_range) return;
    }

    auto match = text_buffer.find(
      *regex,
      search_range ? *search_range : Range::all_inclusive()
    );
    vector<Range> matches;
    if (match) matches.push_back(*match);

    info.GetReturnValue().Set(encode_ranges(matches));
  }
}

void TextBufferWrapper::find_all_sync(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    optional<Range> search_range;
    if (info[1]->IsObject()) {
      search_range = RangeWrapper::range_from_js(info[1]);
      if (!search_range) return;
    }

    vector<Range> matches = text_buffer.find_all(
      *regex,
      search_range ? *search_range : Range::all_inclusive()
    );

    info.GetReturnValue().Set(encode_ranges(matches));
  }
}

void TextBufferWrapper::find_and_mark_all_sync(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  MarkerIndex *marker_index = MarkerIndexWrapper::from_js(info[0]);
  if (!marker_index) return;
  auto next_id = Nan::To<unsigned>(info[1]);
  if (!next_id.IsJust()) return;
  if (!info[2]->IsBoolean()) return;
  bool exclusive = Nan::To<bool>(info[2]).FromMaybe(false);

  const Regex *regex = RegexWrapper::regex_from_js(info[3]);
  if (regex) {
    optional<Range> search_range;
    if (info[4]->IsObject()) {
      search_range = RangeWrapper::range_from_js(info[4]);
      if (!search_range) return;
    }

    unsigned count = text_buffer.find_and_mark_all(
      *marker_index,
      next_id.FromJust(),
      exclusive,
      *regex,
      search_range ? *search_range : Range::all_inclusive()
    );

    info.GetReturnValue().Set(Nan::New<Number>(count));
  }
}

void TextBufferWrapper::find(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto callback = new Nan::Callback(info[1].As<Function>());
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    optional<Range> search_range;
    if (info[2]->IsObject()) {
      search_range = RangeWrapper::range_from_js(info[2]);
      if (!search_range) return;
    }
    Nan::AsyncQueueWorker(new TextBufferSearcher<true>(
      callback,
      text_buffer.create_snapshot(),
      regex,
      search_range ? *search_range : Range::all_inclusive(),
      info[0]
    ));
  }
}

void TextBufferWrapper::find_all(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto callback = new Nan::Callback(info[1].As<Function>());
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    optional<Range> search_range;
    if (info[2]->IsObject()) {
      search_range = RangeWrapper::range_from_js(info[2]);
      if (!search_range) return;
    }
    Nan::AsyncQueueWorker(new TextBufferSearcher<false>(
      callback,
      text_buffer.create_snapshot(),
      regex,
      search_range ? *search_range : Range::all_inclusive(),
      info[0]
    ));
  }
}

void TextBufferWrapper::find_words_with_subsequence_in_range(const Nan::FunctionCallbackInfo<v8::Value> &info) {
  class FindWordsWithSubsequenceInRangeWorker : public Nan::AsyncWorker, public CancellableWorker {
    Nan::Persistent<Object> buffer;
    const TextBuffer::Snapshot *snapshot;
    const u16string query;
    const u16string extra_word_characters;
    const size_t max_count;
    const Range range;
    vector<TextBuffer::SubsequenceMatch> result;
    uv_rwlock_t snapshot_lock;

  public:
    FindWordsWithSubsequenceInRangeWorker(Local<Object> buffer,
                                   Nan::Callback *completion_callback,
                                   const u16string query,
                                   const u16string extra_word_characters,
                                   const size_t max_count,
                                   const Range range) :
      AsyncWorker(completion_callback, "TextBuffer.findWordsWithSubsequence"),
      query{query},
      extra_word_characters{extra_word_characters},
      max_count{max_count},
      range(range) {
      uv_rwlock_init(&snapshot_lock);
      this->buffer.Reset(buffer);
      auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(buffer)->text_buffer;
      snapshot = text_buffer.create_snapshot();
    }

    ~FindWordsWithSubsequenceInRangeWorker() {
      uv_rwlock_destroy(&snapshot_lock);
    }

    void Execute() {
      uv_rwlock_rdlock(&snapshot_lock);
      if (!snapshot) {
        uv_rwlock_rdunlock(&snapshot_lock);
        return;
      }
      result = snapshot->find_words_with_subsequence_in_range(query, extra_word_characters, range);
      uv_rwlock_rdunlock(&snapshot_lock);
    }

    void CancelIfQueued() {
      int lock_status = uv_rwlock_trywrlock(&snapshot_lock);
      if (lock_status == 0) {
        delete snapshot;
        snapshot = nullptr;
        uv_rwlock_wrunlock(&snapshot_lock);
      }
    }

    void HandleOKCallback() {
      if (!snapshot) {
        Local<Value> argv[] = {Nan::Null()};
        callback->Call(1, argv, async_resource);
        return;
      }

      delete snapshot;
      auto text_buffer_wrapper = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(Nan::New(buffer));
      text_buffer_wrapper->outstanding_workers.erase(this);

      Local<Array> js_matches_array = Nan::New<Array>();

      uint32_t positions_buffer_size = 0;
      for (const auto &subsequence_match : result) {
        positions_buffer_size += sizeof(uint32_t) + subsequence_match.positions.size() * sizeof(Point);
      }

      auto positions_buffer = v8::ArrayBuffer::New(v8::Isolate::GetCurrent(), positions_buffer_size);
      #if (V8_MAJOR_VERSION < 8)
        uint32_t *positions_data = reinterpret_cast<uint32_t *>(positions_buffer->GetContents().Data());
      #else
        uint32_t *positions_data = reinterpret_cast<uint32_t *>(positions_buffer->GetBackingStore()->Data());
      #endif

      uint32_t positions_array_index = 0;
      for (size_t i = 0; i < result.size() && i < max_count; i++) {
        const SubsequenceMatch &match = result[i];
        positions_data[positions_array_index++] = match.positions.size();
        uint32_t bytes_to_copy = match.positions.size() * sizeof(Point);
        memcpy(
          positions_data + positions_array_index,
          match.positions.data(),
          bytes_to_copy
        );
        positions_array_index += bytes_to_copy / sizeof(uint32_t);
        Nan::Set(js_matches_array, i, SubsequenceMatchWrapper::from_subsequence_match(match));
      }

      auto positions_array = v8::Uint32Array::New(positions_buffer, 0, positions_buffer_size / sizeof(uint32_t));
      Local<Value> argv[] = {js_matches_array, positions_array};
      callback->Call(2, argv, async_resource);
    }
  };


  auto query = string_conversion::string_from_js(info[0]);
  auto extra_word_characters = string_conversion::string_from_js(info[1]);
  auto max_count = number_conversion::number_from_js<uint32_t>(info[2]);
  auto range = RangeWrapper::range_from_js(info[3]);
  auto callback = new Nan::Callback(info[4].As<Function>());

  if (query && extra_word_characters && max_count && range && callback) {
    auto js_buffer = info.This();
    auto text_buffer_wrapper = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(js_buffer);

    auto worker = new FindWordsWithSubsequenceInRangeWorker(
      js_buffer,
      callback,
      *query,
      *extra_word_characters,
      *max_count,
      *range
    );

    text_buffer_wrapper->outstanding_workers.insert(worker);
    Nan::AsyncQueueWorker(worker);
  } else {
    Nan::ThrowError("Invalid arguments");
  }
}

void TextBufferWrapper::is_modified(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(Nan::New<Boolean>(text_buffer.is_modified()));
}

static const int INVALID_ENCODING = -1;

struct Error {
  int number;
  const char *syscall;
};

static Local<Value> error_to_js(Error error, string encoding_name, string file_name) {
  if (error.number == INVALID_ENCODING) {
    return Nan::Error(("Invalid encoding name: " + encoding_name).c_str());
  } else {
    return node::ErrnoException(
      v8::Isolate::GetCurrent(), error.number, error.syscall, error.syscall, file_name.c_str()
    );
  }
}

template <typename Callback>
static u16string load_file(
  const string &file_name,
  const string &encoding_name,
  optional<Error> *error,
  const Callback &callback
) {
  auto conversion = transcoding_from(encoding_name.c_str());
  if (!conversion) {
    *error = Error{INVALID_ENCODING, nullptr};
    return u"";
  }

  FILE *file = open_file(file_name, "rb");
  if (!file) {
    *error = Error{errno, "open"};
    return u"";
  }

  size_t file_size = get_file_size(file);
  if (file_size == static_cast<size_t>(-1)) {
    *error = Error{errno, "stat"};
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
    *error = Error{errno, "read"};
  }

  fclose(file);
  return loaded_string;
}

class Loader {
  Nan::Callback *progress_callback;
  Nan::AsyncResource *async_resource;
  TextBuffer *buffer;
  TextBuffer::Snapshot *snapshot;
  string file_name;
  string encoding_name;
  optional<Text> loaded_text;
  optional<Error> error;
  Patch patch;
  bool force;
  bool compute_patch;

 public:
  bool cancelled;

  Loader(Nan::Callback *progress_callback, Nan::AsyncResource *async_resource,
         TextBuffer *buffer, TextBuffer::Snapshot *snapshot, string &&file_name,
         string &&encoding_name, bool force, bool compute_patch) :
    progress_callback{progress_callback},
    async_resource{async_resource},
    buffer{buffer},
    snapshot{snapshot},
    file_name{move(file_name)},
    encoding_name{move(encoding_name)},
    force{force},
    compute_patch{compute_patch},
    cancelled{false} {}

  Loader(Nan::Callback *progress_callback, Nan::AsyncResource *async_resource,
         TextBuffer *buffer, TextBuffer::Snapshot *snapshot, Text &&text,
         bool force, bool compute_patch) :
    progress_callback{progress_callback},
    buffer{buffer},
    snapshot{snapshot},
    loaded_text{move(text)},
    force{force},
    compute_patch{compute_patch},
    cancelled{false} {}

  ~Loader() {
    if (progress_callback) delete progress_callback;
  }

  template <typename Callback>
  void Execute(const Callback &callback) {
    if (!loaded_text) loaded_text = Text{load_file(file_name, encoding_name, &error, callback)};
    if (!error && compute_patch) patch = text_diff(snapshot->base_text(), *loaded_text);
  }

  pair<Local<Value>, Local<Value>> Finish(Nan::AsyncResource* caller_async_resource = nullptr) {
    if (error) {
      delete snapshot;
      return {error_to_js(*error, encoding_name, file_name), Nan::Undefined()};
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
      MaybeLocal<v8::Value> progress_result;
      Nan::AsyncResource* resource = caller_async_resource ? caller_async_resource : async_resource;
      if (resource) {
        progress_result = progress_callback->Call(2, argv, resource);
      } else {
        progress_result = Nan::Call(*progress_callback, 2, argv);
      }
      if (!progress_result.IsEmpty() && progress_result.ToLocalChecked()->IsFalse()) {
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

  void CallProgressCallback(size_t percent_done) {
    if (!cancelled && progress_callback) {
      Nan::HandleScope scope;
      Local<Value> argv[] = {Nan::New<Number>(static_cast<uint32_t>(percent_done))};
      MaybeLocal<v8::Value> progress_result;
      if (async_resource) {
        progress_result = progress_callback->Call(1, argv, async_resource);
      } else {
        progress_result = Nan::Call(*progress_callback, 1, argv);
      }

      if (!progress_result.IsEmpty() && progress_result.ToLocalChecked()->IsFalse()) cancelled = true;
    }
  }
};

class LoadWorker : public Nan::AsyncProgressWorkerBase<size_t> {
  Loader loader;

 public:
  LoadWorker(Nan::Callback *completion_callback, Nan::Callback *progress_callback,
             TextBuffer *buffer, TextBuffer::Snapshot *snapshot, string &&file_name,
             string &&encoding_name, bool force, bool compute_patch) :
    AsyncProgressWorkerBase(completion_callback, "TextBuffer.load"),
    loader(progress_callback, async_resource, buffer, snapshot, move(file_name), move(encoding_name), force, compute_patch) {}

  LoadWorker(Nan::Callback *completion_callback, Nan::Callback *progress_callback,
             TextBuffer *buffer, TextBuffer::Snapshot *snapshot, Text &&text,
             bool force, bool compute_patch) :
    AsyncProgressWorkerBase(completion_callback, "TextBuffer.load"),
    loader(progress_callback, async_resource, buffer, snapshot, move(text), force, compute_patch) {}

  void Execute(const Nan::AsyncProgressWorkerBase<size_t>::ExecutionProgress &progress) {
    loader.Execute([&progress](size_t percent_done) {
      progress.Send(&percent_done, 1);
    });
  }

  void HandleProgressCallback(const size_t *percent_done, size_t count) {
    if (percent_done) loader.CallProgressCallback(*percent_done);
  }

  void HandleOKCallback() {
    auto results = loader.Finish(async_resource);
    Local<Value> argv[] = {results.first, results.second};
    callback->Call(2, argv, async_resource);
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
  string file_path = *Nan::Utf8String(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  string encoding_name = *Nan::Utf8String(js_encoding_name);

  Nan::Callback *progress_callback = nullptr;
  if (info[2]->IsFunction()) {
    progress_callback = new Nan::Callback(info[2].As<Function>());
  }

  Nan::HandleScope scope;

  Loader worker(
    progress_callback,
    nullptr,
    &text_buffer,
    text_buffer.create_snapshot(),
    move(file_path),
    move(encoding_name),
    false,
    true
  );

  worker.Execute([&worker](size_t percent_done) {
    worker.CallProgressCallback(percent_done);
  });

  auto results = worker.Finish();
  if (results.first->IsNull()) {
    info.GetReturnValue().Set(results.second);
  } else {
    Nan::ThrowError(results.first);
  }
}

void TextBufferWrapper::load(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  bool force = false;
  if (info[2]->IsTrue()) force = true;

  bool compute_patch = true;
  if (info[3]->IsFalse()) compute_patch = false;

  if (!force && text_buffer.is_modified()) {
    Local<Value> argv[] = {Nan::Null(), Nan::Null()};
    auto callback = info[0].As<Function>();
    #if (V8_MAJOR_VERSION > 9 || (V8_MAJOR_VERSION == 9 && V8_MINOR_VERION > 4))
      Nan::Call(callback, callback->GetCreationContext().ToLocalChecked()->Global(), 2, argv);
    #else
      Nan::Call(callback, callback->CreationContext()->Global(), 2, argv);
    #endif
    return;
  }

  Nan::Callback *completion_callback = new Nan::Callback(info[0].As<Function>());

  Nan::Callback *progress_callback = nullptr;
  if (info[1]->IsFunction()) {
    progress_callback = new Nan::Callback(info[1].As<Function>());
  }

  LoadWorker *worker;
  if (info[4]->IsString()) {
    Local<String> js_file_path;
    if (!Nan::To<String>(info[4]).ToLocal(&js_file_path)) return;
    string file_path = *Nan::Utf8String(js_file_path);

    Local<String> js_encoding_name;
    if (!Nan::To<String>(info[5]).ToLocal(&js_encoding_name)) return;
    string encoding_name = *Nan::Utf8String(info[5].As<String>());

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
    auto text_writer = Nan::ObjectWrap::Unwrap<TextWriter>(Nan::To<Object>(info[4]).ToLocalChecked());
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
  optional<Error> error;
  bool result;

 public:
  BaseTextComparisonWorker(Nan::Callback *completion_callback, TextBuffer::Snapshot *snapshot,
                       string &&file_name, string &&encoding_name) :
    AsyncWorker(completion_callback, "TextBuffer.baseTextMatchesFile"),
    snapshot{snapshot},
    file_name{move(file_name)},
    encoding_name{move(encoding_name)},
    result{false} {}

  void Execute() {
    u16string file_contents = load_file(file_name, encoding_name, &error, [](size_t progress) {});
    result = std::equal(file_contents.begin(), file_contents.end(), snapshot->base_text().begin());
  }

  void HandleOKCallback() {
    delete snapshot;
    if (error) {
      Local<Value> argv[] = {error_to_js(*error, encoding_name, file_name)};
      callback->Call(1, argv, async_resource);
    } else {
      Local<Value> argv[] = {Nan::Null(), Nan::New<Boolean>(result)};
      callback->Call(2, argv, async_resource);
    }
  }
};

void TextBufferWrapper::base_text_matches_file(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  if (info[1]->IsString()) {
    Nan::Callback *completion_callback = new Nan::Callback(info[0].As<Function>());
    Local<String> js_file_path;
    if (!Nan::To<String>(info[1]).ToLocal(&js_file_path)) return;
    string file_path = *Nan::Utf8String(js_file_path);

    Local<String> js_encoding_name;
    if (!Nan::To<String>(info[2]).ToLocal(&js_encoding_name)) return;
    string encoding_name = *Nan::Utf8String(info[2].As<String>());

    Nan::AsyncQueueWorker(new BaseTextComparisonWorker(
      completion_callback,
      text_buffer.create_snapshot(),
      move(file_path),
      move(encoding_name)
    ));
  } else {
    auto file_contents = Nan::ObjectWrap::Unwrap<TextWriter>(Nan::To<Object>(info[1]).ToLocalChecked())->get_text();
    bool result = std::equal(file_contents.begin(), file_contents.end(), text_buffer.base_text().begin());
    Local<Value> argv[] = {Nan::Null(), Nan::New<Boolean>(result)};
    auto callback = info[0].As<Function>();

    #if (V8_MAJOR_VERSION > 9 || (V8_MAJOR_VERSION == 9 && V8_MINOR_VERION > 4))
      Nan::Call(callback, callback->GetCreationContext().ToLocalChecked()->Global(), 2, argv);
    #else
      Nan::Call(callback, callback->CreationContext()->Global(), 2, argv);
    #endif
  }
}

class SaveWorker : public Nan::AsyncWorker {
  TextBuffer::Snapshot *snapshot;
  string file_name;
  string encoding_name;
  optional<Error> error;

 public:
  SaveWorker(Nan::Callback *completion_callback, TextBuffer::Snapshot *snapshot,
             string &&file_name, string &&encoding_name) :
    AsyncWorker(completion_callback, "TextBuffer.save"),
    snapshot{snapshot},
    file_name{file_name},
    encoding_name(encoding_name) {}

  void Execute() {
    auto conversion = transcoding_to(encoding_name.c_str());
    if (!conversion) {
      error = Error{INVALID_ENCODING, nullptr};
      return;
    }

    FILE *file = open_file(file_name, "wb+");
    if (!file) {
      error = Error{errno, "open"};
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
        error = Error{errno, "write"};
        fclose(file);
        return;
      }
    }

    fclose(file);
  }

  Local<Value> Finish() {
    if (error) {
      delete snapshot;
      return error_to_js(*error, encoding_name, file_name);
    } else {
      snapshot->flush_preceding_changes();
      delete snapshot;
      return Nan::Null();
    }
  }

  void HandleOKCallback() {
    Local<Value> argv[] = {Finish()};
    callback->Call(1, argv, async_resource);
  }
};

void TextBufferWrapper::save(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;

  Local<String> js_file_path;
  if (!Nan::To<String>(info[0]).ToLocal(&js_file_path)) return;
  string file_path = *Nan::Utf8String(js_file_path);

  Local<String> js_encoding_name;
  if (!Nan::To<String>(info[1]).ToLocal(&js_encoding_name)) return;
  string encoding_name = *Nan::Utf8String(info[1].As<String>());

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

void TextBufferWrapper::get_snapshot(const Nan::FunctionCallbackInfo<Value> &info) {
  Nan::HandleScope scope;
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  auto snapshot = text_buffer.create_snapshot();
  info.GetReturnValue().Set(TextBufferSnapshotWrapper::new_instance(info.This(), reinterpret_cast<void *>(snapshot)));
}

void TextBufferWrapper::dot_graph(const Nan::FunctionCallbackInfo<Value> &info) {
  auto &text_buffer = Nan::ObjectWrap::Unwrap<TextBufferWrapper>(info.This())->text_buffer;
  info.GetReturnValue().Set(Nan::New<String>(text_buffer.get_dot_graph()).ToLocalChecked());
}

void TextBufferWrapper::cancel_queued_workers() {
  for (auto worker : outstanding_workers) {
    worker->CancelIfQueued();
  }
  outstanding_workers.clear();
}
