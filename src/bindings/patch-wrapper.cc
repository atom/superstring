#include "noop.h"
#include "patch-wrapper.h"
#include <memory>
#include <sstream>
#include <vector>
#include "point-wrapper.h"
#include "text-wrapper.h"

using namespace v8;
using std::move;
using std::vector;

static Nan::Persistent<String> new_text_string;
static Nan::Persistent<String> old_text_string;
static Nan::Persistent<v8::Function> change_wrapper_constructor;
static Nan::Persistent<v8::FunctionTemplate> patch_wrapper_constructor_template;
static Nan::Persistent<v8::Function> patch_wrapper_constructor;

class ChangeWrapper : public Nan::ObjectWrap {
 public:
  static void init() {
    new_text_string.Reset(Nan::New("newText").ToLocalChecked());
    old_text_string.Reset(Nan::New("oldText").ToLocalChecked());
    static Nan::Persistent<String> old_text_string;

    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
    constructor_template->SetClassName(Nan::New<String>("Change").ToLocalChecked());
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    const auto &instance_template = constructor_template->InstanceTemplate();
    Nan::SetAccessor(instance_template, Nan::New("oldStart").ToLocalChecked(), get_old_start);
    Nan::SetAccessor(instance_template, Nan::New("newStart").ToLocalChecked(), get_new_start);
    Nan::SetAccessor(instance_template, Nan::New("oldEnd").ToLocalChecked(), get_old_end);
    Nan::SetAccessor(instance_template, Nan::New("newEnd").ToLocalChecked(), get_new_end);

    const auto &prototype_template = constructor_template->PrototypeTemplate();
    prototype_template->Set(Nan::New<String>("toString").ToLocalChecked(), Nan::New<FunctionTemplate>(to_string));
    change_wrapper_constructor.Reset(constructor_template->GetFunction());
  }

  static Local<Value> FromChange(Patch::Change change) {
    Local<Object> result;
    if (Nan::NewInstance(Nan::New(change_wrapper_constructor)).ToLocal(&result)) {
      (new ChangeWrapper(change))->Wrap(result);
      if (change.new_text) {
        result->Set(Nan::New(new_text_string), TextWrapper::text_to_js(*change.new_text));
      }
      if (change.old_text) {
        result->Set(Nan::New(old_text_string), TextWrapper::text_to_js(*change.old_text));
      }
      return result;
    } else {
      return Nan::Null();
    }
  }

 private:
  ChangeWrapper(Patch::Change change) : change(change) {}

  static void construct(const Nan::FunctionCallbackInfo<Value> &info) {}

  static void get_old_start(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
    info.GetReturnValue().Set(PointWrapper::from_point(change.old_start));
  }

  static void get_new_start(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
    info.GetReturnValue().Set(PointWrapper::from_point(change.new_start));
  }

  static void get_old_end(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
    info.GetReturnValue().Set(PointWrapper::from_point(change.old_end));
  }

  static void get_new_end(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
    info.GetReturnValue().Set(PointWrapper::from_point(change.new_end));
  }

  static void get_preceding_old_text_length(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
    info.GetReturnValue().Set(Nan::New<Number>(change.preceding_old_text_size));
  }

  static void get_preceding_new_text_length(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
    Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
    info.GetReturnValue().Set(Nan::New<Number>(change.preceding_new_text_size));
  }

  static void to_string(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
    std::stringstream result;
    result << change;
    info.GetReturnValue().Set(Nan::New<String>(result.str()).ToLocalChecked());
  }

  Patch::Change change;
};

void PatchWrapper::init(Local<Object> exports) {
  ChangeWrapper::init();

  Local<FunctionTemplate> constructor_template_local = Nan::New<FunctionTemplate>(construct);
  constructor_template_local->SetClassName(Nan::New<String>("Patch").ToLocalChecked());
  constructor_template_local->Set(Nan::New("deserialize").ToLocalChecked(), Nan::New<FunctionTemplate>(deserialize));
  constructor_template_local->Set(Nan::New("compose").ToLocalChecked(), Nan::New<FunctionTemplate>(compose));
  constructor_template_local->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template_local->PrototypeTemplate();
  prototype_template->Set(Nan::New("delete").ToLocalChecked(), Nan::New<FunctionTemplate>(noop));
  prototype_template->Set(Nan::New("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(splice));
  prototype_template->Set(Nan::New("spliceOld").ToLocalChecked(), Nan::New<FunctionTemplate>(splice_old));
  prototype_template->Set(Nan::New("copy").ToLocalChecked(), Nan::New<FunctionTemplate>(copy));
  prototype_template->Set(Nan::New("invert").ToLocalChecked(), Nan::New<FunctionTemplate>(invert));
  prototype_template->Set(Nan::New("getChanges").ToLocalChecked(), Nan::New<FunctionTemplate>(get_changes));
  prototype_template->Set(Nan::New("getChangesInOldRange").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(get_changes_in_old_range));
  prototype_template->Set(Nan::New("getChangesInNewRange").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(get_changes_in_new_range));
  prototype_template->Set(Nan::New("changeForOldPosition").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(change_for_old_position));
  prototype_template->Set(Nan::New("changeForNewPosition").ToLocalChecked(),
                          Nan::New<FunctionTemplate>(change_for_new_position));
  prototype_template->Set(Nan::New("serialize").ToLocalChecked(), Nan::New<FunctionTemplate>(serialize));
  prototype_template->Set(Nan::New("getDotGraph").ToLocalChecked(), Nan::New<FunctionTemplate>(get_dot_graph));
  prototype_template->Set(Nan::New("getJSON").ToLocalChecked(), Nan::New<FunctionTemplate>(get_json));
  prototype_template->Set(Nan::New("rebalance").ToLocalChecked(), Nan::New<FunctionTemplate>(rebalance));
  prototype_template->Set(Nan::New("getChangeCount").ToLocalChecked(), Nan::New<FunctionTemplate>(get_change_count));
  patch_wrapper_constructor_template.Reset(constructor_template_local);
  patch_wrapper_constructor.Reset(constructor_template_local->GetFunction());
  exports->Set(Nan::New("Patch").ToLocalChecked(), Nan::New(patch_wrapper_constructor));
}

PatchWrapper::PatchWrapper(Patch &&patch) : patch{std::move(patch)} {}

Local<Value> PatchWrapper::from_patch(Patch &&patch) {
  Local<Object> result;
  if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
    (new PatchWrapper(move(patch)))->Wrap(result);
    return result;
  } else {
    return Nan::Null();
  }
}

void PatchWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  bool merges_adjacent_changes = true;
  Local<Object> options;

  if (info.Length() > 0 && Nan::To<Object>(info[0]).ToLocal(&options)) {
    Local<Boolean> js_merge_adjacent_changes;
    if (Nan::To<Boolean>(options->Get(Nan::New("mergeAdjacentChanges").ToLocalChecked()))
            .ToLocal(&js_merge_adjacent_changes)) {
      merges_adjacent_changes = js_merge_adjacent_changes->BooleanValue();
    }
  }
  PatchWrapper *patch = new PatchWrapper(Patch{merges_adjacent_changes});
  patch->Wrap(info.This());
}

void PatchWrapper::splice(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> deletion_extent = PointWrapper::point_from_js(info[1]);
  optional<Point> insertion_extent = PointWrapper::point_from_js(info[2]);

  if (start && deletion_extent && insertion_extent) {
    optional<Text> deleted_text;
    optional<Text> inserted_text;

    if (info.Length() >= 4) {
      deleted_text = TextWrapper::text_from_js(info[3]);
      if (!deleted_text) return;
    }

    if (info.Length() >= 5) {
      inserted_text = TextWrapper::text_from_js(info[4]);
      if (!inserted_text) return;
    }

    patch.splice(
      *start,
      *deletion_extent,
      *insertion_extent,
      move(deleted_text),
      move(inserted_text)
    );
  }
}

void PatchWrapper::splice_old(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> deletion_extent = PointWrapper::point_from_js(info[1]);
  optional<Point> insertion_extent = PointWrapper::point_from_js(info[2]);

  if (start && deletion_extent && insertion_extent) {
    patch.splice_old(*start, *deletion_extent, *insertion_extent);
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

void PatchWrapper::get_changes(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  Local<Array> js_result = Nan::New<Array>();

  size_t i = 0;
  for (auto change : patch.get_changes()) {
    js_result->Set(i++, ChangeWrapper::FromChange(change));
  }

  info.GetReturnValue().Set(js_result);
}

void PatchWrapper::get_changes_in_old_range(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    Local<Array> js_result = Nan::New<Array>();

    size_t i = 0;
    for (auto change : patch.grab_changes_in_old_range(*start, *end)) {
      js_result->Set(i++, ChangeWrapper::FromChange(change));
    }

    info.GetReturnValue().Set(js_result);
  }
}

void PatchWrapper::get_changes_in_new_range(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    Local<Array> js_result = Nan::New<Array>();

    size_t i = 0;
    for (auto change : patch.grab_changes_in_new_range(*start, *end)) {
      js_result->Set(i++, ChangeWrapper::FromChange(change));
    }

    info.GetReturnValue().Set(js_result);
  }
}

void PatchWrapper::change_for_old_position(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  if (start) {
    auto change = patch.grab_change_starting_before_old_position(*start);
    if (change) {
      info.GetReturnValue().Set(ChangeWrapper::FromChange(*change));
    } else {
      info.GetReturnValue().Set(Nan::Undefined());
    }
  }
}

void PatchWrapper::change_for_new_position(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  if (start) {
    auto change = patch.grab_change_starting_before_new_position(*start);
    if (change) {
      info.GetReturnValue().Set(ChangeWrapper::FromChange(*change));
    } else {
      info.GetReturnValue().Set(Nan::Undefined());
    }
  }
}

void PatchWrapper::serialize(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

  static vector<uint8_t> output;
  output.clear();
  Serializer serializer(output);
  patch.serialize(serializer);
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

      static vector<uint8_t> input;
      input.assign(data, data + node::Buffer::Length(info[0]));
      Deserializer deserializer(input);
      PatchWrapper *wrapper = new PatchWrapper(Patch{deserializer});
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

void PatchWrapper::get_change_count(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  uint32_t change_count = patch.get_change_count();
  info.GetReturnValue().Set(Nan::New<Number>(change_count));
}

void PatchWrapper::rebalance(const Nan::FunctionCallbackInfo<Value> &info) {
  Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
  patch.rebalance();
}
