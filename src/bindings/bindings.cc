#include "marker-index-wrapper.h"
#include "buffer-offset-index-wrapper.h"
#include "nan.h"
#include "patch-wrapper.h"
#include "range-wrapper.h"
#include "point-wrapper.h"
#include "text-builder.h"
#include "text-reader.h"
#include "text-buffer-wrapper.h"

using namespace v8;

void Init(Local<Object> exports) {
  PointWrapper::init();
  RangeWrapper::init();
  PatchWrapper::init(exports);
  MarkerIndexWrapper::init(exports);
  BufferOffsetIndexWrapper::init(exports);
  TextBufferWrapper::init(exports);
  TextBuilder::init(exports);
  TextReader::init(exports);
}

NODE_MODULE(superstring, Init)
