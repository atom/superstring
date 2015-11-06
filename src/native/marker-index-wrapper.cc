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
    module->Set(Nan::New("exports").ToLocalChecked(),
                constructorTemplate->GetFunction());
  }

private:
  static NAN_METHOD(New) {
    MarkerIndexWrapper *marker_index = new MarkerIndexWrapper(Local<Number>::Cast(info[0]));
    marker_index->Wrap(info.This());
  }

  static NAN_METHOD(GenerateRandomNumber) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
    int random = wrapper->marker_index.GenerateRandomNumber();
    info.GetReturnValue().Set(Nan::New<v8::Number>(random));
  }

  static NAN_METHOD(Insert) {
    MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());

    String::Utf8Value id_utf_8_value {info[0]};
    string id {*id_utf_8_value};

    Local<Number> start_row = Local<Number>::Cast(info[1]);
    Local<Number> start_column = Local<Number>::Cast(info[2]);
    Local<Number> end_row = Local<Number>::Cast(info[3]);
    Local<Number> end_column = Local<Number>::Cast(info[4]);
    Point start_point {static_cast<unsigned>(start_row->Int32Value()), static_cast<unsigned>(start_column->Int32Value())};
    Point end_point {static_cast<unsigned>(end_row->Int32Value()), static_cast<unsigned>(end_column->Int32Value())};

    wrapper->marker_index.Insert(id, start_point, end_point);
  }

  MarkerIndexWrapper(v8::Local<v8::Number> seed) :
    marker_index{static_cast<unsigned>(seed->Int32Value())} {}

  MarkerIndex marker_index;
};

NODE_MODULE(marker_index, MarkerIndexWrapper::Init)
