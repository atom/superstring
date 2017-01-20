#ifndef SERIALIZER_H_
#define SERIALIZER_H_

#include <vector>

class Serializer {
  std::vector<uint8_t> byte_vector;
  uint8_t *read_ptr;
  uint8_t *end_ptr;

 public:
  void clear();
  void assign(char *begin, char *end);

  template <typename T>
  void append(T value) {
    for (auto i = 0u; i < sizeof(T); i++) {
      byte_vector.push_back(value & 0xFF);
      value >>= 8;
    }
    read_ptr = byte_vector.data();
    end_ptr = read_ptr + byte_vector.size();
  }

  template <typename T>
  T read() {
    T value = 0;
    if (static_cast<unsigned>(end_ptr - read_ptr) >= sizeof(T)) {
      for (auto i = 0u; i < sizeof(T); i++) {
        value |= static_cast<T>(*(read_ptr++)) << static_cast<T>(8 * i);
      }
    }
    return value;
  }

  uint8_t *data();
  size_t size();
};

#endif // SERIALIZER_H_
