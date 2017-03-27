#include "test-helpers.h"
#include <sstream>
#include "text-buffer.h"
#include "text-slice.h"

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

  auto snapshot2 = buffer.create_snapshot();
  auto snapshot3 = buffer.create_snapshot();
  buffer.set_text_in_range({{0, 6}, {0, 6}}, Text {u"456"});
  REQUIRE(buffer.text() == Text {u"abc123456\ndef"});
  REQUIRE(buffer.line_length_for_row(0) == 9);
  REQUIRE(snapshot2->text() == Text {u"abc123\ndef"});
  REQUIRE(snapshot2->line_length_for_row(0) == 6);
  REQUIRE(snapshot1->text() == Text {u"abc\ndef"});
  REQUIRE(snapshot1->line_length_for_row(0) == 3);

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

TEST_CASE("TextBuffer::set_text_in_range - random edits") {
  auto t = time(nullptr);
  for (uint i = 0; i < 1000; i++) {
    auto seed = t * 1000 + i;
    srand(seed);
    printf("Seed: %ld\n", seed);

    TextBuffer buffer {get_random_string()};
    vector<pair<const TextBuffer::Snapshot *, Text>> snapshots;

    for (uint j = 0; j < 10; j++) {
      // printf("iteration %u\n", j);

      Text original_text = buffer.text();
      Range deleted_range = get_random_range(buffer);
      Text inserted_text = get_random_text();

      if (rand() % 2) {
        // printf("create snapshot %lu\n", snapshots.size());

        snapshots.push_back({buffer.create_snapshot(), original_text});
      }

      buffer.set_text_in_range(deleted_range, TextSlice {inserted_text});
      original_text.splice(deleted_range.start, deleted_range.extent(), TextSlice {inserted_text});

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
        REQUIRE(
          buffer.text_in_range(range) ==
          Text{TextSlice{original_text}.suffix(range.start).prefix(range.extent())}
        );
      }

      for (uint32_t k = 0; k < snapshots.size(); k++) {
        // printf("check snapshot %u of %lu\n", k, snapshots.size());

        auto &entry = snapshots[k];
        Range range = get_random_range(entry.second);
        REQUIRE(entry.first->text() == entry.second);
        REQUIRE(
          entry.first->text_in_range(range) ==
          Text{TextSlice{entry.second}.suffix(range.start).prefix(range.extent())}
        );
      }

      if (rand() % 3 == 0 && !snapshots.empty()) {
        uint32_t snapshot_index = rand() % snapshots.size();
        // printf("delete snapshot %u of %lu\n", snapshot_index, snapshots.size());

        delete snapshots[snapshot_index].first;
        snapshots.erase(snapshots.begin() + snapshot_index);
      }
    }
  }
}
