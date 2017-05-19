#include "test-helpers.h"
#include <sstream>
#include "text-buffer.h"
#include "text-slice.h"
#include "regex.h"
#include <future>
#include <unistd.h>

using std::move;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;
using std::u16string;
using MatchResult = Regex::MatchResult;

TEST_CASE("TextBuffer::set_text_in_range - basic") {
  TextBuffer buffer {u"abc\ndef\nghi"};
  REQUIRE(buffer.text_in_range({{0, 1}, {0, UINT32_MAX}}) == Text{u"bc"});

  buffer.set_text_in_range(Range {{0, 2}, {2, 1}}, Text{u"jkl\nmno"});
  REQUIRE(buffer.text() == Text{u"abjkl\nmnohi"});
  REQUIRE(buffer.text_in_range(Range {{0, 1}, {1, 4}}) == Text{u"bjkl\nmnoh"});

  buffer.set_text_in_range(Range {{0, 0}, {10, 1}}, Text{u"yz"});
  REQUIRE(buffer.text() == Text{u"yz"});
  REQUIRE(buffer.text_in_range(Range {{0, 1}, {10, 1}}) == Text{u"z"});
}

TEST_CASE("TextBuffer::line_length_for_row - basic") {
  TextBuffer buffer {u"a\n\nb\r\rc\r\n\r\n"};
  REQUIRE(*buffer.line_length_for_row(0) == 1);
  REQUIRE(*buffer.line_length_for_row(1) == 0);
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
  REQUIRE(*buffer.line_length_for_row(0) == 3);

  auto snapshot1 = buffer.create_snapshot();
  buffer.set_text_in_range({{0, 3}, {0, 3}}, Text {u"123"});
  REQUIRE(buffer.text() == Text {u"abc123\ndef"});
  REQUIRE(*buffer.line_length_for_row(0) == 6);
  REQUIRE(snapshot1->text() == Text {u"abc\ndef"});
  REQUIRE(snapshot1->line_length_for_row(0) == 3);
  REQUIRE(snapshot1->line_length_for_row(1) == 3);

  auto snapshot2 = buffer.create_snapshot();
  auto snapshot3 = buffer.create_snapshot();
  buffer.set_text_in_range({{0, 6}, {0, 6}}, Text {u"456"});
  REQUIRE(buffer.text() == Text {u"abc123456\ndef"});
  REQUIRE(*buffer.line_length_for_row(0) == 9);
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
    REQUIRE(*buffer.line_length_for_row(0) == 9);
    REQUIRE(snapshot1->text() == Text {u"abc\ndef"});
    REQUIRE(snapshot1->line_length_for_row(0) == 3);
    delete snapshot1;
  }

  SECTION("deleting an earlier snapshot first") {
    delete snapshot1;
    REQUIRE(buffer.text() == Text {u"abc123456\ndef"});
    REQUIRE(*buffer.line_length_for_row(0) == 9);
    REQUIRE(snapshot2->text() == Text {u"abc123\ndef"});
    REQUIRE(snapshot2->line_length_for_row(0) == 6);
    delete snapshot2;
    delete snapshot3;
  }
}

TEST_CASE("TextBuffer::get_inverted_changes") {
  TextBuffer buffer {u"ab\ndef"};
  auto snapshot1 = buffer.create_snapshot();

  // This range gets clipped. The ::get_inverted_changes method is one of the
  // few reasons it matters that we clip ranges before passing them to
  // Patch::splice.
  buffer.set_text_in_range({{0, 2}, {0, 10}}, Text {u"c"});
  buffer.flush_changes();

  auto patch = buffer.get_inverted_changes(snapshot1);
  REQUIRE(patch.get_changes() == vector<Patch::Change>({
    Patch::Change{
      Point {0, 2}, Point {0, 3},
      Point {0, 2}, Point {0, 2},
      get_text(u"c").get(),
      get_text(u"").get(),
      0, 0
    },
  }));

  delete snapshot1;
}

TEST_CASE("TextBuffer::is_modified") {
  TextBuffer buffer{u"abcdef"};
  REQUIRE(!buffer.is_modified());

  buffer.set_text_in_range({{0, 1}, {0, 2}}, Text{u"BBB"});
  REQUIRE(buffer.is_modified());

  SECTION("undoing a change") {
    buffer.set_text_in_range({{0, 1}, {0, 4}}, Text{u"b"});
    REQUIRE(!buffer.is_modified());
  }

  SECTION("undoing a change after creating a snapshot") {
    auto snapshot1 = buffer.create_snapshot();
    buffer.set_text_in_range({{0, 1}, {0, 4}}, Text{u"b"});
    REQUIRE(!buffer.is_modified());
    delete snapshot1;
  }

  SECTION("undoing a change in multiple steps with snapshots in between") {
    auto snapshot1 = buffer.create_snapshot();
    buffer.set_text_in_range({{0, 3}, {0, 4}}, Text{});
    REQUIRE(buffer.is_modified());

    auto snapshot2 = buffer.create_snapshot();
    buffer.set_text_in_range({{0, 2}, {0, 3}}, Text{});
    REQUIRE(buffer.is_modified());

    auto snapshot3 = buffer.create_snapshot();
    buffer.set_text_in_range({{0, 1}, {0, 2}}, Text{});
    REQUIRE(buffer.is_modified());

    auto snapshot4 = buffer.create_snapshot();
    buffer.set_text_in_range({{0, 1}, {0, 1}}, Text{u"b"});
    REQUIRE(buffer.text() == Text{u"abcdef"});
    REQUIRE(!buffer.is_modified());

    delete snapshot1;
    delete snapshot2;
    delete snapshot3;
    delete snapshot4;
  }
}

TEST_CASE("TextBuffer::flush_changes") {
  TextBuffer buffer{u"abcdef"};
  REQUIRE(buffer.layer_count() == 1);

  buffer.set_text_in_range({{0, 1}, {0, 2}}, Text{u"B"});
  REQUIRE(buffer.text() == Text{u"aBcdef"});
  REQUIRE(buffer.is_modified());
  REQUIRE(buffer.layer_count() == 2);

  auto snapshot1 = buffer.create_snapshot();
  REQUIRE(buffer.layer_count() == 2);

  buffer.set_text_in_range({{0, 2}, {0, 3}}, Text{u"C"});
  REQUIRE(buffer.text() == Text{u"aBCdef"});
  REQUIRE(buffer.is_modified());
  REQUIRE(buffer.layer_count() == 3);

  auto snapshot2 = buffer.create_snapshot();
  REQUIRE(buffer.layer_count() == 3);

  buffer.flush_changes();
  REQUIRE(!buffer.is_modified());
  REQUIRE(buffer.base_text() == Text{u"aBCdef"});
  REQUIRE(buffer.text() == Text{u"aBCdef"});

  REQUIRE(snapshot1->text() == Text{u"aBcdef"});
  REQUIRE(snapshot2->text() == Text{u"aBCdef"});

  delete snapshot2;
  REQUIRE(buffer.layer_count() == 3);

  delete snapshot1;
  REQUIRE(buffer.layer_count() == 1);
}

TEST_CASE("Snapshot::flush_preceding_changes") {
  TextBuffer buffer{u"abcdef"};
  REQUIRE(buffer.layer_count() == 1);

  buffer.set_text_in_range({{0, 1}, {0, 2}}, Text{u"B"});
  REQUIRE(buffer.text() == Text{u"aBcdef"});
  REQUIRE(buffer.is_modified());
  auto snapshot1 = buffer.create_snapshot();
  REQUIRE(buffer.layer_count() == 2);

  buffer.set_text_in_range({{0, 2}, {0, 3}}, Text{u"C"});
  REQUIRE(buffer.text() == Text{u"aBCdef"});
  REQUIRE(buffer.is_modified());
  auto snapshot2 = buffer.create_snapshot();
  REQUIRE(buffer.layer_count() == 3);

  vector<uint8_t> bytes;

  SECTION("flushing the latest snapshot's changes") {
    snapshot2->flush_preceding_changes();

    REQUIRE(buffer.text() == Text{u"aBCdef"});
    REQUIRE(buffer.base_text() == Text{u"aBCdef"});
    REQUIRE(snapshot1->text() == Text{u"aBcdef"});
    REQUIRE(snapshot2->text() == Text{u"aBCdef"});
    REQUIRE(!buffer.is_modified());

    TextBuffer copy_buffer{Text{buffer.base_text()}};
    Serializer serializer(bytes);
    buffer.serialize_changes(serializer);
    Deserializer deserializer(bytes);
    copy_buffer.deserialize_changes(deserializer);

    REQUIRE(copy_buffer.base_text() == buffer.base_text());
    REQUIRE(copy_buffer.text() == buffer.text());
    REQUIRE(!copy_buffer.is_modified());

    delete snapshot1;
    delete snapshot2;
    REQUIRE(buffer.layer_count() == 1);
  }

  SECTION("flushing an earlier snapshot's changes") {
    snapshot1->flush_preceding_changes();

    REQUIRE(buffer.text() == Text{u"aBCdef"});
    REQUIRE(buffer.base_text() == Text{u"aBcdef"});
    REQUIRE(snapshot1->text() == Text{u"aBcdef"});
    REQUIRE(snapshot2->text() == Text{u"aBCdef"});
    REQUIRE(buffer.is_modified());

    TextBuffer copy_buffer{Text{buffer.base_text()}};
    Serializer serializer(bytes);
    buffer.serialize_changes(serializer);
    Deserializer deserializer(bytes);
    copy_buffer.deserialize_changes(deserializer);

    REQUIRE(copy_buffer.base_text() == buffer.base_text());
    REQUIRE(copy_buffer.text() == buffer.text());
    REQUIRE(copy_buffer.is_modified());

    delete snapshot1;
    delete snapshot2;
    REQUIRE(buffer.layer_count() == 2);
  }
}

TEST_CASE("TextBuffer::reset") {
  TextBuffer buffer{u"abcdef"};
  auto snapshot1 = buffer.create_snapshot();

  buffer.set_text_in_range({{0, 1}, {0, 2}}, Text{u"B"});
  auto snapshot2 = buffer.create_snapshot();

  buffer.set_text_in_range({{0, 2}, {0, 3}}, Text{u"C"});
  auto snapshot3 = buffer.create_snapshot();

  buffer.reset(Text{u"123"});

  REQUIRE(!buffer.is_modified());
  REQUIRE(buffer.text() == Text{u"123"});
  REQUIRE(snapshot1->text() == Text{u"abcdef"});
  REQUIRE(snapshot2->text() == Text{u"aBcdef"});
  REQUIRE(snapshot3->text() == Text{u"aBCdef"});

  delete snapshot1;
  delete snapshot2;
  delete snapshot3;

  REQUIRE(!buffer.is_modified());
  REQUIRE(buffer.layer_count() == 1);
  REQUIRE(buffer.text() == Text{u"123"});

  buffer.reset(Text{u"456"});
  REQUIRE(!buffer.is_modified());
  REQUIRE(buffer.layer_count() == 1);
  REQUIRE(buffer.text() == Text{u"456"});
}

TEST_CASE("TextBuffer::search") {
  TextBuffer buffer{u"abcd\nef"};

  REQUIRE(buffer.search(u"(").error_message == u"missing closing parenthesis");

  REQUIRE(buffer.search(u"").range == (Range{{0, 0}, {0, 0}}));
  REQUIRE(buffer.search(u"ef*").range == (Range{{1, 0}, {1, 2}}));
  REQUIRE(buffer.search(u"x").range == optional<Range>{});
  REQUIRE(*buffer.search(u"c.").range == (Range{{0, 2}, {0, 4}}));
  REQUIRE(*buffer.search(u"d").range == (Range{{0, 3}, {0, 4}}));
  REQUIRE(*buffer.search(u"\\n").range == (Range{{0, 4}, {1, 0}}));
  REQUIRE(*buffer.search(u"\\be").range == (Range{{1, 0}, {1, 1}}));
  REQUIRE(*buffer.search(u"^e").range == (Range{{1, 0}, {1, 1}}));
  REQUIRE(*buffer.search(u"^(e|d)g?").range == (Range{{1, 0}, {1, 1}}));

  buffer.reset(Text{u"a1b"});
  REQUIRE(*buffer.search(u"\\d").range == (Range{{0, 1}, {0, 2}}));
}

TEST_CASE("TextBuffer::search - spanning edits") {
  TextBuffer buffer{u"abcd"};
  buffer.set_text_in_range({{0, 2}, {0, 2}}, Text{u"12345"});
  buffer.set_text_in_range({{0, 9}, {0, 9}}, Text{u"67890"});

  REQUIRE(buffer.text() == Text(u"ab12345cd67890"));
  REQUIRE(*buffer.search(u"b1234").range == (Range{{0, 1}, {0, 6}}));
  REQUIRE(*buffer.search(u"b12345c").range == (Range{{0, 1}, {0, 8}}));
  REQUIRE(*buffer.search(u"b12345cd6").range == (Range{{0, 1}, {0, 10}}));
  REQUIRE(*buffer.search(u"345[a-z][a-z]").range == (Range{{0, 4}, {0, 9}}));
  REQUIRE(*buffer.search(u"5cd6").range == (Range{{0, 6}, {0, 10}}));

  buffer.reset(Text{u"abcdef"});
  buffer.set_text_in_range({{0, 2}, {0, 4}}, Text{u""});
  REQUIRE(buffer.text() == Text(u"abef"));
  REQUIRE(*buffer.search(u"abe").range == (Range{{0, 0}, {0, 3}}));
  REQUIRE(*buffer.search(u"bef").range == (Range{{0, 1}, {0, 4}}));
  REQUIRE(buffer.search(u"bc").range == optional<Range>{});
}

struct SnapshotData {
  Text base_text;
  Text text;
  Point extent;
  vector<Point> line_end_positions;
};

struct SnapshotTask {
  TextBuffer::Snapshot *snapshot;
  Text base_text;
  Text mutated_text;
  std::future<vector<SnapshotData>> future;
};

TEST_CASE("TextBuffer - random edits and queries") {
  TextBuffer::MAX_CHUNK_SIZE_TO_COPY = 2;

  auto t = time(nullptr);
  for (uint i = 0; i < 2; i++) {
    uint32_t seed = t * 1000 + i;
    Generator rand(seed);
    cout << "seed: " << seed << "\n";

    Text original_text = get_random_text(rand);
    TextBuffer buffer{Text{original_text}};
    vector<SnapshotTask> snapshot_tasks;

    for (uint32_t k = 0; k < 5; k++) {
      Range range = get_random_range(rand, buffer);
      // cout << "check random range " << range << "\n";
      REQUIRE(buffer.text_in_range(range) == Text(TextSlice(original_text).slice(range)));
      REQUIRE(buffer.position_for_offset(buffer.clip_position(range.start).offset) == range.start);
      REQUIRE(buffer.position_for_offset(buffer.clip_position(range.end).offset) == range.end);
    }

    // cout << "edit: " << i << "\n";
    // cout << "extent: " << original_text.extent() << "\ntext: " << original_text << "\n";

    for (uint j = 0; j < 15; j++) {
      // cout << "iteration: " << j << "\n";

      Text mutated_text = buffer.text();
      Range deleted_range = get_random_range(rand, buffer);
      Text inserted_text = get_random_text(rand);

      if (rand() % 3) {
        // cout << "create snapshot " << snapshot_tasks.size() << "\n";

        auto snapshot = buffer.create_snapshot();
        snapshot_tasks.push_back({
          snapshot,
          buffer.base_text(),
          mutated_text,
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
                snapshot->base_text(),
                snapshot->text(),
                snapshot->extent(),
                line_ending_positions
              });
            }
            return results;
          })
        });
      }

      // cout << "set_text_in_range(" << deleted_range << ", " << inserted_text << ")\n";

      mutated_text.splice(deleted_range.start, deleted_range.extent(), inserted_text);
      buffer.set_text_in_range(deleted_range, move(inserted_text));

      // cout << "extent: " << mutated_text.extent() << "\ntext: " << mutated_text << "\n";

      REQUIRE(buffer.extent() == mutated_text.extent());
      REQUIRE(buffer.text() == mutated_text);

      for (uint32_t row = 0; row < mutated_text.extent().row; row++) {
        REQUIRE(
          Point(row, *buffer.line_length_for_row(row)) ==
          Point(row, mutated_text.line_length_for_row(row))
        );
      }

      for (uint32_t k = 0; k < 5; k++) {
        Range range = get_random_range(rand, buffer);
        Text subtext{TextSlice(mutated_text).slice(range)};
        if (subtext.empty()) subtext.append(Text{u"."});
        if (rand() % 2) subtext.append(Text{u"*"});

        // cout << "search for: /" << subtext << "/\n";
        auto search_result = buffer.search(subtext.content.data(), subtext.size());

        Regex regex(subtext.content.data(), subtext.size());
        MatchResult match_result = regex.match(mutated_text.data(), mutated_text.size());
        if (match_result.type == MatchResult::Partial || match_result.type == MatchResult::Full) {
          REQUIRE(search_result.range == (Range{
            mutated_text.position_for_offset(match_result.start_offset),
            mutated_text.position_for_offset(match_result.end_offset),
          }));
        } else {
          REQUIRE(search_result.range == optional<Range>());
        }
      }

      for (uint32_t k = 0; k < 5; k++) {
        Range range = get_random_range(rand, buffer);

        // cout << "check random range " << range << "\n";

        REQUIRE(buffer.text_in_range(range) == Text(TextSlice(mutated_text).slice(range)));
        REQUIRE(buffer.position_for_offset(buffer.clip_position(range.start).offset) == range.start);
        REQUIRE(buffer.position_for_offset(buffer.clip_position(range.end).offset) == range.end);
      }

      if (rand() % 3 == 0 && !snapshot_tasks.empty()) {
        uint32_t snapshot_index = rand() % snapshot_tasks.size();

        buffer.get_inverted_changes(snapshot_tasks[snapshot_index].snapshot);
        snapshot_tasks[snapshot_index].future.wait();
        auto &base_text = snapshot_tasks[snapshot_index].base_text;
        auto &mutated_text = snapshot_tasks[snapshot_index].mutated_text;

        if (rand() % 3) {
          snapshot_tasks[snapshot_index].snapshot->flush_preceding_changes();
        }

        for (auto data : snapshot_tasks[snapshot_index].future.get()) {
          REQUIRE(data.base_text == base_text);
          REQUIRE(data.text == mutated_text);
          REQUIRE(data.extent == mutated_text.extent());
          for (auto position : data.line_end_positions) {
            REQUIRE(position == Point(position.row, mutated_text.line_length_for_row(position.row)));
          }
        }

        // cout << "delete snapshot " << snapshot_index << "\n";
        delete snapshot_tasks[snapshot_index].snapshot;
        snapshot_tasks.erase(snapshot_tasks.begin() + snapshot_index);
      }
    }

    for (auto &task : snapshot_tasks) {
      task.future.wait();
      delete task.snapshot;
    }

    REQUIRE(buffer.layer_count() <= 2);
    buffer.flush_changes();
    REQUIRE(buffer.layer_count() == 1);
  }
}
