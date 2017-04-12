#include "text-diff.h"
#include "diff_match_patch.h"
#include <vector>
#include <string.h>
#include <ostream>
#include <cassert>

using std::ostream;
using std::vector;
using std::u16string;

struct VectorStringAdapter {
  using vector_type = vector<uint16_t>;
  using value_type = vector_type::value_type;
  using size_type = vector_type::size_type;
  using const_pointer = vector_type::const_pointer;
  using const_iterator = vector_type::const_iterator;
  using const_reverse_iterator = vector_type::const_reverse_iterator;

  struct traits_type {
    static int compare(const_pointer left, const_pointer right, size_type length) {
      return std::basic_string<uint16_t>::traits_type::compare(left, right, length);
    }
  };

  static const size_type npos = -1;

  vector_type content;

  VectorStringAdapter() {}
  VectorStringAdapter(vector_type &&content) : content{move(content)} {}

  bool empty() const { return content.empty(); }
  size_t size() const { return content.size(); }
  size_t length() const { return content.size(); }
  value_type operator[](size_type index) const { return content[index]; }
  void clear() { content.clear(); }
  const_pointer c_str() const { return content.data(); }
  const_iterator begin() const { return content.begin(); }
  const_iterator end() const { return content.end(); }
  const_reverse_iterator rbegin() const { return content.rbegin(); }
  const_reverse_iterator rend() const { return content.rend(); }
  bool operator==(const VectorStringAdapter &other) const { return content == other.content; }
  bool operator!=(const VectorStringAdapter &other) const { return content != other.content; }
  void swap(VectorStringAdapter &other) { content.swap(other.content); }

  int compare(size_type position, size_type length, const VectorStringAdapter &other) const {
    return traits_type::compare(content.data() + position, other.c_str(), length);
  }

  VectorStringAdapter substr(size_type start, size_type length) const {
    return VectorStringAdapter(vector_type(
      content.begin() + start,
      content.begin() + start + length
    ));
  }

  VectorStringAdapter substr(size_type start) const {
    return VectorStringAdapter(vector_type(content.begin() + start, content.end()));
  }

  size_type find(VectorStringAdapter substring) const {
    return find(substring, 0);
  }

  size_type find(VectorStringAdapter substring, size_t start) const {
    auto begin = content.begin() + start;
    auto iter = std::search(begin, content.end(), substring.content.begin(), substring.content.end());
    return iter == content.end() ? npos : iter - begin;
  }


  VectorStringAdapter operator+(const VectorStringAdapter &other) const {
    VectorStringAdapter result = *this;
    result += other;
    return result;
  }

  VectorStringAdapter operator+(uint16_t character) const {
    VectorStringAdapter result = *this;
    result += character;
    return result;
  }

  void append(const_pointer begin, size_type length) {
    content.insert(content.end(), begin, begin + length);
  }

  void operator+=(const VectorStringAdapter &other) {
    content.insert(content.end(), other.content.begin(), other.content.end());
  }

  void operator+=(uint16_t character) {
    content.push_back(character);
  }
};

struct StringAdapterTraits {
  static bool is_alnum(uint16_t c) { return std::iswalnum(c); }
  static bool is_digit(uint16_t c) { return std::iswdigit(c); }
  static bool is_space(uint16_t c) { return std::iswspace(c); }
  static wchar_t to_wchar(uint16_t c) { return c; }
  static uint16_t from_wchar(wchar_t c) { return c; }
  static const uint16_t eol = '\n';
  static const uint16_t tab = '\t';

  // These functions are not used when computing diffs; the diff-match-patch
  // library uses them in functions that we don't call.
  static VectorStringAdapter cs(const wchar_t* s) { assert(false); return VectorStringAdapter(); }
  static int to_int(const uint16_t* s) { assert(false); return 0; }
};

using DiffBuilder = diff_match_patch<VectorStringAdapter, StringAdapterTraits>;

static inline Patch text_diff(Text *old_text, Text *new_text) {
  Patch result;

  VectorStringAdapter old_string(move(old_text->content));
  VectorStringAdapter new_string(move(new_text->content));

  DiffBuilder diff_builder;
  diff_builder.Diff_Timeout = -1;
  DiffBuilder::Diffs diffs = diff_builder.diff_main(old_string, new_string);

  old_text->content = move(old_string.content);
  new_text->content = move(new_string.content);

  size_t old_offset = 0;
  size_t new_offset = 0;
  Point old_position;
  Point new_position;

  for (const DiffBuilder::Diff &diff : diffs) {
    size_t length = diff.text.size();
    switch (diff.operation) {
      case DiffBuilder::Operation::EQUAL: {
        old_offset += length;
        new_offset += length;
        old_position = old_text->position_for_offset(old_offset);
        new_position = new_text->position_for_offset(new_offset);
        break;
      }

      case DiffBuilder::Operation::DELETE: {
        old_offset += length;
        Point next_old_position = old_text->position_for_offset(old_offset);
        result.splice(new_position, next_old_position.traversal(old_position), Point());
        old_position = next_old_position;
        break;
      }

      case DiffBuilder::Operation::INSERT: {
        new_offset += length;
        Point next_new_position = new_text->position_for_offset(new_offset);
        result.splice(new_position, Point(), next_new_position.traversal(new_position));
        new_position = next_new_position;
        break;
      }
    }
  }

  return result;
}

Patch text_diff(const Text &old_text, const Text &new_text) {
  return text_diff(
    const_cast<Text *>(&old_text),
    const_cast<Text *>(&new_text)
  );
}
