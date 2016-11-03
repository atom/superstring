#include "nan.h"
#include "buffer_offset_index.h"
#include "point.h"

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

  return Nan::Just(Point {row, column});
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

class BufferOffsetIndexWrapper : public Nan::ObjectWrap {
public:
  static void Init(Local<Object> exports, Local<Object> module) {
    Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);
    constructor_template->SetClassName(Nan::New<String>("BufferOffsetIndex").ToLocalChecked());
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    const auto &prototype_template = constructor_template->PrototypeTemplate();
    prototype_template->Set(Nan::New<String>("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(Splice));
    prototype_template->Set(Nan::New<String>("positionForCharacterIndex").ToLocalChecked(), Nan::New<FunctionTemplate>(PositionForCharacterIndex));
    prototype_template->Set(Nan::New<String>("characterIndexForPosition").ToLocalChecked(), Nan::New<FunctionTemplate>(CharacterIndexForPosition));
    module->Set(Nan::New("exports").ToLocalChecked(), constructor_template->GetFunction());
  }

private:
  static void New(const Nan::FunctionCallbackInfo<Value> &info) {
    BufferOffsetIndexWrapper *buffer_offset_index = new BufferOffsetIndexWrapper();
    buffer_offset_index->Wrap(info.This());
  }

  static void Splice(const Nan::FunctionCallbackInfo<Value> &info) {
    BufferOffsetIndex &buffer_offset_index = Nan::ObjectWrap::Unwrap<BufferOffsetIndexWrapper>(info.This())->buffer_offset_index;

    auto start_row = info[0].As<Uint32>()->Value();
    auto deleted_lines_count = info[1].As<Uint32>()->Value();
    auto js_new_line_lengths = info[2].As<Array>();
    auto new_line_lengths = std::vector<unsigned>(js_new_line_lengths->Length());
    for (size_t i = 0; i < new_line_lengths.size(); i++) {
      new_line_lengths[i] = js_new_line_lengths->Get(i).As<Uint32>()->Value();
    }
    buffer_offset_index.Splice(start_row, deleted_lines_count, new_line_lengths);
  }

  static void CharacterIndexForPosition(const Nan::FunctionCallbackInfo<Value> &info) {
    BufferOffsetIndex &buffer_offset_index = Nan::ObjectWrap::Unwrap<BufferOffsetIndexWrapper>(info.This())->buffer_offset_index;

    auto position = PointFromJS(Nan::To<Object>(info[0]));
    if (position.IsJust()) {
      auto result = buffer_offset_index.CharacterIndexForPosition(position.FromJust());
      info.GetReturnValue().Set(Nan::New(result));
    }
  }

  static void PositionForCharacterIndex(const Nan::FunctionCallbackInfo<Value> &info) {
    BufferOffsetIndex &buffer_offset_index = Nan::ObjectWrap::Unwrap<BufferOffsetIndexWrapper>(info.This())->buffer_offset_index;

    auto character_index = Nan::To<unsigned>(info[0]);
    if (character_index.IsJust()) {
      auto result = buffer_offset_index.PositionForCharacterIndex(character_index.FromJust());
      info.GetReturnValue().Set(PointWrapper::FromPoint(result));
    }
  }

  BufferOffsetIndex buffer_offset_index;
};

void Init(Local<Object> exports, Local<Object> module) {
  row_string.Reset(Nan::Persistent<String>(Nan::New("row").ToLocalChecked()));
  column_string.Reset(Nan::Persistent<String>(Nan::New("column").ToLocalChecked()));

  PointWrapper::Init();
  BufferOffsetIndexWrapper::Init(exports, module);
}

NODE_MODULE(atom_buffer_offset_index, Init)
