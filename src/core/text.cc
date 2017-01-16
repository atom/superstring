#include "text.h"
#include <limits.h>
#include <vector>
#include <memory>
#include <iconv.h>

using std::move;
using std::vector;
using std::unique_ptr;
using std::basic_ostream;
using std::istream;

bool Line::operator==(const Line &other) const {
  return content == other.content && ending == other.ending;
}

Text::Text() : lines {Line{{}, LineEnding::NONE}} {}

Text::Text(const vector<Line> &lines) : lines{lines} {}

void Text::append(TextSlice slice) {
  Line &last_line = lines.back();

  if (slice.start.row == slice.end.row) {
    last_line.content.insert(
      last_line.content.end(),
      slice.text->lines[slice.start.row].content.begin() + slice.start.column,
      slice.text->lines[slice.end.row].content.begin() + slice.end.column
    );
  } else {
    last_line.content.insert(
      last_line.content.end(),
      slice.text->lines[slice.start.row].content.begin() + slice.start.column,
      slice.text->lines[slice.start.row].content.end()
    );
    last_line.ending = slice.text->lines[slice.start.row].ending;
    lines.insert(
      lines.end(),
      slice.text->lines.begin() + slice.start.row + 1,
      slice.text->lines.begin() + slice.end.row
    );
    lines.push_back({
      slice.text->lines[slice.end.row].content.substr(0, slice.end.column),
      LineEnding::NONE
    });
  }
}

void Text::write(vector<uint16_t> &vector) const {
  for (const Line &line : lines) {
    for (char16_t character : line.content) {
      vector.push_back(character);
    }
    switch (line.ending) {
      case LineEnding::NONE:
        break;
      case LineEnding::LF:
        vector.push_back('\n');
        break;
      case LineEnding::CR:
        vector.push_back('\r');
        break;
      case LineEnding::CRLF:
        vector.push_back('\r');
        vector.push_back('\n');
        break;
    }
  }
}

Point Text::Extent() const {
  return Point(lines.size() - 1, lines.back().content.size());
}

bool Text::operator==(const Text &other) const {
  return lines == other.lines;
}

TextSlice::TextSlice(Text &text) : text{&text}, start{Point()}, end{text.Extent()} {}

TextSlice::TextSlice(Text *text, Point start, Point end) : text{text}, start{start}, end{end} {}

Text TextSlice::concat(TextSlice a, TextSlice b) {
  Text result;
  result.append(a);
  result.append(b);
  return result;
}

Text TextSlice::concat(TextSlice a, TextSlice b, TextSlice c) {
  Text result;
  result.append(a);
  result.append(b);
  result.append(c);
  return result;
}

std::pair<TextSlice, TextSlice> TextSlice::split(Point position) {
  Point split_point = start.traverse(position);
  return {
    TextSlice{text, start, split_point},
    TextSlice{text, Point::min(split_point, end), end}
  };
}

TextSlice TextSlice::suffix(Point suffix_start) {
  return split(suffix_start).second;
}

TextSlice TextSlice::prefix(Point prefix_end) {
  return split(prefix_end).first;
}

std::ostream &operator<<(std::ostream &stream, const Text &text) {
  for (Line line : text.lines) {
    for (char16_t character : line.content) {
      if (character < CHAR_MAX) {
        stream << static_cast<char>(character);
      } else {
        stream << "\\u" << character;
      }
    }

    switch (line.ending) {
      case LineEnding::NONE:
        break;
      case LineEnding::LF:
        stream << '\n';
        break;
      case LineEnding::CR:
        stream << '\r';
        break;
      case LineEnding::CRLF:
        stream << "\r\n";
        break;
    }
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const TextSlice &slice) {
  for (uint32_t row = slice.start.row; row <= slice.end.row; row++) {
    Line &line = slice.text->lines[row];

    uint32_t column = 0;
    if (row == slice.start.row) column = slice.start.column;

    uint32_t end_column = line.content.size();
    if (row == slice.end.row) end_column = slice.end.column;

    for (; column < end_column; column++) {
      char16_t character = line.content[column];
      if (character < CHAR_MAX) {
        stream << static_cast<char>(character);
      } else {
        stream << "\\u" << character;
      }
    }

    if (row != slice.end.row) {
      switch (line.ending) {
        case LineEnding::NONE:
          break;
        case LineEnding::LF:
          stream << '\n';
          break;
        case LineEnding::CR:
          stream << '\r';
          break;
        case LineEnding::CRLF:
          stream << "\r\n";
          break;
      }
    }
  }

  return stream;
}
