#include "nan.h"
#include <random>

using namespace std;
using namespace v8;

class MarkerIndex : public Nan::ObjectWrap {
public:
  static void Init(Local<Object> exports, Local<Object> module) {
    Local<FunctionTemplate> constructorTemplate =
        Nan::New<FunctionTemplate>(New);
    constructorTemplate->SetClassName(
        Nan::New<String>("MarkerIndex").ToLocalChecked());
    constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
    module->Set(Nan::New("exports").ToLocalChecked(),
                constructorTemplate->GetFunction());
  }

private:
  static NAN_METHOD(New) {
    MarkerIndex *scanner = new MarkerIndex(Local<Number>::Cast(info[0]));
    scanner->Wrap(info.This());
  }

  MarkerIndex(v8::Local<v8::Number> seed) :
    random_engine{static_cast<default_random_engine::result_type>(
      seed->Int32Value())} {}

  std::default_random_engine random_engine;
};

NODE_MODULE(marker_index, MarkerIndex::Init)
