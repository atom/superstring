#ifndef SUPERSTRING_TEXT_BUFFER_WRAPPER_H
#define SUPERSTRING_TEXT_BUFFER_WRAPPER_H

#include "nan.h"
#include "text-buffer.h"

class TextBufferWrapper : public Nan::ObjectWrap {
public:
  static void init(v8::Local<v8::Object> exports);
  TextBuffer text_buffer;

private:
  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_length(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_extent(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_line_count(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_text(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_text_in_range(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void set_text(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void set_text_in_range(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void line_for_row(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void line_length_for_row(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void line_ending_for_row(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_lines(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void character_index_for_position(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void position_for_character_index(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void find(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void find_sync(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void find_all_sync(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void is_modified(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void load(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void base_text_matches_file(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void save(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void save_to_file_descriptor(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void load_sync(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void save_sync(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void serialize_changes(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void deserialize_changes(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void reset(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void base_text_digest(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void dot_graph(const Nan::FunctionCallbackInfo<v8::Value> &info);
};

#endif // SUPERSTRING_TEXT_BUFFER_WRAPPER_H
