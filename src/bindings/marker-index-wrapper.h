#include "nan.h"
#include "marker-index.h"
#include <optional>
using std::optional;
#include "range.h"

class MarkerIndexWrapper : public Nan::ObjectWrap {
public:
  static void init(v8::Local<v8::Object> exports);
  static MarkerIndex *from_js(v8::Local<v8::Value>);

private:
  static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void generate_random_number(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static bool is_finite(v8::Local<v8::Integer> number);
  static v8::Local<v8::Set> marker_ids_set_to_js(const MarkerIndex::MarkerIdSet &marker_ids);
  static v8::Local<v8::Array> marker_ids_vector_to_js(const std::vector<MarkerIndex::MarkerId> &marker_ids);
  static v8::Local<v8::Object> snapshot_to_js(const std::unordered_map<MarkerIndex::MarkerId, Range> &snapshot);
  static optional<MarkerIndex::MarkerId> marker_id_from_js(v8::Local<v8::Value> value);
  static optional<unsigned> unsigned_from_js(v8::Local<v8::Value> value);
  static optional<bool> bool_from_js(v8::Local<v8::Value> value);
  static void insert(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void set_exclusive(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void remove(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void has(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void splice(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_start(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_end(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void get_range(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void compare(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void find_intersecting(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void find_containing(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void find_contained_in(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void find_starting_in(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void find_starting_at(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void find_ending_in(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void find_ending_at(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void find_boundaries_after(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static void dump(const Nan::FunctionCallbackInfo<v8::Value> &info);
  MarkerIndexWrapper(unsigned seed);
  MarkerIndex marker_index;
};
