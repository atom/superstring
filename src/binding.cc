#include <cmath>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include "nan.h"
#include "point.h"
#include "hunk.h"
#include "patch.h"
#include "text.h"

using namespace v8;
using std::vector;
using std::unique_ptr;

static Nan::Persistent<String> row_string;
static Nan::Persistent<String> column_string;
static Nan::Persistent<String> new_text_string;
static Nan::Persistent<String> old_text_string;

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

static unique_ptr<Text> TextFromJS(Nan::MaybeLocal<String> maybe_string) {
  Local<String> string;
  if (!maybe_string.ToLocal(&string)) {
    Nan::ThrowTypeError("Expected a string.");
    return nullptr;
  }

  vector<uint16_t> content(string->Length());
  string->Write(content.data(), 0, -1, String::WriteOptions::NO_NULL_TERMINATION);
  return Text::Build(content);
}

static Local<String> TextToJS(Text *text) {
  return Nan::New<String>(text->content.data(), text->content.size()).ToLocalChecked();
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

class PatchWrapper : public Nan::ObjectWrap {
public:
  static void Init(Local<Object> exports, Local<Object> module) {
    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);
    constructor_template->SetClassName(Nan::New<String>("Patch").ToLocalChecked());
    constructor_template->Set(Nan::New("deserialize").ToLocalChecked(), Nan::New<FunctionTemplate>(Deserialize));
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    const auto &prototype_template = constructor_template->PrototypeTemplate();
    prototype_template->Set(Nan::New("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(Splice));
    prototype_template->Set(Nan::New("spliceOld").ToLocalChecked(), Nan::New<FunctionTemplate>(SpliceOld));
    prototype_template->Set(Nan::New("getHunks").ToLocalChecked(), Nan::New<FunctionTemplate>(GetHunks));
    prototype_template->Set(Nan::New("getHunksInOldRange").ToLocalChecked(), Nan::New<FunctionTemplate>(GetHunksInOldRange));
    prototype_template->Set(Nan::New("getHunksInNewRange").ToLocalChecked(), Nan::New<FunctionTemplate>(GetHunksInNewRange));
    prototype_template->Set(Nan::New("hunkForOldPosition").ToLocalChecked(), Nan::New<FunctionTemplate>(HunkForOldPosition));
    prototype_template->Set(Nan::New("hunkForNewPosition").ToLocalChecked(), Nan::New<FunctionTemplate>(HunkForNewPosition));
    prototype_template->Set(Nan::New("serialize").ToLocalChecked(), Nan::New<FunctionTemplate>(Serialize));
    prototype_template->Set(Nan::New("printDotGraph").ToLocalChecked(), Nan::New<FunctionTemplate>(PrintDotGraph));
    prototype_template->Set(Nan::New("rebalance").ToLocalChecked(), Nan::New<FunctionTemplate>(Rebalance));
    prototype_template->Set(Nan::New("getHunkCount").ToLocalChecked(), Nan::New<FunctionTemplate>(GetHunkCount));
    constructor.Reset(constructor_template->GetFunction());
    module->Set(Nan::New("exports").ToLocalChecked(), Nan::New(constructor));
  }

private:
  PatchWrapper(bool merges_adjacent_hunks) : patch{merges_adjacent_hunks} {}

  PatchWrapper(const std::vector<uint8_t> &data) : patch{data} {}

  static void New(const Nan::FunctionCallbackInfo<Value> &info) {
    bool merges_adjacent_hunks = true;
    Local<Object> options;

    if (info.Length() > 0 && Nan::To<Object>(info[0]).ToLocal(&options)) {
      Local<Boolean> js_merge_adjacent_hunks;
      if (Nan::To<Boolean>(options->Get(Nan::New("mergeAdjacentHunks").ToLocalChecked())).ToLocal(&js_merge_adjacent_hunks)) {
        merges_adjacent_hunks = js_merge_adjacent_hunks->BooleanValue();
      }
    }
    PatchWrapper *patch = new PatchWrapper(merges_adjacent_hunks);
    patch->Wrap(info.This());
  }

  static void Splice(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Nan::Maybe<Point> deletion_extent = PointFromJS(Nan::To<Object>(info[1]));
    Nan::Maybe<Point> insertion_extent = PointFromJS(Nan::To<Object>(info[2]));

    if (start.IsJust() && deletion_extent.IsJust() && insertion_extent.IsJust()) {
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

      if (!patch.Splice(start.FromJust(), deletion_extent.FromJust(), insertion_extent.FromJust(), move(deleted_text), move(inserted_text))) {
        Nan::ThrowError("Can't splice into a frozen patch");
      }
    }
  }

  static void SpliceOld(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    Nan::Maybe<Point> deletion_extent = PointFromJS(Nan::To<Object>(info[1]));
    Nan::Maybe<Point> insertion_extent = PointFromJS(Nan::To<Object>(info[2]));

    if (start.IsJust() && deletion_extent.IsJust() && insertion_extent.IsJust()) {
      if (!patch.SpliceOld(start.FromJust(), deletion_extent.FromJust(), insertion_extent.FromJust())) {
        Nan::ThrowError("Can't spliceOld into a frozen patch");
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

  static void HunkForOldPosition(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    if (start.IsJust()) {
      Nan::Maybe<Hunk> result = patch.HunkForOldPosition(start.FromJust());
      if (result.IsJust()) {
        info.GetReturnValue().Set(HunkWrapper::FromHunk(result.FromJust()));
      } else {
        info.GetReturnValue().Set(Nan::Undefined());
      }
    }
  }

  static void HunkForNewPosition(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[0]));
    if (start.IsJust()) {
      Nan::Maybe<Hunk> result = patch.HunkForNewPosition(start.FromJust());
      if (result.IsJust()) {
        info.GetReturnValue().Set(HunkWrapper::FromHunk(result.FromJust()));
      } else {
        info.GetReturnValue().Set(Nan::Undefined());
      }
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

  static void GetHunkCount(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
    uint32_t hunk_count = patch.GetHunkCount();
    info.GetReturnValue().Set(Nan::New<Number>(hunk_count));
  }

  static void Rebalance(const Nan::FunctionCallbackInfo<Value> &info) {
    Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
    patch.Rebalance();
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
  new_text_string.Reset(Nan::Persistent<String>(Nan::New("newText").ToLocalChecked()));
  old_text_string.Reset(Nan::Persistent<String>(Nan::New("oldText").ToLocalChecked()));

  PointWrapper::Init();
  HunkWrapper::Init();
  PatchWrapper::Init(exports, module);
}

NODE_MODULE(atom_patch, Init)
