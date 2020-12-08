#include "test-helpers.h"
#include <sstream>
#include "text-buffer.h"
#include "text-slice.h"
#include "regex.h"
#include <future>
#include <chrono>
#include <thread>

using std::move;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;
using std::u16string;
using MatchResult = Regex::MatchResult;
using SubsequenceMatch = TextBuffer::SubsequenceMatch;

TEST_CASE("TextBuffer::set_text_in_range - basic") {
  TextBuffer buffer{u"abc\ndef\nghi"};
  REQUIRE(buffer.text_in_range({{0, 1}, {0, UINT32_MAX}}) == u"bc");

  buffer.set_text_in_range({{0, 2}, {2, 1}}, u"jkl\nmno");
  REQUIRE(buffer.text() == u"abjkl\nmnohi");
  REQUIRE(buffer.text_in_range(Range {{0, 1}, {1, 4}}) == u"bjkl\nmnoh");

  buffer.set_text_in_range(Range {{0, 0}, {10, 1}}, u"yz");
  REQUIRE(buffer.text() == u"yz");
  REQUIRE(buffer.text_in_range(Range {{0, 1}, {10, 1}}) == u"z");
}

TEST_CASE("TextBuffer::line_length_for_row - basic") {
  TextBuffer buffer{u"a\n\nb\r\rc\r\n\r\n"};
  REQUIRE(*buffer.line_length_for_row(0) == 1);
  REQUIRE(*buffer.line_length_for_row(1) == 0);
}

TEST_CASE("TextBuffer::position_for_offset") {
  TextBuffer buffer{u"ab\ndef\r\nhijk"};
  buffer.set_text_in_range({{0, 2}, {0, 2}}, u"c");
  buffer.set_text_in_range({{1, 3}, {1, 3}}, u"g");
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
  TextBuffer buffer{u"ab\ndef"};
  buffer.set_text_in_range({{0, 2}, {0, 2}}, u"c");
  REQUIRE(buffer.text() == u"abc\ndef");
  REQUIRE(*buffer.line_length_for_row(0) == 3);

  auto snapshot1 = buffer.create_snapshot();
  buffer.set_text_in_range({{0, 3}, {0, 3}}, u"123");
  REQUIRE(buffer.text() == u"abc123\ndef");
  REQUIRE(*buffer.line_length_for_row(0) == 6);
  REQUIRE(snapshot1->text() == u"abc\ndef");
  REQUIRE(snapshot1->line_length_for_row(0) == 3);
  REQUIRE(snapshot1->line_length_for_row(1) == 3);

  auto snapshot2 = buffer.create_snapshot();
  auto snapshot3 = buffer.create_snapshot();
  buffer.set_text_in_range({{0, 6}, {0, 6}}, u"456");
  REQUIRE(buffer.text() == u"abc123456\ndef");
  REQUIRE(*buffer.line_length_for_row(0) == 9);
  REQUIRE(snapshot2->text() == u"abc123\ndef");
  REQUIRE(snapshot2->line_length_for_row(0) == 6);
  REQUIRE(snapshot2->line_length_for_row(1) == 3);
  REQUIRE(snapshot1->text() == u"abc\ndef");
  REQUIRE(snapshot1->line_length_for_row(0) == 3);
  REQUIRE(snapshot1->line_length_for_row(1) == 3);

  SECTION("deleting the latest snapshot") {
    delete snapshot2;
    delete snapshot3;
    REQUIRE(buffer.text() == u"abc123456\ndef");
    REQUIRE(*buffer.line_length_for_row(0) == 9);
    REQUIRE(snapshot1->text() == u"abc\ndef");
    REQUIRE(snapshot1->line_length_for_row(0) == 3);
    delete snapshot1;
  }

  SECTION("deleting an earlier snapshot first") {
    delete snapshot1;
    REQUIRE(buffer.text() == u"abc123456\ndef");
    REQUIRE(*buffer.line_length_for_row(0) == 9);
    REQUIRE(snapshot2->text() == u"abc123\ndef");
    REQUIRE(snapshot2->line_length_for_row(0) == 6);
    delete snapshot2;
    delete snapshot3;
  }
}

TEST_CASE("TextBuffer::chunks()") {
  TextBuffer buffer{u"abc"};
  buffer.set_text_in_range({{0, 2}, {0, 2}}, u"1");

  {
    vector<u16string> chunk_strings;
    for (auto &slice : buffer.chunks()) chunk_strings.push_back(u16string(slice.data(), slice.size()));
    REQUIRE(chunk_strings == vector<u16string>({u"ab", u"1", u"c"}));
  }

  buffer.set_text_in_range({{0, 2}, {0, 3}}, u"");
  {
    vector<u16string> chunk_strings;
    for (auto &slice : buffer.chunks()) chunk_strings.push_back(u16string(slice.data(), slice.size()));
    REQUIRE(chunk_strings == vector<u16string>({u"abc"}));
  }

  buffer.set_text_in_range({{0, 1}, {0, 2}}, u"");
  {
    vector<u16string> chunk_strings;
    for (auto &slice : buffer.chunks()) chunk_strings.push_back(u16string(slice.data(), slice.size()));
    REQUIRE(chunk_strings == vector<u16string>({u"a", u"c"}));
  }

  buffer.set_text(u"");
  {
    vector<u16string> chunk_strings;
    for (auto &slice : buffer.chunks()) chunk_strings.push_back(u16string(slice.data(), slice.size()));
    REQUIRE(chunk_strings == vector<u16string>({u""}));
  }
}

TEST_CASE("TextBuffer::get_inverted_changes") {
  TextBuffer buffer{u"ab\ndef"};
  auto snapshot1 = buffer.create_snapshot();

  // This range gets clipped. The ::get_inverted_changes method is one of the
  // few reasons it matters that we clip ranges before passing them to
  // Patch::splice.
  buffer.set_text_in_range({{0, 2}, {0, 10}}, u"c");
  buffer.flush_changes();

  auto patch = buffer.get_inverted_changes(snapshot1);
  REQUIRE(patch.get_changes() == vector<Patch::Change>({
    Patch::Change{
      Point {0, 2}, Point {0, 3},
      Point {0, 2}, Point {0, 2},
      get_text(u"c").get(),
      get_text(u"").get(),
      0, 0, 0
    },
  }));

  delete snapshot1;
}

TEST_CASE("TextBuffer::is_modified") {
  TextBuffer buffer{u"abcdef"};
  REQUIRE(!buffer.is_modified());

  buffer.set_text_in_range({{0, 1}, {0, 2}}, u"BBB");
  REQUIRE(buffer.is_modified());

  SECTION("undoing a change") {
    buffer.set_text_in_range({{0, 1}, {0, 4}}, u"b");
    REQUIRE(!buffer.is_modified());
  }

  SECTION("undoing a change after creating a snapshot") {
    auto snapshot1 = buffer.create_snapshot();
    buffer.set_text_in_range({{0, 1}, {0, 4}}, u"b");
    REQUIRE(!buffer.is_modified());
    delete snapshot1;
  }

  SECTION("undoing a change in multiple steps with snapshots in between") {
    auto snapshot1 = buffer.create_snapshot();
    buffer.set_text_in_range({{0, 3}, {0, 4}}, u"");
    REQUIRE(buffer.is_modified());

    auto snapshot2 = buffer.create_snapshot();
    buffer.set_text_in_range({{0, 2}, {0, 3}}, u"");
    REQUIRE(buffer.is_modified());

    auto snapshot3 = buffer.create_snapshot();
    buffer.set_text_in_range({{0, 1}, {0, 2}}, u"");
    REQUIRE(buffer.is_modified());

    auto snapshot4 = buffer.create_snapshot();
    buffer.set_text_in_range({{0, 1}, {0, 1}}, u"b");
    REQUIRE(buffer.text() == u"abcdef");
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

  buffer.set_text_in_range({{0, 1}, {0, 2}}, u"B");
  REQUIRE(buffer.text() == u"aBcdef");
  REQUIRE(buffer.is_modified());
  REQUIRE(buffer.layer_count() == 2);

  auto snapshot1 = buffer.create_snapshot();
  REQUIRE(buffer.layer_count() == 2);

  buffer.set_text_in_range({{0, 2}, {0, 3}}, u"C");
  REQUIRE(buffer.text() == u"aBCdef");
  REQUIRE(buffer.is_modified());
  REQUIRE(buffer.layer_count() == 3);

  auto snapshot2 = buffer.create_snapshot();
  REQUIRE(buffer.layer_count() == 3);

  buffer.flush_changes();
  REQUIRE(!buffer.is_modified());
  REQUIRE(buffer.base_text() == Text{u"aBCdef"});
  REQUIRE(buffer.text() == u"aBCdef");

  REQUIRE(snapshot1->text() == u"aBcdef");
  REQUIRE(snapshot2->text() == u"aBCdef");

  delete snapshot2;
  REQUIRE(buffer.layer_count() == 3);

  delete snapshot1;
  REQUIRE(buffer.layer_count() == 1);
}

TEST_CASE("Snapshot::flush_preceding_changes") {
  TextBuffer buffer{u"abcdef"};
  REQUIRE(buffer.layer_count() == 1);

  buffer.set_text_in_range({{0, 1}, {0, 2}}, u"B");
  REQUIRE(buffer.text() == u"aBcdef");
  REQUIRE(buffer.is_modified());
  auto snapshot1 = buffer.create_snapshot();
  REQUIRE(buffer.layer_count() == 2);

  buffer.set_text_in_range({{0, 2}, {0, 3}}, u"C");
  REQUIRE(buffer.text() == u"aBCdef");
  REQUIRE(buffer.is_modified());
  auto snapshot2 = buffer.create_snapshot();
  REQUIRE(buffer.layer_count() == 3);

  vector<uint8_t> bytes;

  SECTION("flushing the latest snapshot's changes") {
    snapshot2->flush_preceding_changes();

    REQUIRE(buffer.text() == u"aBCdef");
    REQUIRE(buffer.base_text() == Text{u"aBCdef"});
    REQUIRE(snapshot1->text() == u"aBcdef");
    REQUIRE(snapshot2->text() == u"aBCdef");
    REQUIRE(!buffer.is_modified());

    TextBuffer copy_buffer{buffer.base_text().content};
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

    REQUIRE(buffer.text() == u"aBCdef");
    REQUIRE(buffer.base_text() == Text{u"aBcdef"});
    REQUIRE(snapshot1->text() == u"aBcdef");
    REQUIRE(snapshot2->text() == u"aBCdef");
    REQUIRE(buffer.is_modified());

    TextBuffer copy_buffer{buffer.base_text().content};
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

  buffer.set_text_in_range({{0, 1}, {0, 2}}, u"B");
  auto snapshot2 = buffer.create_snapshot();

  buffer.set_text_in_range({{0, 2}, {0, 3}}, u"C");
  auto snapshot3 = buffer.create_snapshot();

  buffer.reset(Text{u"123"});

  REQUIRE(!buffer.is_modified());
  REQUIRE(buffer.text() == u"123");
  REQUIRE(snapshot1->text() == u"abcdef");
  REQUIRE(snapshot2->text() == u"aBcdef");
  REQUIRE(snapshot3->text() == u"aBCdef");

  delete snapshot1;
  delete snapshot2;
  delete snapshot3;

  REQUIRE(!buffer.is_modified());
  REQUIRE(buffer.layer_count() == 1);
  REQUIRE(buffer.text() == u"123");

  buffer.reset(Text{u"456"});
  REQUIRE(!buffer.is_modified());
  REQUIRE(buffer.layer_count() == 1);
  REQUIRE(buffer.text() == u"456");
}

TEST_CASE("TextBuffer::find") {
  TextBuffer buffer{u"abcd\nef"};

  u16string error_message;
  Regex(u"(", &error_message);
  REQUIRE(error_message == u"missing closing parenthesis");

  REQUIRE(buffer.find(Regex(u"", nullptr)) == (Range{{0, 0}, {0, 0}}));
  REQUIRE(TextBuffer().find(Regex(u"", nullptr)) == (Range{{0, 0}, {0, 0}}));

  REQUIRE(buffer.find(Regex(u"ef*", nullptr)) == (Range{{1, 0}, {1, 2}}));
  REQUIRE(buffer.find(Regex(u"x", nullptr)) == optional<Range>{});
  REQUIRE(buffer.find(Regex(u"c.", nullptr)) == (Range{{0, 2}, {0, 4}}));
  REQUIRE(buffer.find(Regex(u"d", nullptr)) == (Range{{0, 3}, {0, 4}}));
  REQUIRE(buffer.find(Regex(u"\\n", nullptr)) == (Range{{0, 4}, {1, 0}}));
  REQUIRE(buffer.find(Regex(u"\\be", nullptr)) == (Range{{1, 0}, {1, 1}}));
  REQUIRE(buffer.find(Regex(u"^e", nullptr)) == (Range{{1, 0}, {1, 1}}));
  REQUIRE(buffer.find(Regex(u"^(e|d)g?", nullptr)) == (Range{{1, 0}, {1, 1}}));

  buffer.reset(Text{u"a1b"});
  REQUIRE(buffer.find(Regex(u"\\d", nullptr)) == (Range{{0, 1}, {0, 2}}));
}

TEST_CASE("TextBuffer::find - spanning edits") {
  TextBuffer buffer{u"abcd"};
  buffer.set_text_in_range({{0, 2}, {0, 2}}, u"12345");
  buffer.set_text_in_range({{0, 9}, {0, 9}}, u"67890");

  REQUIRE(buffer.text() == u"ab12345cd67890");
  REQUIRE(buffer.find(Regex(u"b1234", nullptr)) == (Range{{0, 1}, {0, 6}}));
  REQUIRE(buffer.find(Regex(u"b12345c", nullptr)) == (Range{{0, 1}, {0, 8}}));
  REQUIRE(buffer.find(Regex(u"b12345cd6", nullptr)) == (Range{{0, 1}, {0, 10}}));
  REQUIRE(buffer.find(Regex(u"345[a-z][a-z]", nullptr)) == (Range{{0, 4}, {0, 9}}));
  REQUIRE(buffer.find(Regex(u"5cd6", nullptr)) == (Range{{0, 6}, {0, 10}}));

  buffer.reset(Text{u"abcdef"});
  buffer.set_text_in_range({{0, 2}, {0, 4}}, u"");
  REQUIRE(buffer.text() == u"abef");
  REQUIRE(buffer.find(Regex(u"abe", nullptr)) == (Range{{0, 0}, {0, 3}}));
  REQUIRE(buffer.find(Regex(u"bef", nullptr)) == (Range{{0, 1}, {0, 4}}));
  REQUIRE(buffer.find(Regex(u"bc", nullptr)) == optional<Range>{});
}

TEST_CASE("TextBuffer::find - partial matches at EOF") {
  TextBuffer buffer{u"abc\r\ndef\r\nghi\r\n"};
  REQUIRE(buffer.find(Regex(u"[^\r]\n", nullptr)) == optional<Range>());
}

TEST_CASE("TextBuffer::find_all") {
  TextBuffer buffer{u"abc\ndefg\nhijkl"};
  REQUIRE(buffer.find_all(Regex(u"\\w+", nullptr)) == vector<Range>({
    Range{Point{0, 0}, Point{0, 3}},
    Range{Point{1, 0}, Point{1, 4}},
    Range{Point{2, 0}, Point{2, 5}},
  }));

  buffer.set_text_in_range({{1, 3}, {1, 3}}, u"34");
  buffer.set_text_in_range({{1, 1}, {1, 1}}, u"12");
  REQUIRE(buffer.text() == u"abc\nd12ef34g\nhijkl");

  REQUIRE(buffer.find_all(Regex(u"\\w+", nullptr)) == vector<Range>({
    Range{Point{0, 0}, Point{0, 3}},
    Range{Point{1, 0}, Point{1, 8}},
    Range{Point{2, 0}, Point{2, 5}},
  }));

  REQUIRE(buffer.find_all(Regex(u"^\\w", nullptr)) == vector<Range>({
    Range{Point{0, 0}, Point{0, 1}},
    Range{Point{1, 0}, Point{1, 1}},
    Range{Point{2, 0}, Point{2, 1}},
  }));
}

TEST_CASE("TextBuffer::find_all - empty matches") {
  TextBuffer buffer{u"aab\nab\nb\n"};
  REQUIRE(buffer.find_all(Regex(u"^a*", nullptr)) == vector<Range>({
    Range{Point{0, 0}, Point{0, 2}},
    Range{Point{1, 0}, Point{1, 1}},
    Range{Point{2, 0}, Point{2, 0}},
    Range{Point{3, 0}, Point{3, 0}},
  }));

  REQUIRE(buffer.find_all(Regex(u"^a*", nullptr), {{1, 0}, {2, 0}}) == vector<Range>({
    Range{Point{1, 0}, Point{1, 1}},
    Range{Point{2, 0}, Point{2, 0}},
  }));

  buffer.set_text(u"abc");
  REQUIRE(buffer.find_all(Regex(u"", nullptr)) == vector<Range>({
    Range{Point{0, 0}, Point{0, 0}},
    Range{Point{0, 1}, Point{0, 1}},
    Range{Point{0, 2}, Point{0, 2}},
    Range{Point{0, 3}, Point{0, 3}},
  }));
}

TEST_CASE("TextBuffer::find_words_with_subsequence_in_range") {
  {
    TextBuffer buffer{u"banana band bandana banana"};

    REQUIRE(buffer.find_words_with_subsequence_in_range(u"bna", u"", Range{Point{0, 0}, Point{0, UINT32_MAX}}) == vector<SubsequenceMatch>({
      {u"banana", {Point{0, 0}, Point{0, 20}}, {0, 2, 3}, 12},
      {u"bandana", {Point{0, 12}}, {0, 5, 6}, 7}
    }));
  }

  {
    TextBuffer buffer{u"a_b_c abc aBc"};

    REQUIRE(buffer.find_words_with_subsequence_in_range(u"abc", u"_", Range{Point{0, 0}, Point{0, UINT32_MAX}}) == vector<SubsequenceMatch>({
      {u"aBc", {Point{0, 10}}, {0, 1, 2}, 29},
      {u"a_b_c", {Point{0, 0}}, {0, 2, 4}, 26},
      {u"abc", {Point{0, 6}}, {0, 1, 2}, 20},
    }));
  }

  {
    TextBuffer buffer{u"abc Abc"};

    REQUIRE(buffer.find_words_with_subsequence_in_range(u"Abc", u"", Range{Point{0, 0}, Point{0, UINT32_MAX}}) == vector<SubsequenceMatch>({
      {u"Abc", {Point{0, 4}}, {0, 1, 2}, 20},
      {u"abc", {Point{0, 0}}, {0, 1, 2}, 19}
    }));
  }

  {
    // Does not match words longer than 80 characters
    TextBuffer buffer{u"eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIi4uLy4uL2xpYi9jb252ZXJ0LmpzIl0sIm5hbWVzIjpbImxzi"};

    REQUIRE(buffer.find_words_with_subsequence_in_range(u"eyJ", u"", Range{Point{0, 0}, Point{0, UINT32_MAX}}) == vector<SubsequenceMatch>({
    }));
  }
}

TEST_CASE("TextBuffer::has_astral") {
  REQUIRE(TextBuffer{u"ab" u"\xd83d" u"\xde01" u"cd"}.has_astral());
  REQUIRE(!TextBuffer{u"abcd"}.has_astral());
}

struct SnapshotData {
  Text base_text;
  u16string text;
  Point extent;
  vector<Point> line_end_positions;
};

struct SnapshotTask {
  TextBuffer::Snapshot *snapshot;
  Text base_text;
  Text mutated_text;
  std::future<vector<SnapshotData>> future;
};

void query_random_ranges(TextBuffer &buffer, Generator &rand, Text &mutated_text) {
  for (uint32_t k = 0; k < 5; k++) {
    Range range = get_random_range(rand, buffer);
    // cout << "query random range " << range << "\n";

    TextSlice slice = TextSlice(mutated_text).slice(range);
    u16string expected_text{slice.begin(), slice.end()};
    REQUIRE(buffer.text_in_range(range) == expected_text);
    REQUIRE(buffer.position_for_offset(buffer.clip_position(range.start).offset) == range.start);
    REQUIRE(buffer.position_for_offset(buffer.clip_position(range.end).offset) == range.end);
  }
}

TEST_CASE("TextBuffer - random edits and queries") {
  TextBuffer::MAX_CHUNK_SIZE_TO_COPY = 2;

  auto t = time(nullptr);
  for (uint i = 0; i < 100; i++) {
    uint32_t seed = t * 1000 + i;
    Generator rand(seed);
    cout << "seed: " << seed << "\n";

    Text original_text = get_random_text(rand);
    TextBuffer buffer{original_text.content};
    vector<SnapshotTask> snapshot_tasks;
    Text mutated_text(original_text);

    query_random_ranges(buffer, rand, mutated_text);

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
              std::this_thread::sleep_for(std::chrono::microseconds(rand() % 1000));
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

      query_random_ranges(buffer, rand, mutated_text);

      // cout << "set_text_in_range(" << deleted_range << ", " << inserted_text << ")\n";
      mutated_text.splice(deleted_range.start, deleted_range.extent(), inserted_text);
      buffer.set_text_in_range(deleted_range, move(inserted_text.content));

      // cout << "extent: " << mutated_text.extent() << "\ntext: " << mutated_text << "\n";
      REQUIRE(buffer.extent() == mutated_text.extent());
      REQUIRE(buffer.text() == mutated_text.content);

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

        // cout << "find for: /" << subtext << "/\n";
        Regex regex(subtext.content.c_str(), subtext.size(), nullptr);
        Regex::MatchData match_data(regex);

        auto search_result = buffer.find(regex);

        MatchResult match_result = regex.match(mutated_text.data(), mutated_text.size(), match_data);
        if (match_result.type == MatchResult::Partial || match_result.type == MatchResult::Full) {
          REQUIRE(search_result == (Range{
            mutated_text.position_for_offset(match_result.start_offset),
            mutated_text.position_for_offset(match_result.end_offset),
          }));
        } else {
          REQUIRE(search_result == optional<Range>());
        }
      }

      query_random_ranges(buffer, rand, mutated_text);

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
          REQUIRE(data.text == mutated_text.content);
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
