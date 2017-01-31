#include "text.h"

class TextBuffer {
  Text base_text;
  TextBuffer(Text &&base_text);

 public:
  static TextBuffer build(std::istream &stream, size_t input_size, const char *encoding_name,
                          size_t chunk_size, std::function<void(size_t)> progress_callback);

  Text text() const;
};
