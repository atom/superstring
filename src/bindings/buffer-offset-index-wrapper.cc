#include "buffer-offset-index-wrapper.h"
#include "point-wrapper.h"

using namespace v8;

void BufferOffsetIndexWrapper::Init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);
  constructor_template->SetClassName(Nan::New<String>("BufferOffsetIndex").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template->PrototypeTemplate();
  prototype_template->Set(Nan::New<String>("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(Splice));
  prototype_template->Set(Nan::New<String>("positionForCharacterIndex").ToLocalChecked(), Nan::New<FunctionTemplate>(PositionForCharacterIndex));
  prototype_template->Set(Nan::New<String>("characterIndexForPosition").ToLocalChecked(), Nan::New<FunctionTemplate>(CharacterIndexForPosition));
  exports->Set(Nan::New("BufferOffsetIndex").ToLocalChecked(), constructor_template->GetFunction());
}

void BufferOffsetIndexWrapper::New(const Nan::FunctionCallbackInfo<Value> &info) {
  BufferOffsetIndexWrapper *buffer_offset_index = new BufferOffsetIndexWrapper();
  buffer_offset_index->Wrap(info.This());
}

void BufferOffsetIndexWrapper::Splice(const Nan::FunctionCallbackInfo<Value> &info) {
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

void BufferOffsetIndexWrapper::CharacterIndexForPosition(const Nan::FunctionCallbackInfo<Value> &info) {
  BufferOffsetIndex &buffer_offset_index = Nan::ObjectWrap::Unwrap<BufferOffsetIndexWrapper>(info.This())->buffer_offset_index;

  auto position = PointWrapper::PointFromJS(info[0]);
  if (position) {
    auto result = buffer_offset_index.CharacterIndexForPosition(*position);
    info.GetReturnValue().Set(Nan::New(result));
  }
}

void BufferOffsetIndexWrapper::PositionForCharacterIndex(const Nan::FunctionCallbackInfo<Value> &info) {
  BufferOffsetIndex &buffer_offset_index = Nan::ObjectWrap::Unwrap<BufferOffsetIndexWrapper>(info.This())->buffer_offset_index;

  auto character_index = Nan::To<unsigned>(info[0]);
  if (character_index.IsJust()) {
    auto result = buffer_offset_index.PositionForCharacterIndex(character_index.FromJust());
    info.GetReturnValue().Set(PointWrapper::FromPoint(result));
  }
}
