#include "patch-wrapper.h"
#include <memory>
#include <sstream>
#include <vector>
#include "point-wrapper.h"

using namespace v8;
using std::vector;
using std::unique_ptr;

static Nan::Persistent<String> new_text_string;
static Nan::Persistent<String> old_text_string;
static Nan::Persistent<v8::Function> hunk_wrapper_constructor;
static Nan::Persistent<v8::FunctionTemplate> patch_wrapper_constructor_template;
static Nan::Persistent<v8::Function> patch_wrapper_constructor;
static vector<uint16_t> text_conversion_vector;

static unique_ptr<Text> text_from_js(Nan::MaybeLocal<String> maybe_string) {
  Local<String> string;
  if (!maybe_string.ToLocal(&string)) {
    Nan::ThrowTypeError("Expected a string.");
    return nullptr;
  }

  text_conversion_vector.clear();
  text_conversion_vector.resize(string->Length());
  string->Write(text_conversion_vector.data(), 0, -1, String::WriteOptions::NO_NULL_TERMINATION);

  unique_ptr<Text> text {new Text()};
  auto end = text_conversion_vector.end();
  auto line_start = text_conversion_vector.begin();
  auto line_end = line_start;

  while (line_end != end) {
    switch (*line_end) {
      case '\n': {
        auto &line = text->lines.back();
        line.content.assign(line_start, line_end);
        line.ending = LineEnding::LF;
        text->lines.push_back(Line{});
        ++line_end;
        line_start = line_end;
        break;
      }

      case '\r': {
        auto &line = text->lines.back();
        line.content.assign(line_start, line_end);
        if (line_end + 1 != end && *(line_end + 1) == '\n') {
          line.ending = LineEnding::CRLF;
          line_end += 2;
          line_start = line_end;
        } else {
          line.ending = LineEnding::CR;
          ++line_end;
          line_start = line_end;
        }
        text->lines.push_back(Line{});
        break;
      }

      default: {
        ++line_end;
        break;
      }
    }
  }

  auto &line = text->lines.back();
  line.content.assign(line_start, line_end);

  return text;
}

static Local<String> text_to_js(Text *text) {
  text_conversion_vector.clear();
  text->write(text_conversion_vector);
  return Nan::New<String>(text_conversion_vector.data(), text_conversion_vector.size()).ToLocalChecked();
}

class HunkWrapper : public Nan::ObjectWrap {
 public:
  static void init() {
    new_text_string.Reset(Nan::New("newText").ToLocalChecked());
    old_text_string.Reset(Nan::New("oldText").ToLocalChecked());
    static Nan::Persistent<String> old_text_string;

    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
    constructor_template->SetClassName(Nan::New<String>("Hunk").ToLocalChecked());
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    const auto &instance_template = constructor_template->InstanceTemplate();
    Nan::SetAccessor(instance_template, Nan::New("oldStart").ToLocalChecked(), get_old_start);
    Nan::SetAccessor(instance_template, Nan::New("newStart").ToLocalChecked(), get_new_start);
    Nan::SetAccessor(instance_template, Nan::New("oldEnd").ToLocalChecked(), get_old_end);
    Nan::SetAccessor(instance_template, Nan::New("newEnd").ToLocalChecked(), get_new_end);

    // Non-enumerable legacy properties for backward compatibility
    Nan::SetAccessor(instance_template, Nan::New("start").ToLocalChecked(), get_new_start, nullptr, Handle<Value>(),
                     AccessControl::DEFAULT, PropertyAttribute::DontEnum);
    Nan::SetAccessor(instance_template, Nan::New("oldExtent").ToLocalChecked(), get_old_extent, nullptr, Handle<Value>(),
                     AccessControl::DEFAULT, PropertyAttribute::DontEnum);
    Nan::SetAccessor(instance_template, Nan::New("newExtent").ToLocalChecked(), get_new_extent, nullptr, Handle<Value>(),
                     AccessControl::DEFAULT, PropertyAttribute::DontEnum);

    const auto &prototype_template = constructor_template->PrototypeTemplate();
    prototype_template->Set(Nan::New<String>("toString").ToLocalChecked(), Nan::New<FunctionTemplate>(to_string));
    hunk_wrapper_constructor.Reset(constructor_template->GetFunction());
  }

  static Local<Value> FromHunk(Patch::Hunk hunk) {
    Local<Object> result;
    if (Nan::NewInstance(Nan::New(hunk_wrapper_constructor)).ToLocal(&result)) {
      (new HunkWrapper(hunk))->Wrap(result);
      if (hunk.new_text) {
        result->Set(Nan::New(new_text_string), text_to_js(hunk.new_text));
      }
      if (hunk.old_text) {
        result->Set(Nan::New(old_text_string), text_to_js(hunk.old_text));
      }
      return result;
    } else {
      return Nan::Null();
    }
  }

 private:
  HunkWrapper(Patch::Hunk hunk) : hunk(hunk) {}

  static void construct(const Nan::FunctionCallbackInfo<Value> &info) {}

  static void get_old_start(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::from_point(hunk.old_start));
  }

  static void get_new_start(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::from_point(hunk.new_start));
  }

  static void get_old_end(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::from_point(hunk.old_end));
  }

  static void get_new_end(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::from_point(hunk.new_end));
  }

  static void get_old_extent(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::from_point(hunk.old_end.traversal(hunk.old_start)));
  }

  static void get_new_extent(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::from_point(hunk.new_end.traversal(hunk.new_start)));
  }

  static void to_string(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    std::stringstream result;
    result << hunk;
    info.GetReturnValue().Set(Nan::New<String>(result.str()).ToLocalChecked());
  }

  Patch::Hunk hunk;
};

void PatchWrapper::init(Local<Object> exports) {
  HunkWrapper::init();

  Local<FunctionTemplate> constructor_template_local = Nan::New<FunctionTemplate>(construct);
  constructor_template_local->SetClassName(Nan::New<String>("Patch").ToLocalChecked());
  constructor_template_local->Set(Nan::New("deserialize").ToLocalChecked(), Nan::New<FunctionTemplate>(deserialize));
  constructor_template_local->Set(Nan::New("compose").ToLocalChecked(), Nan::New<FunctionTemplate>(compose));
  constructor_template_local->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template_local->PrototypeTemplate();
  prototype_template->Set(Nan::New("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(splice));
  prototype_template->Set(Nan::New("spliceOld").ToLocalChecked(), Nan::New<FunctionTemplate>(splice_old));
  prototype_template->Set(Nan::New("copy").ToLocalChecked(), Nan::New<FunctionTemplate>(copy));
  prototype_template->Set(Nan::New("invert").ToLocalChecked(), Nan::New<FunctionTemplate>(invert));
  prototype_template->Set(Nan::New("getHunks").ToLocalChecked(), Nan::New<FunctionTemplate>(get_hunks));
  prototype_template->Set(Nan::New("getHunksInOldRange").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(get_hunks_in_old_range));
  prototype_template->Set(Nan::New("getHunksInNewRange").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(get_hunks_in_new_range));
  prototype_template->Set(Nan::New("hunkForOldPosition").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(hunk_for_old_position));
  prototype_template->Set(Nan::New("hunkForNewPosition").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(hunk_for_new_position));
  prototype_template->Set(Nan::New("serialize").ToLocalChecked(), Nan::New<FunctionTemplate>(serialize));
  prototype_template->Set(Nan::New("getDotGraph").ToLocalChecked(), Nan::New<FunctionTemplate>(get_dot_graph));
  prototype_template->Set(Nan::New("getJSON").ToLocalChecked(), Nan::New<FunctionTemplate>(get_json));
  prototype_template->Set(Nan::New("rebalance").ToLocalChecked(), Nan::New<FunctionTemplate>(rebalance));
  prototype_template->Set(Nan::New("getHunkCount").ToLocalChecked(), Nan::New<FunctionTemplate>(get_hunk_count));
  patch_wrapper_constructor_template.Reset(constructor_template_local);
  patch_wrapper_constructor.Reset(constructor_template_local->GetFunction());
  exports->Set(Nan::New("Patch").ToLocalChecked(), Nan::New(patch_wrapper_constructor));
}

PatchWrapper::PatchWrapper(Patch &&patch) : patch{std::move(patch)} {}

void PatchWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  bool merges_adjacent_hunks = true;
  Local<Object> options;

  if (info.Length() > 0 && Nan::To<Object>(info[0]).ToLocal(&options)) {
    Local<Boolean> js_merge_adjacent_hunks;
    if (Nan::To<Boolean>(options->Get(Nan::New("mergeAdjacentHunks").ToLocalChecked()))
            .ToLocal(&js_merge_adjacent_hunks)) {
      merges_adjacent_hunks = js_merge_adjacent_hunks->BooleanValue();
    }
  }
  PatchWrapper *patch = new PatchWrapper(Patch{merges_adjacent_hunks});
  patch->Wrap(info.This());
}

void PatchWrapper::splice(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> deletion_extent = PointWrapper::point_from_js(info[1]);
  optional<Point> insertion_extent = PointWrapper::point_from_js(info[2]);

  if (start && deletion_extent && insertion_extent) {
    unique_ptr<Text> deleted_text;
    unique_ptr<Text> inserted_text;

    if (info.Length() >= 4) {
      deleted_text = text_from_js(Nan::To<String>(info[3]));
      if (!deleted_text) return;
    }

    if (info.Length() >= 5) {
      inserted_text = text_from_js(Nan::To<String>(info[4]));
      if (!inserted_text) return;
    }

    if (!patch.splice(*start, *deletion_extent, *insertion_extent, move(deleted_text),
                      move(inserted_text))) {
      Nan::ThrowError("Can't splice into a frozen patch");
    }
  }
}

void PatchWrapper::splice_old(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> deletion_extent = PointWrapper::point_from_js(info[1]);
  optional<Point> insertion_extent = PointWrapper::point_from_js(info[2]);

  if (start && deletion_extent && insertion_extent) {
    if (!patch.splice_old(*start, *deletion_extent, *insertion_extent)) {
      Nan::ThrowError("Can't splice_old into a frozen patch");
    }
  }
}

void PatchWrapper::copy(const Nan::FunctionCallbackInfo<Value> &info) {
  Local<Object> result;
  if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
    auto wrapper = new PatchWrapper{patch.copy()};
    wrapper->Wrap(result);
    info.GetReturnValue().Set(result);
  }
}

void PatchWrapper::invert(const Nan::FunctionCallbackInfo<Value> &info) {
  Local<Object> result;
  if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
    auto wrapper = new PatchWrapper{patch.invert()};
    wrapper->Wrap(result);
    info.GetReturnValue().Set(result);
  }
}

void PatchWrapper::get_hunks(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  Local<Array> js_result = Nan::New<Array>();

  size_t i = 0;
  for (auto hunk : patch.get_hunks()) {
    js_result->Set(i++, HunkWrapper::FromHunk(hunk));
  }

  info.GetReturnValue().Set(js_result);
}

void PatchWrapper::get_hunks_in_old_range(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    Local<Array> js_result = Nan::New<Array>();

    size_t i = 0;
    for (auto hunk : patch.get_hunks_in_old_range(*start, *end)) {
      js_result->Set(i++, HunkWrapper::FromHunk(hunk));
    }

    info.GetReturnValue().Set(js_result);
  }
}

void PatchWrapper::get_hunks_in_new_range(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    Local<Array> js_result = Nan::New<Array>();

    size_t i = 0;
    for (auto hunk : patch.get_hunks_in_new_range(*start, *end)) {
      js_result->Set(i++, HunkWrapper::FromHunk(hunk));
    }

    info.GetReturnValue().Set(js_result);
  }
}

void PatchWrapper::hunk_for_old_position(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  if (start) {
    auto hunk = patch.hunk_for_old_position(*start);
    if (hunk) {
      info.GetReturnValue().Set(HunkWrapper::FromHunk(*hunk));
    } else {
      info.GetReturnValue().Set(Nan::Undefined());
    }
  }
}

void PatchWrapper::hunk_for_new_position(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  if (start) {
    auto hunk = patch.hunk_for_new_position(*start);
    if (hunk) {
      info.GetReturnValue().Set(HunkWrapper::FromHunk(*hunk));
    } else {
      info.GetReturnValue().Set(Nan::Undefined());
    }
  }
}

void PatchWrapper::serialize(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  static Serializer output;

  output.clear();
  patch.serialize(output);
  Local<Object> result;
  auto maybe_result =
      Nan::CopyBuffer(reinterpret_cast<char *>(output.data()), output.size());
  if (maybe_result.ToLocal(&result)) {
    info.GetReturnValue().Set(result);
  }
}

void PatchWrapper::deserialize(const Nan::FunctionCallbackInfo<Value> &info) {
  Local<Object> result;
  if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
    if (info[0]->IsUint8Array()) {
      auto *data = node::Buffer::Data(info[0]);

      static Serializer input;
      input.assign(data, data + node::Buffer::Length(info[0]));
      PatchWrapper *wrapper = new PatchWrapper(Patch{input});
      wrapper->Wrap(result);
      info.GetReturnValue().Set(result);
    }
  }
}

void PatchWrapper::compose(const Nan::FunctionCallbackInfo<Value> &info) {
  Local<Object> result;
  if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
    Local<Array> js_patches = Local<Array>::Cast(info[0]);
    if (!js_patches->IsArray()) {
      Nan::ThrowTypeError("Compose requires an array of patches");
      return;
    }

    vector<const Patch *> patches;
    for (uint32_t i = 0, n = js_patches->Length(); i < n; i++) {
      Local<Object> js_patch = Local<Object>::Cast(js_patches->Get(i));
      if (!Nan::New(patch_wrapper_constructor_template)->HasInstance(js_patch)) {
        Nan::ThrowTypeError("Patch.compose must be called with an array of patches");
        return;
      }

      Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(js_patch)->patch;
      patches.push_back(&patch);
    }

    auto wrapper = new PatchWrapper{Patch{patches}};
    wrapper->Wrap(result);
    info.GetReturnValue().Set(result);
  }
}

void PatchWrapper::get_dot_graph(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  std::string graph = patch.get_dot_graph();
  info.GetReturnValue().Set(Nan::New<String>(graph).ToLocalChecked());
}

void PatchWrapper::get_json(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  std::string graph = patch.get_json();
  info.GetReturnValue().Set(Nan::New<String>(graph).ToLocalChecked());
}

void PatchWrapper::get_hunk_count(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  uint32_t hunk_count = patch.get_hunk_count();
  info.GetReturnValue().Set(Nan::New<Number>(hunk_count));
}

void PatchWrapper::rebalance(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  patch.rebalance();
}
