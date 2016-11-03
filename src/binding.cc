#include <cmath>
#include <sstream>
#include <string>
#include "nan.h"
#include "point.h"
#include "hunk.h"
#include "patch.h"

using namespace v8;

static Nan::Persistent<String> row_string;
static Nan::Persistent<String> column_string;

static Nan::Maybe<Point> PointFromJS(Nan::MaybeLocal<Object> maybe_object) {
  Local<Object> object;
  if (!maybe_object.ToLocal(&object)) {
    Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
    return Nan::Nothing<Point>();
  }

  Nan::MaybeLocal<Integer> maybe_row = Nan::To<Integer>(object->Get(Nan::New(row_string)));
  Local<Integer> js_row;
  if (!maybe_row.ToLocal(&js_row)) {
    Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
    return Nan::Nothing<Point>();
  }

  Nan::MaybeLocal<Integer> maybe_column = Nan::To<Integer>(object->Get(Nan::New(column_string)));
  Local<Integer> js_column;
  if (!maybe_column.ToLocal(&js_column)) {
    Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
    return Nan::Nothing<Point>();
  }

  unsigned row, column;
  if (std::isfinite(js_row->NumberValue())) {
    row = static_cast<unsigned>(js_row->Int32Value());
  } else {
    row = UINT_MAX;
  }

  if (std::isfinite(js_column->NumberValue())) {
    column = static_cast<unsigned>(js_column->Int32Value());
  } else {
    column = UINT_MAX;
  }

  return Nan::Just(Point(row, column));
}

class PointWrapper : public Nan::ObjectWrap {
public:
  static void Init() {
    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);
    constructor_template->SetClassName(Nan::New<String>("Point").ToLocalChecked());
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    Nan::SetAccessor(constructor_template->InstanceTemplate(), Nan::New(row_string), GetRow);
    Nan::SetAccessor(constructor_template->InstanceTemplate(), Nan::New(column_string), GetColumn);
    constructor.Reset(constructor_template->GetFunction());
  }

  static Local<Value> FromPoint(Point point) {
    Local<Object> result;
    if (Nan::New(constructor)->NewInstance(Nan::GetCurrentContext()).ToLocal(&result)) {
      (new PointWrapper(point))->Wrap(result);
      return result;
    } else {
      return Nan::Null();
    }
  }

private:
  PointWrapper(Point point) : point(point) {}

  static void New(const Nan::FunctionCallbackInfo<Value> &info) {}

  static void GetRow(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value>& info) {
    PointWrapper *wrapper = Nan::ObjectWrap::Unwrap<PointWrapper>(info.This());
    Point &point = wrapper->point;
    info.GetReturnValue().Set(Nan::New(point.row));
  }

  static void GetColumn(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value>& info) {
    PointWrapper *wrapper = Nan::ObjectWrap::Unwrap<PointWrapper>(info.This());
    Point &point = wrapper->point;
    info.GetReturnValue().Set(Nan::New(point.column));
  }

  static Nan::Persistent<v8::Function> constructor;
  Point point;
};

class HunkWrapper : public Nan::ObjectWrap {
public:
  static void Init() {
    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);
    constructor_template->SetClassName(Nan::New<String>("Hunk").ToLocalChecked());
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    const auto &instance_template = constructor_template->InstanceTemplate();
    Nan::SetAccessor(instance_template, Nan::New("oldStart").ToLocalChecked(), GetOldStart);
    Nan::SetAccessor(instance_template, Nan::New("newStart").ToLocalChecked(), GetNewStart);
    Nan::SetAccessor(instance_template, Nan::New("oldEnd").ToLocalChecked(), GetOldEnd);
    Nan::SetAccessor(instance_template, Nan::New("newEnd").ToLocalChecked(), GetNewEnd);

    const auto &prototype_template = constructor_template->PrototypeTemplate();
    prototype_template->Set(Nan::New<String>("toString").ToLocalChecked(), Nan::New<FunctionTemplate>(ToString));
    constructor.Reset(constructor_template->GetFunction());
  }

  static Local<Value> FromHunk(Hunk hunk) {
    Local<Object> result;
    if (Nan::NewInstance(Nan::New(constructor)).ToLocal(&result)) {
      (new HunkWrapper(hunk))->Wrap(result);
      return result;
    } else {
      return Nan::Null();
    }
  }

private:
  HunkWrapper(Hunk hunk) : hunk(hunk) {}

  static void New(const Nan::FunctionCallbackInfo<Value> &info) {}

  static void GetOldStart(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value>& info) {
    Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::FromPoint(hunk.old_start));
  }

  static void GetNewStart(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value>& info) {
    Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::FromPoint(hunk.new_start));
  }

  static void GetOldEnd(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value>& info) {
    Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::FromPoint(hunk.old_end));
  }

  static void GetNewEnd(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value>& info) {
    Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    info.GetReturnValue().Set(PointWrapper::FromPoint(hunk.new_end));
  }

  static void ToString(const Nan::FunctionCallbackInfo<Value> &info) {
    Hunk &hunk = Nan::ObjectWrap::Unwrap<HunkWrapper>(info.This())->hunk;
    std::stringstream result;
    result << hunk;
    info.GetReturnValue().Set(Nan::New<String>(result.str()).ToLocalChecked());
  }

  static Nan::Persistent<v8::Function> constructor;
  Hunk hunk;
};

namespace clip_mode {
  static Nan::Persistent<v8::Symbol> closest;
  static Nan::Persistent<v8::Symbol> backward;
  static Nan::Persistent<v8::Symbol> forward;

  static void Init() {
    closest.Reset(v8::Symbol::New(Isolate::GetCurrent(), Nan::New("ClipMode.CLOSEST").ToLocalChecked()));
    backward.Reset(v8::Symbol::New(Isolate::GetCurrent(), Nan::New("ClipMode.BACKWARD").ToLocalChecked()));
    forward.Reset(v8::Symbol::New(Isolate::GetCurrent(), Nan::New("ClipMode.FORWARD").ToLocalChecked()));
  }
}

class PatchWrapper : public Nan::ObjectWrap {
public:
  static void Init(Local<Object> exports, Local<Object> module) {
    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);
    constructor_template->SetClassName(Nan::New<String>("Patch").ToLocalChecked());
    constructor_template->Set(Nan::New("deserialize").ToLocalChecked(), Nan::New<FunctionTemplate>(Deserialize));
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    const auto &prototype_template = constructor_template->PrototypeTemplate();
    prototype_template->Set(Nan::New("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(Splice));
    prototype_template->Set(Nan::New("getHunks").ToLocalChecked(), Nan::New<FunctionTemplate>(GetHunks));
    prototype_template->Set(Nan::New("getHunksInOldRange").ToLocalChecked(), Nan::New<FunctionTemplate>(GetHunksInOldRange));
    prototype_template->Set(Nan::New("getHunksInNewRange").ToLocalChecked(), Nan::New<FunctionTemplate>(GetHunksInNewRange));
    prototype_template->Set(Nan::New("translateOldPosition").ToLocalChecked(), Nan::New<FunctionTemplate>(TranslateOldPosition));
    prototype_template->Set(Nan::New("translateNewPosition").ToLocalChecked(), Nan::New<FunctionTemplate>(TranslateNewPosition));
    prototype_template->Set(Nan::New("serialize").ToLocalChecked(), Nan::New<FunctionTemplate>(Serialize));
    prototype_template->Set(Nan::New("printDotGraph").ToLocalChecked(), Nan::New<FunctionTemplate>(PrintDotGraph));
    constructor.Reset(constructor_template->GetFunction());

    Local<Function> constructor_local = Nan::New(constructor);
    auto js_clip_mode_enum = Nan::New<Object>();
    js_clip_mode_enum->Set(Nan::New("CLOSEST").ToLocalChecked(), Nan::New(clip_mode::closest));
    js_clip_mode_enum->Set(Nan::New("BACKWARD").ToLocalChecked(), Nan::New(clip_mode::backward));
    js_clip_mode_enum->Set(Nan::New("FORWARD").ToLocalChecked(), Nan::New(clip_mode::forward));
    constructor_local->Set(Nan::New("ClipMode").ToLocalChecked(), js_clip_mode_enum);

    module->Set(Nan::New("exports").ToLocalChecked(), constructor_local);
  }

private:
  PatchWrapper() : patch{} {}

  PatchWrapper(const std::vector<uint8_t> &data) : patch(data) {}

  static void New(const Nan::FunctionCallbackInfo<Value> &info) {
    PatchWrapper *patch = new PatchWrapper();
    patch->Wrap(info.This());
  }

  static void Splice(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Nan::Maybe<Point> deletion_extent = PointFromJS(Nan::To<Object>(info[1]));
    Nan::Maybe<Point> insertion_extent = PointFromJS(Nan::To<Object>(info[2]));

    if (start.IsJust() && deletion_extent.IsJust() && insertion_extent.IsJust()) {
      if (!patch.Splice(start.FromJust(), deletion_extent.FromJust(), insertion_extent.FromJust())) {
        Nan::ThrowError("Can't splice into a frozen patch");
      }
    }
  }

  static void GetHunks(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

    Local<Array> js_result = Nan::New<Array>();

    size_t i = 0;
    for (Hunk hunk : patch.GetHunks()) {
      js_result->Set(i++, HunkWrapper::FromHunk(hunk));
    }

    info.GetReturnValue().Set(js_result);
  }

  static void GetHunksInOldRange(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Nan::Maybe<Point> end = PointFromJS(Nan::To<Object>(info[1]));

    if (start.IsJust() && end.IsJust()) {
      Local<Array> js_result = Nan::New<Array>();

      size_t i = 0;
      for (Hunk hunk : patch.GetHunksInOldRange(start.FromJust(), end.FromJust())) {
        js_result->Set(i++, HunkWrapper::FromHunk(hunk));
      }

      info.GetReturnValue().Set(js_result);
    }
  }

  static void GetHunksInNewRange(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Nan::Maybe<Point> end = PointFromJS(Nan::To<Object>(info[1]));

    if (start.IsJust() && end.IsJust()) {
      Local<Array> js_result = Nan::New<Array>();

      size_t i = 0;
      for (Hunk hunk : patch.GetHunksInNewRange(start.FromJust(), end.FromJust())) {
        js_result->Set(i++, HunkWrapper::FromHunk(hunk));
      }

      info.GetReturnValue().Set(js_result);
    }
  }

  static Patch::ClipMode ClipModeFromJS(Local<Value> js_clip_mode) {
    if (js_clip_mode->Equals(Nan::New(clip_mode::backward))) {
      return Patch::ClipMode::kBackward;
    } else if (js_clip_mode->Equals(Nan::New(clip_mode::forward))) {
      return Patch::ClipMode::kForward;
    } else {
      return Patch::ClipMode::kClosest;
    }
  }

  static void TranslateOldPosition(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Patch::ClipMode clip_mode = ClipModeFromJS(info[1]);

    if (start.IsJust()) {
      Point result = patch.TranslateOldPosition(start.FromJust(), clip_mode);
      info.GetReturnValue().Set(PointWrapper::FromPoint(result));
    }
  }

  static void TranslateNewPosition(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Patch::ClipMode clip_mode = ClipModeFromJS(info[1]);

    if (start.IsJust()) {
      Point result = patch.TranslateNewPosition(start.FromJust(), clip_mode);
      info.GetReturnValue().Set(PointWrapper::FromPoint(result));
    }
  }

  static void Serialize(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
    auto &serialization_vector = SerializationVector();
    serialization_vector.clear();
    patch.Serialize(&serialization_vector);
    Local<Object> result;
    auto maybe_result = Nan::CopyBuffer(
      reinterpret_cast<char *>(serialization_vector.data()),
      serialization_vector.size()
    );
    if (maybe_result.ToLocal(&result)) {
      info.GetReturnValue().Set(result);
    }
  }

  static void Deserialize(const Nan::FunctionCallbackInfo<Value> &info) {
    Local<Object> result;
    if (Nan::NewInstance(Nan::New(constructor)).ToLocal(&result)) {
      Local<Uint8Array> typed_array = Local<Uint8Array>::Cast(info[0]);
      if (typed_array->IsUint8Array()) {
        auto buffer = typed_array->Buffer();
        auto contents = buffer->GetContents();
        auto &serialization_vector = SerializationVector();
        auto *data = reinterpret_cast<const uint8_t *>(contents.Data());
        serialization_vector.assign(data, data + contents.ByteLength());
        PatchWrapper *wrapper = new PatchWrapper(serialization_vector);
        wrapper->Wrap(result);
        info.GetReturnValue().Set(result);
      }
    }
  }

  static inline std::vector<uint8_t> &SerializationVector() {
    static std::vector<uint8_t> result;
    return result;
  }

  static void PrintDotGraph(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
    patch.PrintDotGraph();
  }

  static Nan::Persistent<v8::Function> constructor;
  Patch patch;
};

Nan::Persistent<v8::Function> PointWrapper::constructor;
Nan::Persistent<v8::Function> HunkWrapper::constructor;
Nan::Persistent<v8::Function> PatchWrapper::constructor;

void Init(Local<Object> exports, Local<Object> module) {
  row_string.Reset(Nan::Persistent<String>(Nan::New("row").ToLocalChecked()));
  column_string.Reset(Nan::Persistent<String>(Nan::New("column").ToLocalChecked()));

  clip_mode::Init();
  PointWrapper::Init();
  HunkWrapper::Init();
  PatchWrapper::Init(exports, module);
}

NODE_MODULE(atom_patch, Init)
