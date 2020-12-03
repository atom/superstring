#ifndef SUPERSTRING_ENCODING_CONVERSION_H_
#define SUPERSTRING_ENCODING_CONVERSION_H_

#include <optional>
using std::optional;
#include "text.h"
#include <cstdio>

class EncodingConversion {
  void *data;
  int mode;

  EncodingConversion(int, void *);
  int convert(const char **, const char *, char **, char *) const;

 public:
  EncodingConversion(EncodingConversion &&);
  EncodingConversion();
  ~EncodingConversion();

  bool encode(const std::u16string &, size_t start_offset, size_t end_offset,
              FILE *stream, std::vector<char> &buffer);
  size_t encode(const std::u16string &, size_t *start_offset, size_t end_offset,
                char *buffer, size_t buffer_size, bool is_last = false);
  bool decode(std::u16string &, FILE *stream, std::vector<char> &buffer,
              std::function<void(size_t)> progress_callback);
  size_t decode(std::u16string &, const char *buffer, size_t buffer_size,
                bool is_last = false);

  friend optional<EncodingConversion> transcoding_to(const char *);
  friend optional<EncodingConversion> transcoding_from(const char *);
};

optional<EncodingConversion> transcoding_to(const char *);
optional<EncodingConversion> transcoding_from(const char *);

#endif // SUPERSTRING_ENCODING_CONVERSION_H_
