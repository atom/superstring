#include "buffer-offset-index-wrapper.h"
#include "point-wrapper.h"

using namespace v8;

void BufferOffsetIndexWrapper::init(Local<Object> exports) {
  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
  constructor_template->SetClassName(Nan::New<String>("BufferOffsetIndex").ToLocalChecked());
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  const auto &prototype_template = constructor_template->PrototypeTemplate();
  prototype_template->Set(Nan::New<String>("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(splice));
  prototype_template->Set(Nan::New<String>("positionForCharacterIndex").ToLocalChecked(), Nan::New<FunctionTemplate>(position_for_character_index));
  prototype_template->Set(Nan::New<String>("characterIndexForPosition").ToLocalChecked(), Nan::New<FunctionTemplate>(character_index_for_position));
  exports->Set(Nan::New("BufferOffsetIndex").ToLocalChecked(), constructor_template->GetFunction());
}

void BufferOffsetIndexWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {
  BufferOffsetIndexWrapper *buffer_offset_index = new BufferOffsetIndexWrapper();
  buffer_offset_index->Wrap(info.This());
}

void BufferOffsetIndexWrapper::splice(const Nan::FunctionCallbackInfo<Value> &info) {
  BufferOffsetIndex &buffer_offset_index = Nan::ObjectWrap::Unwrap<BufferOffsetIndexWrapper>(info.This())->buffer_offset_index;

  auto start_row = info[0].As<Uint32>()->Value();
  auto deleted_lines_count = info[1].As<Uint32>()->Value();
  auto js_new_line_lengths = info[2].As<Array>();
  auto new_line_lengths = std::vector<unsigned>(js_new_line_lengths->Length());
  for (size_t i = 0; i < new_line_lengths.size(); i++) {
    new_line_lengths[i] = js_new_line_lengths->Get(i).As<Uint32>()->Value();
  }
  buffer_offset_index.splice(start_row, deleted_lines_count, new_line_lengths);
}

void BufferOffsetIndexWrapper::character_index_for_position(const Nan::FunctionCallbackInfo<Value> &info) {
  BufferOffsetIndex &buffer_offset_index = Nan::ObjectWrap::Unwrap<BufferOffsetIndexWrapper>(info.This())->buffer_offset_index;

  auto position = PointWrapper::point_from_js(info[0]);
  if (position) {
    auto result = buffer_offset_index.character_index_for_position(*position);
    info.GetReturnValue().Set(Nan::New(result));
  }
}

void BufferOffsetIndexWrapper::position_for_character_index(const Nan::FunctionCallbackInfo<Value> &info) {
  BufferOffsetIndex &buffer_offset_index = Nan::ObjectWrap::Unwrap<BufferOffsetIndexWrapper>(info.This())->buffer_offset_index;

  auto character_index = Nan::To<unsigned>(info[0]);
  if (character_index.IsJust()) {
    auto result = buffer_offset_index.position_for_character_index(character_index.FromJust());
    info.GetReturnValue().Set(PointWrapper::from_point(result));
  }
}
