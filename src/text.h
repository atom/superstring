#ifndef TEXT_H_
#define TEXT_H_

#include <vector>
#include <memory>
#include "point.h"

struct Text;

struct TextSlice {
  const Text *text;
  size_t start_index;
  size_t end_index;
};

struct Text {
  std::vector<uint16_t> content;

  static std::unique_ptr<Text> Build(std::vector<uint16_t> &content);
  static std::unique_ptr<Text> Concat(TextSlice a, TextSlice b);
  static std::unique_ptr<Text> Concat(TextSlice a, TextSlice b, TextSlice c);

  Text(std::vector<uint16_t> &&content);
  Text(TextSlice slice);

  std::pair<TextSlice, TextSlice> Split(Point position) const;
  TextSlice Prefix(Point prefix_end) const;
  TextSlice Suffix(Point suffix_start) const;
  TextSlice AsSlice() const;
  void TrimLeft(Point position);
  void TrimRight(Point position);
  void Append(TextSlice slice);

private:
  size_t CharacterIndexForPosition(Point) const;
};

#endif // TEXT_H_
