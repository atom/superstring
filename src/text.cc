#include "text.h"
#include <vector>
#include <memory>

using std::move;
using std::vector;
using std::unique_ptr;

unique_ptr<Text> Text::Build(vector<uint16_t> &content) {
  return unique_ptr<Text>(new Text{move(content)});
}

unique_ptr<Text> Text::Concat(TextSlice a, TextSlice b) {
  vector<uint16_t> content;
  content.reserve(
    a.end_index - a.start_index +
    b.end_index - b.start_index
  );
  auto result = Build(content);
  result->Append(a);
  result->Append(b);
  return result;
}

unique_ptr<Text> Text::Concat(TextSlice a, TextSlice b, TextSlice c) {
  vector<uint16_t> content;
  content.reserve(
    a.end_index - a.start_index +
    b.end_index - b.start_index +
    c.end_index - c.start_index
  );
  auto result = Build(content);
  result->Append(a);
  result->Append(b);
  result->Append(c);
  return result;
}

Text::Text(vector<uint16_t> &&content) : content{content} {}

Text::Text(TextSlice slice) : content{
  slice.text->content.begin() + slice.start_index,
  slice.text->content.begin() + slice.end_index
} {}

std::pair<TextSlice, TextSlice> Text::Split(Point position) const {
  size_t index{CharacterIndexForPosition(position)};
  return {
    TextSlice{this, 0, index},
    TextSlice{this, index, content.size()}
  };
}

TextSlice Text::Prefix(Point prefix_end) const {
  return Split(prefix_end).first;
}

TextSlice Text::Suffix(Point suffix_start) const {
  return Split(suffix_start).second;
}

TextSlice Text::AsSlice() const {
  return TextSlice{this, 0, content.size()};
}

void Text::Append(TextSlice slice) {
  content.insert(
    content.end(),
    slice.text->content.begin() + slice.start_index,
    slice.text->content.begin() + slice.end_index
  );
}

void Text::TrimLeft(Point position) {
  content.erase(content.begin(), content.begin() + CharacterIndexForPosition(position));
}

void Text::TrimRight(Point position) {
  content.erase(content.begin() + CharacterIndexForPosition(position), content.end());
}

size_t Text::CharacterIndexForPosition(Point target) const {
  size_t index{0}, size{content.size()};
  Point position;

  while (index < size && position < target) {
    if (content[index] == '\n') {
      position.row++;
      position.column = 0;
    } else {
      position.column++;
    }
    index++;
  }

  return index;
}
