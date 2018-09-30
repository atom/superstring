#ifndef SUPERSTRING_DISPLAY_INDEX_H_
#define SUPERSTRING_DISPLAY_INDEX_H_

#include <vector>
#include "point.h"
#include "range.h"
#include "patch.h"
#include "text-buffer.h"
#include "marker-index.h"

class DisplayIndex {
 public:
  struct Change {
    Point start;
    Point old_end;
    Point new_end;
  };

  struct Params {
    uint32_t tab_length = 4;
    uint32_t soft_wrap_column = 0;
    char16_t fold_character = 0;
  };

  enum ClipDirection {
    Closest,
    Forward,
    Backward,
  };

  DisplayIndex(TextBuffer *, Params);
  Change handle_buffer_change(Change);
  Point translate_buffer_position(
    Point,
    ClipDirection direction = Closest
  );
  Point translate_screen_position(
    Point,
    ClipDirection direction = Closest,
    bool skip_soft_wrap_indentation = false
  );

 private:
  std::vector<Range> get_folds_in_buffer_row_range(uint32_t, uint32_t);
  uint32_t find_boundary_preceding_buffer_row(uint32_t);
  uint32_t find_boundary_following_buffer_row(uint32_t);

  TextBuffer *buffer;
  Params params;
  MarkerIndex folds_marker_index;
  Patch patch;
  std::vector<uint32_t> tab_counts;
  std::vector<uint32_t> screen_line_lengths;
  Point rightmost_screen_position;
};

#endif  // SUPERSTRING_DISPLAY_INDEX_H_
