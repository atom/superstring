#include <math.h>
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
  if (isfinite(js_row->NumberValue())) {
    row = static_cast<unsigned>(js_row->Int32Value());
  } else {
    row = UINT_MAX;
  }

  if (isfinite(js_column->NumberValue())) {
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
  PointWrapper(Point point) : point{point} {}

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

Nan::Persistent<v8::Function> PointWrapper::constructor;

class HunkWrapper : public Nan::ObjectWrap {
public:
  static void Init() {
    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);
    constructor_template->SetClassName(Nan::New<String>("Hunk").ToLocalChecked());
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    const auto &instance_template = constructor_template->InstanceTemplate();
    Nan::SetAccessor(instance_template, Nan::New("old_start").ToLocalChecked(), GetOldStart);
    Nan::SetAccessor(instance_template, Nan::New("new_start").ToLocalChecked(), GetNewStart);
    Nan::SetAccessor(instance_template, Nan::New("old_end").ToLocalChecked(), GetOldEnd);
    Nan::SetAccessor(instance_template, Nan::New("new_end").ToLocalChecked(), GetNewEnd);
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
  HunkWrapper(Hunk hunk) : hunk{hunk} {}

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

  static Nan::Persistent<v8::Function> constructor;
  Hunk hunk;
};

Nan::Persistent<v8::Function> HunkWrapper::constructor;

class PatchWrapper : public Nan::ObjectWrap {
public:
  static void Init(Local<Object> exports, Local<Object> module) {
    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);
    constructor_template->SetClassName(Nan::New<String>("Patch").ToLocalChecked());
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    const auto &prototype_template = constructor_template->PrototypeTemplate();
    prototype_template->Set(Nan::New<String>("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(Splice));
    prototype_template->Set(Nan::New<String>("getHunks").ToLocalChecked(), Nan::New<FunctionTemplate>(GetHunks));
    module->Set(Nan::New("exports").ToLocalChecked(), constructor_template->GetFunction());
  }

private:
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
      patch.Splice(start.FromJust(), deletion_extent.FromJust(), insertion_extent.FromJust());
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

  Patch patch;
};

void Init(Local<Object> exports, Local<Object> module) {
  row_string.Reset(Nan::Persistent<String>(Nan::New("row").ToLocalChecked()));
  column_string.Reset(Nan::Persistent<String>(Nan::New("column").ToLocalChecked()));

  PointWrapper::Init();
  HunkWrapper::Init();
  PatchWrapper::Init(exports, module);
}

NODE_MODULE(atom_patch, Init)
