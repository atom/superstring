#ifndef TEXT_H_
#define TEXT_H_

#include <memory>
#include <vector>
#include <ostream>
#include "point.h"
#include "serializer.h"

enum class LineEnding : uint8_t {
  NONE,
  LF,
  CR,
  CRLF
};

struct Line {
  Line() : ending{LineEnding::NONE} {}
  Line(const std::u16string &content, LineEnding ending) :
    content{content}, ending{ending} {}

  bool operator==(const Line &other) const;
  std::u16string content;
  LineEnding ending;
};

struct TextSlice;

struct Text {
  std::vector<Line> lines;

  Text();
  Text(const std::vector<Line> &);
  Text(Serializer &input);

  bool operator==(const Text &other) const;
  Point Extent() const;
  void append(TextSlice slice);
  void write(std::vector<uint16_t> &) const;
  void serialize(Serializer &output) const;
};

struct TextSlice {
  Text *text;
  Point start;
  Point end;

  static Text concat(TextSlice a, TextSlice b);
  static Text concat(TextSlice a, TextSlice b, TextSlice c);

  TextSlice(Text &text);
  TextSlice(Text *text, Point start, Point end);

  std::pair<TextSlice, TextSlice> split(Point);
  TextSlice prefix(Point);
  TextSlice suffix(Point);
};

std::ostream &operator<<(std::ostream &stream, const Text &text);
std::ostream &operator<<(std::ostream &stream, const TextSlice &text);

#endif  // TEXT_H_
