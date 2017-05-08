#ifndef SUPERSTRING_TEXT_READER_H
#define SUPERSTRING_TEXT_READER_H

#include "nan.h"
#include "text.h"
#include "text-buffer.h"

class TextReader : public Nan::ObjectWrap {
public:
  static void init(v8::Local<v8::Object> exports);

private:
  TextReader(TextBuffer::Snapshot *snapshot, Text::EncodingConversion conversion);
  ~TextReader();

  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void read(const Nan::FunctionCallbackInfo<v8::Value> &info);

  TextBuffer::Snapshot *snapshot;
  std::vector<TextSlice> slices;
  size_t slice_index;
  size_t slice_offset;
  Text::EncodingConversion conversion;
};

#endif // SUPERSTRING_TEXT_READER_H
