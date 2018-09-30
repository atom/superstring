#include "test-helpers.h"
#include "text-buffer.h"
#include "display-index.h"

using std::string;
using std::stringstream;
using std::vector;
using std::u16string;

void require_position_translations(
  const DisplayIndex &index,
  const vector<vector<Point>> &translations
) {
  for (auto &row : translations) {
    Point screen_position = row[0];
    Point backward_buffer_position = row[1];
    Point forward_buffer_position = row.size() > 2 ? row[2] : row[1];

    REQUIRE(index.translate_screen_position(
      screen_position,
      DisplayIndex::Backward
    ) == backward_buffer_position);

    REQUIRE(index.translate_screen_position(
      screen_position,
      DisplayIndex::Forward
    ) == forward_buffer_position);
  }
}

TEST_CASE("DisplayIndex - hard tabs") {
  TextBuffer buffer{u"\ta\tbc\tdef\tg\nh\t\ti"};

  DisplayIndex::Params params;
  params.tab_length = 4;
  DisplayIndex index(&buffer, params);

  require_position_translations(index, {
    {{0, 0}, {0, 0}},
    {{0, 1}, {0, 0}, {0, 1}},
    {{0, 2}, {0, 0}, {0, 1}},
    {{0, 3}, {0, 0}, {0, 1}},
    {{0, 4}, {0, 1}},
    {{0, 5}, {0, 2}},
    {{0, 6}, {0, 2}, {0, 3}},
    {{0, 7}, {0, 2}, {0, 3}},
    {{0, 8}, {0, 3}},
    {{0, 9}, {0, 4}},
    {{0, 10}, {0, 5}},
    {{0, 11}, {0, 5}, {0, 6}},
    {{0, 12}, {0, 6}},
    {{0, 13}, {0, 7}},
    {{0, 14}, {0, 8}},
    {{0, 15}, {0, 9}},
    {{0, 16}, {0, 10}},
    {{0, 17}, {0, 11}},
    {{0, 18}, {0, 11}, {1, 0}},
    {{1, 0}, {1, 0}},
    {{1, 1}, {1, 1}},
    {{1, 2}, {1, 1}, {1, 2}},
    {{1, 3}, {1, 1}, {1, 2}},
    {{1, 4}, {1, 2}},
    {{1, 5}, {1, 2}, {1, 3}},
    {{1, 2}, {1, 1}, {1, 2}},
    {{1, 3}, {1, 1}, {1, 2}},
    {{1, 4}, {1, 2}},
    {{1, 5}, {1, 2}, {1, 3}},
    {{1, 6}, {1, 2}, {1, 3}},
    {{1, 7}, {1, 2}, {1, 3}},
    {{1, 8}, {1, 3}},
  });
}
