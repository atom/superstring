#include "display-index.h"
#include "text-slice.h"

using std::vector;

DisplayIndex::DisplayIndex(
  TextBuffer *buffer,
  DisplayIndex::Params params
) : buffer(buffer), params(params) {
  handle_buffer_change({Point(), buffer->extent(), buffer->extent()});
}

Point DisplayIndex::translate_screen_position(
  Point point,
  ClipDirection direction,
  bool skip_soft_wrap_indentation
) {
  return point;
}

Point DisplayIndex::translate_buffer_position(
  Point point,
  ClipDirection direction
) {
  return point;
}

vector<Range> DisplayIndex::get_folds_in_buffer_row_range(uint32_t start, uint32_t end) {
  vector<Range> folds;
  auto fold_markers = folds_marker_index.find_intersecting({start, 0}, {end, UINT32_MAX}).to_vector();

  for (unsigned i = 0, n = fold_markers.size(); i < n; i++) {
    Point fold_start = folds_marker_index.get_start(fold_markers[i]);
    Point fold_end = folds_marker_index.get_end(fold_markers[i]);

    // Merge overlapping folds
    while (i < fold_markers.size() - 1) {
      auto next_fold_marker = fold_markers[i + 1];
      if (folds_marker_index.get_start(next_fold_marker) < fold_end) {
        Point next_fold_end = folds_marker_index.get_end(next_fold_marker);
        if (next_fold_end > fold_end) {
          fold_end = next_fold_end;
        }
        i++;
      } else {
        break;
      }
    }

    if (fold_start == fold_end) continue;
    folds.push_back({fold_start, fold_end});
  }

  return folds;
}


uint32_t DisplayIndex::find_boundary_preceding_buffer_row(uint32_t buffer_row) {
  while (true) {
    if (buffer_row == 0) return 0;
    auto screen_position = translate_buffer_position(Point{buffer_row, 0}, Backward);
    auto buffer_position = translate_screen_position(Point{screen_position.row, 0}, Backward);
    if (screen_position.column == 0 && buffer_position.column == 0) {
      return buffer_position.row;
    } else {
      buffer_row = buffer_position.row;
    }
  }
}

uint32_t DisplayIndex::find_boundary_following_buffer_row(uint32_t buffer_row) {
  while (true) {
    auto screen_position = translate_buffer_position(Point{buffer_row, 0}, Forward);
    if (screen_position.column == 0) {
      return buffer_row;
    } else {
      auto end_of_screen_row = Point{
        screen_position.row,
        screen_line_lengths[screen_position.row]
      };
      buffer_row = translate_screen_position(end_of_screen_row, Forward).row + 1;
    }
  }
}

static bool is_wrap_boundary(char16_t left, char16_t right) {

}

static 

DisplayIndex::Change DisplayIndex::handle_buffer_change(DisplayIndex::Change buffer_change) {
  uint32_t start_buffer_row = find_boundary_preceding_buffer_row(buffer_change.start.row);
  uint32_t old_end_buffer_row = find_boundary_following_buffer_row(buffer_change.old_end.row);
  uint32_t new_end_buffer_row = buffer_change.new_end.row + (old_end_buffer_row - buffer_change.old_end.row);

  uint32_t start_screen_row = translate_buffer_position({start_buffer_row, 0}, Backward).row;
  uint32_t old_end_screen_row = translate_buffer_position({old_end_buffer_row, 0}, Backward).row;
  patch.splice_old(
    {start_buffer_row, 0},
    {old_end_buffer_row - start_buffer_row, 0},
    {new_end_buffer_row - start_buffer_row, 0}
  );

  auto folds = get_folds_in_buffer_row_range(start_buffer_row, new_end_buffer_row);
  uint32_t consumed_fold_count = 0;

  vector<uint32_t> inserted_screen_line_lengths;
  vector<uint32_t> inserted_tab_counts;
  vector<uint32_t> current_screen_line_tab_columns;
  Point rightmost_inserted_screen_position = {0, UINT32_MAX};

  uint32_t buffer_row = start_buffer_row;
  uint32_t screen_row = start_screen_row;
  uint32_t buffer_column = 0;
  uint32_t unexpanded_screen_column = 0;
  uint32_t expanded_screen_column = 0;

  auto text_slices = buffer->chunks_in_range({
    {buffer_row, buffer_column},
    {new_end_buffer_row, 0}
  });

  auto slices_iter = text_slices.begin();
  auto slices_end = text_slices.end();
  if (slices_iter == slices_end) return buffer_change;

  auto slice_iter = slices_iter->begin();
  auto slice_end = slices_iter->end();

  uint32_t screen_line_width = 0;
  uint32_t last_wrap_boundary_unexpanded_screen_column = 0;
  uint32_t last_wrap_boundary_expanded_screen_column = 0;
  uint32_t last_wrap_boundary_screen_line_width = 0;
  uint32_t first_non_whitespace_screen_column = UINT32_MAX;
  char16_t previous_character = 0;

  while (true) {
    if (slice_iter == slice_end) {
      ++slices_iter;
      if (slices_iter == slice_end) break;
    }

    // Determine if there's a fold at the current position
    Point fold_end;
    for (;;) {
      if (consumed_fold_count >= folds.size()) break;
      if (folds[consumed_fold_count].start.row < buffer_row) {
        consumed_fold_count++;
      } else if (folds[consumed_fold_count].start.row < buffer_row) {
        fold_end = folds[consumed_fold_count.end];
      }
    }

    char16_t character = fold_end.is_zero() ? *slice_iter : params.fold_character;

    if (first_non_whitespace_screen_column == UINT32_MAX) {
      if (character != ' ' && character != '\t') {
        first_non_whitespace_screen_column = expanded_screen_column;
      }
    } else {
      if (previous_character && character && is_wrap_boundary(previousCharacter, character)) {
        last_wrap_boundary_unexpanded_screen_column = unexpanded_screen_column;
        last_wrap_boundary_expanded_screen_column = expanded_screen_column;
        last_wrap_boundary_screen_line_width = screen_line_width;
      }
    }

    uint32_t character_width;
    if (character == '\t') {
      const distanceToNextTabStop = this.tabLength - (expandedScreenColumn % this.tabLength)
      characterWidth = this.ratioForCharacter(' ') * distanceToNextTabStop
    }
  }

  return buffer_change;
}
