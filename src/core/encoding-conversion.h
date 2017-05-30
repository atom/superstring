#ifndef SUPERSTRING_ENCODING_CONVERSION_H_
#define SUPERSTRING_ENCODING_CONVERSION_H_

#include "optional.h"
#include "text.h"

class EncodingConversion {
  void *data;
  EncodingConversion(void *);

 public:
  EncodingConversion(EncodingConversion &&);
  EncodingConversion();
  ~EncodingConversion();

  bool encode(const Text::String &, size_t start_offset, size_t end_offset,
              std::ostream &stream, std::vector<char> &buffer);
  size_t encode(const Text::String &, size_t *start_offset, size_t end_offset,
                char *buffer, size_t buffer_size, bool is_last = false);
  bool decode(Text::String &, std::istream &stream, std::vector<char> &buffer,
              std::function<void(size_t)> progress_callback);
  size_t decode(Text::String &, const char *buffer, size_t buffer_size,
                bool is_last = false);

  friend optional<EncodingConversion> transcoding_to(const char *);
  friend optional<EncodingConversion> transcoding_from(const char *);
};

optional<EncodingConversion> transcoding_to(const char *);
optional<EncodingConversion> transcoding_from(const char *);

#endif // SUPERSTRING_ENCODING_CONVERSION_H_