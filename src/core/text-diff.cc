#include "text-diff.h"
#include "diff_match_patch.h"
#include "text-slice.h"
#include <vector>
#include <string.h>
#include <ostream>
#include <cassert>

using std::move;
using std::ostream;
using std::vector;
using std::u16string;

// The `diff-match-patch` library operates on basic_string-compatible objects.
// Our `Text` stores its content in a `vector` rather than a `string` because
// we can construct a vector from a v8::String efficiently by writing directly
// to its internal buffer via the `.data()` method. So in order to diff two
// `Text` instances, we need to adapt them to the `basic_string` interface.
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

  vector_type content_;
  const vector_type *borrowed_content_;

  VectorStringAdapter() : borrowed_content_{nullptr} {}
  VectorStringAdapter(vector_type &&content) : content_{move(content)}, borrowed_content_{nullptr} {}
  VectorStringAdapter(const vector_type *borrowed_content) : borrowed_content_{borrowed_content} {}

  vector_type &content() { assert(!borrowed_content_); return content_; }
  const vector_type &content() const { return borrowed_content_ ? *borrowed_content_ : content_; }

  bool empty() const { return content().empty(); }
  size_t size() const { return content().size(); }
  size_t length() const { return content().size(); }
  value_type operator[](size_type index) const { return content()[index]; }
  void clear() { content().clear(); }
  const_pointer c_str() const { return content().data(); }
  const_iterator begin() const { return content().begin(); }
  const_iterator end() const { return content().end(); }
  const_reverse_iterator rbegin() const { return content().rbegin(); }
  const_reverse_iterator rend() const { return content().rend(); }
  bool operator==(const VectorStringAdapter &other) const { return content() == other.content(); }
  bool operator!=(const VectorStringAdapter &other) const { return content() != other.content(); }
  void swap(VectorStringAdapter &other) { content().swap(other.content()); }

  int compare(size_type position, size_type length, const VectorStringAdapter &other) const {
    return traits_type::compare(content().data() + position, other.c_str(), length);
  }

  VectorStringAdapter substr(size_type start, size_type length) const {
    return VectorStringAdapter(vector_type(
      content().begin() + start,
      content().begin() + start + length
    ));
  }

  VectorStringAdapter substr(size_type start) const {
    return VectorStringAdapter(vector_type(content().begin() + start, content().end()));
  }

  size_type find(VectorStringAdapter substring) const {
    return find(substring, 0);
  }

  size_type find(VectorStringAdapter substring, size_t start) const {
    auto begin = content().begin() + start;
    auto iter = std::search(begin, content().end(), substring.content().begin(), substring.content().end());
    return iter == content().end() ? npos : iter - begin;
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
    content().insert(content().end(), begin, begin + length);
  }

  void operator+=(const VectorStringAdapter &other) {
    content().insert(content().end(), other.content().begin(), other.content().end());
  }

  void operator+=(uint16_t character) {
    content().push_back(character);
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

static Point previous_column(Point position) {
  assert(position.column > 0);
  position.column--;
  return position;
}

Patch text_diff(const Text &old_text, const Text &new_text) {
  Patch result;

  VectorStringAdapter old_string(&old_text.content);
  VectorStringAdapter new_string(&new_text.content);

  DiffBuilder diff_builder;
  diff_builder.Diff_Timeout = -1;
  DiffBuilder::Diffs diffs = diff_builder.diff_main(old_string, new_string);

  size_t old_offset = 0;
  size_t new_offset = 0;
  Point old_position;
  Point new_position;

  Text empty;
  Text cr{u"\r"};
  Text lf{u"\n"};

  for (DiffBuilder::Diff &diff : diffs) {
    switch (diff.operation) {
      case DiffBuilder::Operation::EQUAL:

        // If the previous change ended between a CR and an LF, then expand
        // that change downward to include the LF.
        if (new_text.at(new_offset) == '\n' &&
            ((old_offset > 0 && old_text.at(old_offset - 1) == '\r') ||
             (new_offset > 0 && new_text.at(new_offset - 1) == '\r'))) {
          result.splice(new_position, Point(1, 0), Point(1, 0), lf, lf);
          old_position.row++;
          old_position.column = 0;
          new_position.row++;
          new_position.column = 0;
        }

        old_offset += diff.text.size();
        new_offset += diff.text.size();
        old_position = old_text.position_for_offset(old_offset, false);
        new_position = new_text.position_for_offset(new_offset, false);

        // If the next change starts between a CR and an LF, then expand that
        // change leftward to include the CR.
        if (new_text.at(new_offset - 1) == '\r' &&
            ((old_offset < old_text.size() && old_text.at(old_offset) == '\n') ||
             (new_offset < new_text.size() && new_text.at(new_offset) == '\n'))) {
          result.splice(previous_column(new_position), Point(0, 1), Point(0, 1), cr, cr);
        }
        break;

      case DiffBuilder::Operation::DELETE: {
        Text deleted_text{move(diff.text.content())};
        old_offset += diff.text.size();
        Point next_old_position = old_text.position_for_offset(old_offset, false);
        result.splice(new_position, next_old_position.traversal(old_position), Point(), deleted_text, empty);
        old_position = next_old_position;
        break;
      }

      case DiffBuilder::Operation::INSERT: {
        Text inserted_text{move(diff.text.content())};
        new_offset += diff.text.size();
        Point next_new_position = new_text.position_for_offset(new_offset, false);
        result.splice(new_position, Point(), next_new_position.traversal(new_position), empty, inserted_text);
        new_position = next_new_position;
        break;
      }
    }
  }

  return result;
}
