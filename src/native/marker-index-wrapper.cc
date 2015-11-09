#include "nan.h"
#include "marker-index.h"

using namespace std;
using namespace v8;

class MarkerIndexWrapper : public Nan::ObjectWrap {
public:
  static void Init(Local<Object> exports, Local<Object> module) {
    Local<FunctionTemplate> constructorTemplate =
        Nan::New<FunctionTemplate>(New);
    constructorTemplate->SetClassName(
        Nan::New<String>("MarkerIndex").ToLocalChecked());
    constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("generateRandomNumber").ToLocalChecked(), Nan::New<FunctionTemplate>(GenerateRandomNumber)->GetFunction());
    constructorTemplate->PrototypeTemplate()->Set(Nan::New<String>("insert").ToLocalChecked(), Nan::New<FunctionTemplate>(Insert)->GetFunction());

    row_key.Reset(Nan::Persistent<String>(Nan::New("row").ToLocalChecked()));
    column_key.Reset(Nan::Persistent<String>(Nan::New("column").ToLocalChecked()));

    module->Set(Nan::New("exports").ToLocalChecked(),
                constructorTemplate->GetFunction());
  }

private:
  static Nan::Persistent<String> row_key;
  static Nan::Persistent<String> column_key;

  static void New(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *marker_index = new MarkerIndexWrapper(Local<Number>::Cast(info[0]));
    marker_index->Wrap(info.This());
  }

  static void GenerateRandomNumber(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
    int random = wrapper->marker_index.GenerateRandomNumber();
    info.GetReturnValue().Set(Nan::New<v8::Number>(random));
  }

  static Nan::Maybe<Point> PointFromJS(Nan::MaybeLocal<Object> maybe_object) {
    Local<Object> object;
    if (!maybe_object.ToLocal(&object)) {
      Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
      return Nan::Nothing<Point>();
    }

    Nan::MaybeLocal<Integer> maybe_row = Nan::To<Integer>(object->Get(Nan::New(row_key)));
    Local<Integer> row;
    if (!maybe_row.ToLocal(&row)) {
      Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
      return Nan::Nothing<Point>();
    }

    Nan::MaybeLocal<Integer> maybe_column = Nan::To<Integer>(object->Get(Nan::New(column_key)));
    Local<Integer> column;
    if (!maybe_column.ToLocal(&column)) {
      Nan::ThrowTypeError("Expected an object with 'row' and 'column' properties.");
      return Nan::Nothing<Point>();
    }

    return Nan::Just(Point(
      static_cast<unsigned>(row->Int32Value()),
      static_cast<unsigned>(column->Int32Value())
    ));
  }

  static void Insert(const Nan::FunctionCallbackInfo<Value> &info) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    Local<Integer> id;
    Nan::MaybeLocal<Integer> maybe_id = Nan::To<Integer>(info[0]);
    if (!maybe_id.ToLocal(&id)) {
      Nan::ThrowTypeError("Expected an integer marker id.");
      return;
    }

    Nan::Maybe<Point> start = PointFromJS(Nan::To<Object>(info[1]));
    Nan::Maybe<Point> end = PointFromJS(Nan::To<Object>(info[2]));

    if (start.IsJust() && end.IsJust()) {
      wrapper->marker_index.Insert(static_cast<unsigned>(id->Uint32Value()), start.FromJust(), end.FromJust());
    }
  }

  MarkerIndexWrapper(v8::Local<v8::Number> seed) :
    marker_index{static_cast<unsigned>(seed->Int32Value())} {}

  MarkerIndex marker_index;
};

Nan::Persistent<String> MarkerIndexWrapper::row_key;
Nan::Persistent<String> MarkerIndexWrapper::column_key;

NODE_MODULE(marker_index, MarkerIndexWrapper::Init)
