#ifndef SUPERSTRING_OPTIONAL_H
#define SUPERSTRING_OPTIONAL_H

template <typename T> class optional {
  T value;
  bool is_some;

public:
  optional(T value) : value{value}, is_some{true} {}
  optional() : value{T()}, is_some{false} {}


  const T &operator*() const { return value; }
  operator bool() const { return is_some; }
};

#endif // SUPERSTRING_OPTIONAL_H
