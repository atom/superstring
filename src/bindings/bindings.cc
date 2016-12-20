#include "marker-index-wrapper.h"
#include "buffer-offset-index-wrapper.h"
#include "nan.h"
#include "patch-wrapper.h"
#include "point-wrapper.h"

using namespace v8;

void Init(Local<Object> exports) {
  PointWrapper::Init();
  PatchWrapper::Init(exports);
  MarkerIndexWrapper::Init(exports);
  BufferOffsetIndexWrapper::Init(exports);
}

NODE_MODULE(superstring, Init)
