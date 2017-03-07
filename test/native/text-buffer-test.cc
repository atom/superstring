#include "test-helpers.h"
#include <sstream>
#include "text-buffer.h"
#include "text-slice.h"

using std::move;
using std::string;
using std::stringstream;

TEST_CASE("TextBuffer::build - can build a TextBuffer from a UTF8 stream") {
  string input = "abγdefg\nhijklmnop";
  stringstream stream(input, std::ios_base::in);

  vector<size_t> progress_reports;
  TextBuffer buffer = TextBuffer::build(stream, input.size(), "UTF8", 3, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  auto text = buffer.text();
  REQUIRE(buffer.text() == Text {u"abγdefg\nhijklmnop"});
  REQUIRE(progress_reports == vector<size_t>({3, 5, 8, 11, 14, 17, 18}));
}

TEST_CASE("TextBuffer::set_text_in_range - basic") {
  TextBuffer buffer {u"abc\ndef\nghi"};
  buffer.set_text_in_range(Range {{0, 2}, {2, 1}}, Text {u"jkl\nmno"});
  REQUIRE(buffer.text() == Text {u"abjkl\nmnohi"});
  REQUIRE(buffer.text_in_range(Range {{0, 1}, {1, 4}}) == Text {u"bjkl\nmnoh"});
}

TEST_CASE("TextBuffer::line_length_for_row - basic") {
  TextBuffer buffer {u"a\n\nb\r\rc\r\n\r\n"};
  REQUIRE(buffer.line_length_for_row(0) == 1);
  REQUIRE(buffer.line_length_for_row(1) == 0);
}

TEST_CASE("TextBuffer::set_text_in_range, random edits") {
  auto t = time(nullptr);
  for (uint i = 0; i < 100; i++) {
    auto seed = t + i;
    srand(seed);
    printf("Seed: %ld\n", seed);

    TextBuffer buffer;

    for (uint j = 0; j < 2; j++) {
      Text original_text = buffer.text();
      Range deleted_range = get_random_range(buffer);
      Text inserted_text = get_random_text();

      buffer.set_text_in_range(deleted_range, TextSlice {inserted_text});
      original_text.splice(deleted_range.start, deleted_range.extent(), TextSlice {inserted_text});

      REQUIRE(buffer.text() == original_text);
    }
  }
}
