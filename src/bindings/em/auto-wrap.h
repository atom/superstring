#pragma once

#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <emscripten/val.h>

#include "as.h"
#include "flat_set.h"
#include "marker-index.h"
#include "optional.h"
#include "point-wrapper.h"
#include "point.h"
#include "text.h"

/********** **********/

template <typename LocalTypeTpl, typename WireTypeTpl>
struct em_wrap_type_base {
  using LocalType = LocalTypeTpl;
  using WireType = WireTypeTpl;
};

template <typename LocalType, typename WireType>
struct em_wrap_type_simple : public em_wrap_type_base<LocalType, WireType> {
  static LocalType receive(WireType data) { return data; }
  static WireType transmit(LocalType data) { return data; }
};

/********** **********/

template <typename LocalType>
struct em_wrap_type : public em_wrap_type_simple<LocalType, LocalType> {};

/********** **********/

template <typename LocalType>
LocalType em_receive(typename em_wrap_type<typename std::decay<LocalType>::type>::WireType data) {
  return em_wrap_type<typename std::decay<LocalType>::type>::receive(data);
}

template <typename LocalType>
typename em_wrap_type<typename std::decay<LocalType>::type>::WireType em_transmit(LocalType data) {
  return em_wrap_type<typename std::decay<LocalType>::type>::transmit(data);
}

/********** **********/

template <>
struct em_wrap_type<void> : public em_wrap_type_base<void, void> {};

template <>
struct em_wrap_type<Point> : public em_wrap_type_simple<Point, PointWrapper> {};

template <typename ValueType>
struct em_wrap_type<std::vector<ValueType>> : public em_wrap_type_base<std::vector<ValueType>, emscripten::val> {
  static std::vector<ValueType> receive(emscripten::val const &value) {
    std::vector<ValueType> result;
    for (auto i = 0u, length = value["length"].as<unsigned>(); i < length; ++i) {
      result.push_back(value[i].as<ValueType>());
    }
    return result;
  }

  static emscripten::val transmit(std::vector<ValueType> const &input) {
    auto result = emscripten::val::array();
    for (auto const &element : input) {
      result.call<void>("push", em_transmit(element));
    }
    return result;
  }
};

template <typename ValueType>
struct em_wrap_type<optional<ValueType>> : public em_wrap_type_base<optional<ValueType>, emscripten::val> {
  static optional<ValueType> receive(emscripten::val const &value) {
    if (!value.as<bool>()) {
      return optional<ValueType>();
    }
    return optional<ValueType>(value.as<ValueType>());
  }

  static emscripten::val transmit(optional<ValueType> const &input) {
    if (!input) {
      return emscripten::val::undefined();
    }
    return emscripten::val(em_transmit(*input));
  }
};

template <typename KeyType, typename ValueType>
struct em_wrap_type<std::unordered_map<KeyType, ValueType>> : public em_wrap_type_base<std::unordered_map<KeyType, ValueType>, emscripten::val> {
  static std::unordered_map<KeyType, ValueType> receive(emscripten::val const &value) {
    throw std::runtime_error("Unimplemented");
  }

  static emscripten::val transmit(std::unordered_map<KeyType, ValueType> const &input) {
    auto object = emscripten::val::object();
    for (auto const &pair : input) {
      object.set(em_transmit(pair.first), em_transmit(pair.second));
    }
    return object;
  }
};

template <typename ValueType>
struct em_wrap_type<flat_set<ValueType>> : public em_wrap_type_base<flat_set<ValueType>, emscripten::val> {
  static flat_set<ValueType> receive(emscripten::val const & val) {
    throw std::runtime_error("Unimplemented");
  }

  static emscripten::val transmit(flat_set<ValueType> const & set) {
    auto object = emscripten::val::global("Set").new_();
    for (auto const &element : set) {
      object.call<void>("add", em_transmit(element));
    }
    return object;
  }
};

template <>
struct em_wrap_type<MarkerIndex::SpliceResult> : public em_wrap_type_base<MarkerIndex::SpliceResult, emscripten::val> {
  static MarkerIndex::SpliceResult receive(emscripten::val const &value) {
    throw std::runtime_error("Unimplemented");
  }

  static emscripten::val transmit(MarkerIndex::SpliceResult const &splice_result) {
    auto result = emscripten::val::object();
    result.set("touch", em_transmit(splice_result.touch));
    result.set("inside", em_transmit(splice_result.inside));
    result.set("overlap", em_transmit(splice_result.overlap));
    result.set("surround", em_transmit(splice_result.surround));
    return result;
  }
};

template <>
struct em_wrap_type<Text> : public em_wrap_type_base<Text, std::string> {
  static Text receive(std::string const &str) {
    return Text(str.begin(), str.end());
  }

  static std::string transmit(Text const &text) {
    return std::string(text.begin(), text.end());
  }
};

template <>
struct em_wrap_type<std::unique_ptr<Text>> : public em_wrap_type_base<std::unique_ptr<Text>, emscripten::val> {
  static std::unique_ptr<Text> receive(emscripten::val const &value) {
    return std::make_unique<Text>(em_wrap_type<Text>::receive(value.as<std::string>()));
  }

  static emscripten::val transmit(std::unique_ptr<Text> const &text) {
    if (!text) return emscripten::val::undefined();
    return emscripten::val(em_wrap_type<Text>::transmit(*text));
  }
};

template <>
struct em_wrap_type<Text *> : public em_wrap_type_base<Text *, emscripten::val> {
  static Text * receive(emscripten::val const &value) {
    return new Text(em_wrap_type<Text>::receive(value.as<std::string>()));
  }

  static emscripten::val transmit(Text *text) {
    if (!text) return emscripten::val::undefined();
    return emscripten::val(em_wrap_type<Text>::transmit(*text));
  }
};

/********** **********/

template <typename T, T>
struct em_wrap_fn;

template <typename T, typename ... Args, void (T::*fn)(Args ...)>
struct em_wrap_fn<void (T::*)(Args ...), fn> {
  static void wrap(T & t, typename em_wrap_type<typename std::decay<Args>::type>::WireType ... args) {
    return (t.*fn)(em_wrap_type<typename std::decay<Args>::type>::receive(args) ...);
  }
};

template <typename T, typename ... Args, void (T::*fn)(Args ...) const>
struct em_wrap_fn<void (T::*)(Args ...) const, fn> {
  static void wrap(T const & t, typename em_wrap_type<typename std::decay<Args>::type>::WireType ... args) {
    return (t.*fn)(em_wrap_type<typename std::decay<Args>::type>::receive(args) ...);
  }
};

template <typename T, typename Ret, typename ... Args, Ret (T::*fn)(Args ...)>
struct em_wrap_fn<Ret (T::*)(Args ...), fn> {
  static typename em_wrap_type<typename std::decay<Ret>::type>::WireType wrap(T & t, typename em_wrap_type<typename std::decay<Args>::type>::WireType ... args) {
    return em_wrap_type<typename std::decay<Ret>::type>::transmit((t.*fn)(em_wrap_type<typename std::decay<Args>::type>::receive(args) ...));
  }
};

template <typename T, typename Ret, typename ... Args, Ret (T::*fn)(Args ...) const>
struct em_wrap_fn<Ret (T::*)(Args ...) const, fn> {
  static typename em_wrap_type<typename std::decay<Ret>::type>::WireType wrap(T const & t, typename em_wrap_type<typename std::decay<Args>::type>::WireType ... args) {
    return em_wrap_type<typename std::decay<Ret>::type>::transmit((t.*fn)(em_wrap_type<typename std::decay<Args>::type>::receive(args) ...));
  }
};

template <typename T, typename ... Args, void (*fn)(T &, Args ...)>
struct em_wrap_fn<void (*)(T &, Args ...), fn> {
  static void wrap(T & t, typename em_wrap_type<typename std::decay<Args>::type>::WireType ... args) {
    return fn(t, em_wrap_type<typename std::decay<Args>::type>::receive(args) ...);
  }
};

template <typename T, typename ... Args, void (*fn)(T const &, Args ...)>
struct em_wrap_fn<void (*)(T const &, Args ...), fn> {
  static void wrap(T const & t, typename em_wrap_type<typename std::decay<Args>::type>::WireType ... args) {
    return fn(t, em_wrap_type<typename std::decay<Args>::type>::receive(args) ...);
  }
};

template <typename T, typename Ret, typename ... Args, Ret (*fn)(T &, Args ...)>
struct em_wrap_fn<Ret (*)(T &, Args ...), fn> {
  static typename em_wrap_type<typename std::decay<Ret>::type>::WireType wrap(T & t, typename em_wrap_type<typename std::decay<Args>::type>::WireType ... args) {
    return em_wrap_type<typename std::decay<Ret>::type>::transmit(fn(t, em_wrap_type<typename std::decay<Args>::type>::receive(args) ...));
  }
};

template <typename T, typename Ret, typename ... Args, Ret (*fn)(T const &, Args ...)>
struct em_wrap_fn<Ret (*)(T const &, Args ...), fn> {
  static typename em_wrap_type<typename std::decay<Ret>::type>::WireType wrap(T const & t, typename em_wrap_type<typename std::decay<Args>::type>::WireType ... args) {
    return em_wrap_type<typename std::decay<Ret>::type>::transmit(fn(t, em_wrap_type<typename std::decay<Args>::type>::receive(args) ...));
  }
};

/********** **********/

template <typename T, T>
struct em_wrap_static_fn;

template <typename ... Args, void (*fn)(Args ...)>
struct em_wrap_static_fn<void (*)(Args ...), fn> {
  static void wrap(typename em_wrap_type<typename std::decay<Args>::type>::WireType ... args) {
    return fn(em_wrap_type<typename std::decay<Args>::type>::receive(args) ...);
  }
};

template <typename Ret, typename ... Args, Ret (*fn)(Args ...)>
struct em_wrap_static_fn<Ret (*)(Args ...), fn> {
  static typename em_wrap_type<typename std::decay<Ret>::type>::WireType wrap(typename em_wrap_type<typename std::decay<Args>::type>::WireType ... args) {
    return em_wrap_type<typename std::decay<Ret>::type>::transmit(fn(em_wrap_type<typename std::decay<Args>::type>::receive(args) ...));
  }
};

/********** **********/

template <typename T, T>
struct em_wrap_property;

template <typename T, typename PropertyType, PropertyType T::*property>
struct em_wrap_property<PropertyType T::*, property> {
  static typename em_wrap_type<typename std::decay<PropertyType>::type>::WireType get(T const & t) {
    return em_transmit(t.*property);
  }

  static void set(T & t, typename em_wrap_type<typename std::decay<PropertyType>::type>::WireType wire) {
    t.*property = em_receive<PropertyType>(wire);
  }
};

/********** **********/

#define WRAP(FN) WRAP_OVERLOAD((FN), decltype(FN))
#define WRAP_OVERLOAD(FN, ...) &em_wrap_fn<__VA_ARGS__, (FN)>::wrap

#define WRAP_STATIC(FN) WRAP_STATIC_OVERLOAD((FN), decltype(FN))
#define WRAP_STATIC_OVERLOAD(FN, ...) &em_wrap_static_fn<__VA_ARGS__, (FN)>::wrap

#define WRAP_FIELD(CLASS, FIELD) &em_wrap_property<decltype(&CLASS::FIELD), &CLASS::FIELD>::get, &em_wrap_property<decltype(&CLASS::FIELD), &CLASS::FIELD>::set
