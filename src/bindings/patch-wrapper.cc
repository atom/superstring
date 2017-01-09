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

static unique_ptr<Text> TextFromJS(Nan::MaybeLocal<String> maybe_string) {
  Local<String> string;
  if (!maybe_string.ToLocal(&string)) {
    Nan::ThrowTypeError("Expected a string.");
    return nullptr;
  }

  unique_ptr<Text> text {new Text(string->Length())};
  string->Write(text->data(), 0, -1, String::WriteOptions::NO_NULL_TERMINATION);
  return text;
}

static Local<String> TextToJS(Text *text) {
  return Nan::New<String>(text->data(), text->size()).ToLocalChecked();
}

class HunkWrapper : public Nan::ObjectWrap {
 public:
  static void Init() {
    new_text_string.Reset(Nan::New("newText").ToLocalChecked());
    old_text_string.Reset(Nan::New("oldText").ToLocalChecked());
    static Nan::Persistent<String> old_text_string;

    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);
    constructor_template->SetClassName(Nan::New<String>("Hunk").ToLocalChecked());
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    const auto &instance_template = constructor_template->InstanceTemplate();
    Nan::SetAccessor(instance_template, Nan::New("oldStart").ToLocalChecked(), GetOldStart);
    Nan::SetAccessor(instance_template, Nan::New("newStart").ToLocalChecked(), GetNewStart);
    Nan::SetAccessor(instance_template, Nan::New("oldEnd").ToLocalChecked(), GetOldEnd);
    Nan::SetAccessor(instance_template, Nan::New("newEnd").ToLocalChecked(), GetNewEnd);

    // Non-enumerable legacy properties for backward compatibility
    Nan::SetAccessor(instance_template, Nan::New("start").ToLocalChecked(), GetNewStart, nullptr, Handle<Value>(),
                     AccessControl::DEFAULT, PropertyAttribute::DontEnum);
    Nan::SetAccessor(instance_template, Nan::New("oldExtent").ToLocalChecked(), GetOldExtent, nullptr, Handle<Value>(),
                     AccessControl::DEFAULT, PropertyAttribute::DontEnum);
    Nan::SetAccessor(instance_template, Nan::New("newExtent").ToLocalChecked(), GetNewExtent, nullptr, Handle<Value>(),
                     AccessControl::DEFAULT, PropertyAttribute::DontEnum);

    const auto &prototype_template = constructor_template->PrototypeTemplate();
    prototype_template->Set(Nan::New<String>("toString").ToLocalChecked(), Nan::New<FunctionTemplate>(ToString));
    hunk_wrapper_constructor.Reset(constructor_template->GetFunction());
  }

  static Local<Value> FromHunk(Patch::Hunk hunk) {
    Local<Object> result;
    if (Nan::NewInstance(Nan::New(hunk_wrapper_constructor)).ToLocal(&result)) {
      (new HunkWrapper(hunk))->Wrap(result);
      if (hunk.new_text) {
        result->Set(Nan::New(new_text_string), TextToJS(hunk.new_text));
      }
      if (hunk.old_text) {
        result->Set(Nan::New(old_text_string), TextToJS(hunk.old_text));
      }
      return result;
    } else {
      return Nan::Null();
    }
  }

 private:
  HunkWrapper(Patch::Hunk hunk) : hunk(hunk) {}

  static void New(const Nan::FunctionCallbackInfo<Value> &info) {}

  static void GetOldStart(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::FromPoint(hunk.old_start));
  }

  static void GetNewStart(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::FromPoint(hunk.new_start));
  }

  static void GetOldEnd(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::FromPoint(hunk.old_end));
  }

  static void GetNewEnd(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::FromPoint(hunk.new_end));
  }

  static void GetOldExtent(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::FromPoint(hunk.old_end.Traversal(hunk.old_start)));
  }

  static void GetNewExtent(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::FromPoint(hunk.new_end.Traversal(hunk.new_start)));
  }

  static void ToString(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch::Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    std::stringstream result;
    result << hunk;
    info.GetReturnValue().Set(Nan::New<String>(result.str()).ToLocalChecked());
  }

  Patch::Hunk hunk;
};

void PatchWrapper::Init(Local<Object> exports) {
  HunkWrapper::Init();

  Local<FunctionTemplate> constructor_template_local = Nan::New<FunctionTemplate>(New);
  constructor_template_local->SetClassName(Nan::New<String>("Patch").ToLocalChecked());
  constructor_template_local->Set(Nan::New("deserialize").ToLocalChecked(), Nan::New<FunctionTemplate>(Deserialize));
  constructor_template_local->Set(Nan::New("compose").ToLocalChecked(), Nan::New<FunctionTemplate>(Compose));
  constructor_template_local->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template_local->PrototypeTemplate();
  prototype_template->Set(Nan::New("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(Splice));
  prototype_template->Set(Nan::New("spliceOld").ToLocalChecked(), Nan::New<FunctionTemplate>(SpliceOld));
  prototype_template->Set(Nan::New("copy").ToLocalChecked(), Nan::New<FunctionTemplate>(Copy));
  prototype_template->Set(Nan::New("invert").ToLocalChecked(), Nan::New<FunctionTemplate>(Invert));
  prototype_template->Set(Nan::New("getHunks").ToLocalChecked(), Nan::New<FunctionTemplate>(GetHunks));
  prototype_template->Set(Nan::New("getHunksInOldRange").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(GetHunksInOldRange));
  prototype_template->Set(Nan::New("getHunksInNewRange").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(GetHunksInNewRange));
  prototype_template->Set(Nan::New("hunkForOldPosition").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(HunkForOldPosition));
  prototype_template->Set(Nan::New("hunkForNewPosition").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(HunkForNewPosition));
  prototype_template->Set(Nan::New("serialize").ToLocalChecked(), Nan::New<FunctionTemplate>(Serialize));
  prototype_template->Set(Nan::New("getDotGraph").ToLocalChecked(), Nan::New<FunctionTemplate>(GetDotGraph));
  prototype_template->Set(Nan::New("getJSON").ToLocalChecked(), Nan::New<FunctionTemplate>(GetJSON));
  prototype_template->Set(Nan::New("rebalance").ToLocalChecked(), Nan::New<FunctionTemplate>(Rebalance));
  prototype_template->Set(Nan::New("getHunkCount").ToLocalChecked(), Nan::New<FunctionTemplate>(GetHunkCount));
  patch_wrapper_constructor_template.Reset(constructor_template_local);
  patch_wrapper_constructor.Reset(constructor_template_local->GetFunction());
  exports->Set(Nan::New("Patch").ToLocalChecked(), Nan::New(patch_wrapper_constructor));
}

PatchWrapper::PatchWrapper(Patch &&patch) : patch{std::move(patch)} {}

void PatchWrapper::New(const Nan::FunctionCallbackInfo<Value> &info) {
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

void PatchWrapper::Splice(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  optional<Point> deletion_extent = PointWrapper::PointFromJS(info[1]);
  optional<Point> insertion_extent = PointWrapper::PointFromJS(info[2]);

  if (start && deletion_extent && insertion_extent) {
    unique_ptr<Text> deleted_text;
    unique_ptr<Text> inserted_text;

    if (info.Length() >= 4) {
      deleted_text = TextFromJS(Nan::To<String>(info[3]));
      if (!deleted_text) return;
    }

    if (info.Length() >= 5) {
      inserted_text = TextFromJS(Nan::To<String>(info[4]));
      if (!inserted_text) return;
    }

    if (!patch.Splice(*start, *deletion_extent, *insertion_extent, move(deleted_text),
                      move(inserted_text))) {
      Nan::ThrowError("Can't splice into a frozen patch");
    }
  }
}

void PatchWrapper::SpliceOld(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  optional<Point> deletion_extent = PointWrapper::PointFromJS(info[1]);
  optional<Point> insertion_extent = PointWrapper::PointFromJS(info[2]);

  if (start && deletion_extent && insertion_extent) {
    if (!patch.SpliceOld(*start, *deletion_extent, *insertion_extent)) {
      Nan::ThrowError("Can't spliceOld into a frozen patch");
    }
  }
}

void PatchWrapper::Copy(const Nan::FunctionCallbackInfo<Value> &info) {
  Local<Object> result;
  if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
    auto wrapper = new PatchWrapper{patch.Copy()};
    wrapper->Wrap(result);
    info.GetReturnValue().Set(result);
  }
}

void PatchWrapper::Invert(const Nan::FunctionCallbackInfo<Value> &info) {
  Local<Object> result;
  if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
    auto wrapper = new PatchWrapper{patch.Invert()};
    wrapper->Wrap(result);
    info.GetReturnValue().Set(result);
  }
}

void PatchWrapper::GetHunks(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  Local<Array> js_result = Nan::New<Array>();

  size_t i = 0;
  for (auto hunk : patch.GetHunks()) {
    js_result->Set(i++, HunkWrapper::FromHunk(hunk));
  }

  info.GetReturnValue().Set(js_result);
}

void PatchWrapper::GetHunksInOldRange(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  optional<Point> end = PointWrapper::PointFromJS(info[1]);

  if (start && end) {
    Local<Array> js_result = Nan::New<Array>();

    size_t i = 0;
    for (auto hunk : patch.GetHunksInOldRange(*start, *end)) {
      js_result->Set(i++, HunkWrapper::FromHunk(hunk));
    }

    info.GetReturnValue().Set(js_result);
  }
}

void PatchWrapper::GetHunksInNewRange(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  optional<Point> end = PointWrapper::PointFromJS(info[1]);

  if (start && end) {
    Local<Array> js_result = Nan::New<Array>();

    size_t i = 0;
    for (auto hunk : patch.GetHunksInNewRange(*start, *end)) {
      js_result->Set(i++, HunkWrapper::FromHunk(hunk));
    }

    info.GetReturnValue().Set(js_result);
  }
}

void PatchWrapper::HunkForOldPosition(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  if (start) {
    auto hunk = patch.HunkForOldPosition(*start);
    if (hunk) {
      info.GetReturnValue().Set(HunkWrapper::FromHunk(*hunk));
    } else {
      info.GetReturnValue().Set(Nan::Undefined());
    }
  }
}

void PatchWrapper::HunkForNewPosition(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  optional<Point> start = PointWrapper::PointFromJS(info[0]);
  if (start) {
    auto hunk = patch.HunkForNewPosition(*start);
    if (hunk) {
      info.GetReturnValue().Set(HunkWrapper::FromHunk(*hunk));
    } else {
      info.GetReturnValue().Set(Nan::Undefined());
    }
  }
}

void PatchWrapper::Serialize(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  static vector<uint8_t> serialization_vector;

  serialization_vector.clear();
  patch.Serialize(&serialization_vector);
  Local<Object> result;
  auto maybe_result =
      Nan::CopyBuffer(reinterpret_cast<char *>(serialization_vector.data()), serialization_vector.size());
  if (maybe_result.ToLocal(&result)) {
    info.GetReturnValue().Set(result);
  }
}

void PatchWrapper::Deserialize(const Nan::FunctionCallbackInfo<Value> &info) {
  Local<Object> result;
  if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
    if (info[0]->IsUint8Array()) {
      auto *data = node::Buffer::Data(info[0]);
      static vector<uint8_t> serialization_vector;
      serialization_vector.assign(data, data + node::Buffer::Length(info[0]));
      PatchWrapper *wrapper = new PatchWrapper(Patch{serialization_vector});
      wrapper->Wrap(result);
      info.GetReturnValue().Set(result);
    }
  }
}

void PatchWrapper::Compose(const Nan::FunctionCallbackInfo<Value> &info) {
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

void PatchWrapper::GetDotGraph(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  std::string graph = patch.GetDotGraph();
  info.GetReturnValue().Set(Nan::New<String>(graph).ToLocalChecked());
}

void PatchWrapper::GetJSON(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  std::string graph = patch.GetJSON();
  info.GetReturnValue().Set(Nan::New<String>(graph).ToLocalChecked());
}

void PatchWrapper::GetHunkCount(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  uint32_t hunk_count = patch.GetHunkCount();
  info.GetReturnValue().Set(Nan::New<Number>(hunk_count));
}

void PatchWrapper::Rebalance(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  patch.Rebalance();
}
