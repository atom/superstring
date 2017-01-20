#include "serializer.h"

void Serializer::clear() {
  byte_vector.clear();
  read_ptr = byte_vector.data();
  end_ptr = read_ptr;
}

void Serializer::assign(char *begin, char *end) {
  byte_vector.assign(begin, end);
  read_ptr = byte_vector.data();
  end_ptr = read_ptr + byte_vector.size();
}

uint8_t *Serializer::data() {
  return byte_vector.data();
}

size_t Serializer::size() {
  return byte_vector.size();
}
