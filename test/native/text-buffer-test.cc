#include "test-helpers.h"
#include <sstream>
#include "text-buffer.h"
#include "text-slice.h"
#include <future>
#include <unistd.h>
#include <iostream>

using std::move;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;

TEST_CASE("TextBuffer::set_text_in_range - basic") {
  TextBuffer buffer {u"abc\ndef\nghi"};
  buffer.set_text_in_range(Range {{0, 2}, {2, 1}}, Text {u"jkl\nmno"});
  REQUIRE(buffer.text() == Text {u"abjkl\nmnohi"});
  REQUIRE(buffer.text_in_range(Range {{0, 1}, {1, 4}}) == Text {u"bjkl\nmnoh"});

  buffer.set_text_in_range(Range {{0, 0}, {10, 1}}, Text {u"yz"});
  REQUIRE(buffer.text() == Text {u"yz"});
  REQUIRE(buffer.text_in_range(Range {{0, 1}, {10, 1}}) == Text {u"z"});
}

TEST_CASE("TextBuffer::line_length_for_row - basic") {
  TextBuffer buffer {u"a\n\nb\r\rc\r\n\r\n"};
  REQUIRE(buffer.line_length_for_row(0) == 1);
  REQUIRE(buffer.line_length_for_row(1) == 0);
}

TEST_CASE("TextBuffer::position_for_offset") {
  TextBuffer buffer {u"ab\ndef\r\nhijk"};
  buffer.set_text_in_range({{0, 2}, {0, 2}}, Text{u"c"});
  buffer.set_text_in_range({{1, 3}, {1, 3}}, Text{u"g"});
  REQUIRE(buffer.position_for_offset(0) == Point(0, 0));
  REQUIRE(buffer.position_for_offset(1) == Point(0, 1));
  REQUIRE(buffer.position_for_offset(2) == Point(0, 2));
  REQUIRE(buffer.position_for_offset(3) == Point(0, 3));
  REQUIRE(buffer.position_for_offset(4) == Point(1, 0));
  REQUIRE(buffer.position_for_offset(5) == Point(1, 1));
  REQUIRE(buffer.position_for_offset(7) == Point(1, 3));
  REQUIRE(buffer.position_for_offset(8) == Point(1, 4));
  REQUIRE(buffer.position_for_offset(9) == Point(1, 4));
  REQUIRE(buffer.position_for_offset(10) == Point(2, 0));
}

TEST_CASE("TextBuffer::create_snapshot") {
  TextBuffer buffer {u"ab\ndef"};
  buffer.set_text_in_range({{0, 2}, {0, 2}}, Text {u"c"});
  REQUIRE(buffer.text() == Text {u"abc\ndef"});
  REQUIRE(buffer.line_length_for_row(0) == 3);

  auto snapshot1 = buffer.create_snapshot();
  buffer.set_text_in_range({{0, 3}, {0, 3}}, Text {u"123"});
  REQUIRE(buffer.text() == Text {u"abc123\ndef"});
  REQUIRE(buffer.line_length_for_row(0) == 6);
  REQUIRE(snapshot1->text() == Text {u"abc\ndef"});
  REQUIRE(snapshot1->line_length_for_row(0) == 3);
  REQUIRE(snapshot1->line_length_for_row(1) == 3);

  auto snapshot2 = buffer.create_snapshot();
  auto snapshot3 = buffer.create_snapshot();
  buffer.set_text_in_range({{0, 6}, {0, 6}}, Text {u"456"});
  REQUIRE(buffer.text() == Text {u"abc123456\ndef"});
  REQUIRE(buffer.line_length_for_row(0) == 9);
  REQUIRE(snapshot2->text() == Text {u"abc123\ndef"});
  REQUIRE(snapshot2->line_length_for_row(0) == 6);
  REQUIRE(snapshot2->line_length_for_row(1) == 3);
  REQUIRE(snapshot1->text() == Text {u"abc\ndef"});
  REQUIRE(snapshot1->line_length_for_row(0) == 3);
  REQUIRE(snapshot1->line_length_for_row(1) == 3);

  SECTION("deleting the latest snapshot") {
    delete snapshot2;
    delete snapshot3;
    REQUIRE(buffer.text() == Text {u"abc123456\ndef"});
    REQUIRE(buffer.line_length_for_row(0) == 9);
    REQUIRE(snapshot1->text() == Text {u"abc\ndef"});
    REQUIRE(snapshot1->line_length_for_row(0) == 3);
    delete snapshot1;
  }

  SECTION("deleting an earlier snapshot first") {
    delete snapshot1;
    REQUIRE(buffer.text() == Text {u"abc123456\ndef"});
    REQUIRE(buffer.line_length_for_row(0) == 9);
    REQUIRE(snapshot2->text() == Text {u"abc123\ndef"});
    REQUIRE(snapshot2->line_length_for_row(0) == 6);
    delete snapshot2;
    delete snapshot3;
  }
}

TEST_CASE("TextBuffer::is_modified") {
  TextBuffer buffer;
  REQUIRE(!buffer.is_modified());

  auto snapshot1 = buffer.create_snapshot();
  REQUIRE(!buffer.is_modified());

  buffer.set_text_in_range({{0, 0}, {0, 0}}, Text{u"a"});
  REQUIRE(buffer.is_modified());

  auto snapshot2 = buffer.create_snapshot();
  REQUIRE(buffer.is_modified());

  delete snapshot1;
  delete snapshot2;
  REQUIRE(buffer.is_modified());
}

struct SnapshotData {
  Text text;
  Point extent;
  vector<Point> line_end_positions;
};

struct SnapshotTask {
  const TextBuffer::Snapshot *snapshot;
  Text original_text;
  std::future<vector<SnapshotData>> future;
};

TEST_CASE("TextBuffer::set_text_in_range - random edits") {
  auto t = time(nullptr);
  for (uint i = 0; i < 100; i++) {
    uint32_t seed = t * 1000 + i;
    Generator rand(seed);
    printf("seed: %u\n", seed);

    TextBuffer buffer {get_random_string()};
    vector<SnapshotTask> snapshot_tasks;

    for (uint j = 0; j < 10; j++) {
      // printf("iteration %u\n", j);

      Text original_text = buffer.text();
      Range deleted_range = get_random_range(buffer);
      Text inserted_text = get_random_text();

      if (rand() % 2) {
        // printf("create snapshot %lu\n", snapshot_tasks.size());

        auto snapshot = buffer.create_snapshot();
        snapshot_tasks.push_back({
          snapshot,
          original_text,
          std::async([seed, snapshot]() {
            Generator rand(seed);
            vector<SnapshotData> results;
            for (uint32_t k = 0; k < 5; k++) {
              usleep(rand() % 1000);
              vector<Point> line_ending_positions;
              for (uint32_t row = 0; row < snapshot->extent().row; row++) {
                line_ending_positions.push_back({row, snapshot->line_length_for_row(row)});
              }
              results.push_back({
                snapshot->text(),
                snapshot->extent(),
                line_ending_positions
              });
            }
            return results;
          })
        });
      }

      original_text.splice(deleted_range.start, deleted_range.extent(), TextSlice{inserted_text});
      buffer.set_text_in_range(deleted_range, move(inserted_text));

      REQUIRE(buffer.extent() == original_text.extent());
      REQUIRE(buffer.text() == original_text);

      for (uint32_t row = 0; row < original_text.extent().row; row++) {
        REQUIRE(
          Point(row, buffer.line_length_for_row(row)) ==
          Point(row, original_text.line_length_for_row(row))
        );
      }

      for (uint32_t k = 0; k < 5; k++) {
        // printf("check random range %u\n", k);

        Range range = get_random_range(buffer);
        REQUIRE(buffer.text_in_range(range) == Text(TextSlice(original_text).slice(range)));
        REQUIRE(buffer.position_for_offset(buffer.clip_position(range.start).offset) == range.start);
        REQUIRE(buffer.position_for_offset(buffer.clip_position(range.end).offset) == range.end);
      }

      if (rand() % 3 == 0 && !snapshot_tasks.empty()) {
        uint32_t snapshot_index = rand() % snapshot_tasks.size();
        // printf("delete snapshot %u of %lu\n", snapshot_index, snapshot_tasks.size());

        snapshot_tasks[snapshot_index].future.wait();
        auto original_text = snapshot_tasks[snapshot_index].original_text;
        delete snapshot_tasks[snapshot_index].snapshot;
        for (auto data : snapshot_tasks[snapshot_index].future.get()) {
          REQUIRE(data.text == original_text);
          REQUIRE(data.extent == original_text.extent());
          for (auto position : data.line_end_positions) {
            REQUIRE(position == Point(position.row, original_text.line_length_for_row(position.row)));
          }
        }
        snapshot_tasks.erase(snapshot_tasks.begin() + snapshot_index);
      }
    }

    for (auto &task : snapshot_tasks) {
      task.future.wait();
      delete task.snapshot;
    }
  }
}
