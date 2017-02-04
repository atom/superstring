#include "test-helpers.h"
#include <sstream>
#include "text.h"
#include "text-slice.h"

using std::string;
using std::stringstream;

TEST_CASE("Text::build - can build a Text from a UTF8 stream") {
  string input = "abγdefg\nhijklmnop";
  stringstream stream(input, std::ios_base::in);

  vector<size_t> progress_reports;
  Text text = Text::build(stream, input.size(), "UTF8", 3, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  REQUIRE(text == Text { u"abγdefg\nhijklmnop" });
  REQUIRE(progress_reports == vector<size_t>({3, 5, 8, 11, 14, 17, 18}));
}

TEST_CASE("Text::build - replaces invalid byte sequences in the middle of the stream with the Unicode replacement character") {
  string input = "ab" "\xc0" "\xc1" "de";
  stringstream stream(input, std::ios_base::in);

  vector<size_t> progress_reports;
  Text text = Text::build(stream, input.size(), "UTF8", 3, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  REQUIRE(text == Text { u"ab" "\ufffd" "\ufffd" "de" });
  REQUIRE(progress_reports == vector<size_t>({ 3, 6 }));

}

TEST_CASE("Text::build - replaces invalid byte sequences at the end of the stream with the Unicode replacement characters") {
  string input = "ab" "\xf0\x9f"; // incomplete 4-byte code point for '😁' at the end of the stream
  stringstream stream(input, std::ios_base::in);

  Text text = Text::build(stream, input.size(), "UTF8", 5, [&](size_t percent_done) {});
  REQUIRE(text == Text { u"ab" "\ufffd" "\ufffd" });
}

TEST_CASE("Text::build - handles characters that require two 16-bit code units") {
  string input = "ab" "\xf0\x9f" "\x98\x81" "cd"; // 'ab😁cd'
  stringstream stream(input, std::ios_base::in);

  Text text = Text::build(stream, input.size(), "UTF8", 5, [&](size_t percent_done) {});
  REQUIRE(text == Text { u"ab" "\xd83d" "\xde01" "cd" });
}

TEST_CASE("Text::build - resizes the buffer if the encoding conversion runs out of room") {
  string input = "abcdef";
  stringstream stream(input, std::ios_base::in);

  Text text = Text::build(stream, 3, "UTF8", 5, [&](size_t percent_done) {});
  REQUIRE(text == Text { u"abcdef" });
}

TEST_CASE("Text::build - handles CRLF newlines") {
  string input = "abc\r\nde\rf\r\ng\r";
  stringstream stream(input, std::ios_base::in);

  Text text = Text::build(stream, input.size(), "UTF8", 4, [&](size_t percent_done) {});
  REQUIRE(text == Text { u"abc\r\nde\rf\r\ng\r" });
}

TEST_CASE("Text::line_iterators - returns iterators at the start and end of lines") {
  Text text { u"a\nbc\r\nde\rf\r\ng\r" };

  auto line0 = text.line_iterators(0);
  REQUIRE(std::string(line0.first, line0.second) == "a");
  auto line1 = text.line_iterators(1);
  REQUIRE(std::string(line1.first, line1.second) == "bc");
  auto line2 = text.line_iterators(2);
  REQUIRE(std::string(line2.first, line2.second) == "de");
  auto line3 = text.line_iterators(3);
  REQUIRE(std::string(line3.first, line3.second) == "f");
  auto line4 = text.line_iterators(4);
  REQUIRE(std::string(line4.first, line4.second) == "g");
  auto line5 = text.line_iterators(5);
  REQUIRE(std::string(line5.first, line5.second) == "");
}

TEST_CASE("Text::split") {
  Text text {u"abc\ndef\r\nghi\rjkl"};
  TextSlice base_slice {text};

  {
    auto slices = base_slice.split({0, 2});
    REQUIRE(Text(slices.first) == Text(u"ab"));
    REQUIRE(Text(slices.second) == Text(u"c\ndef\r\nghi\rjkl"));
  }

  {
    auto slices = base_slice.split({1, 2});
    REQUIRE(Text(slices.first) == Text(u"abc\nde"));
    REQUIRE(Text(slices.second) == Text(u"f\r\nghi\rjkl"));
  }

  {
    auto slices = base_slice.split({1, 3});
    REQUIRE(Text(slices.first) == Text(u"abc\ndef"));
    REQUIRE(Text(slices.second) == Text(u"\r\nghi\rjkl"));
  }

  {
    auto slices = base_slice.split({2, 0});
    REQUIRE(Text(slices.first) == Text(u"abc\ndef\r\n"));
    REQUIRE(Text(slices.second) == Text(u"ghi\rjkl"));
  }

  {
    auto slices = base_slice.split({3, 3});
    REQUIRE(Text(slices.first) == Text(u"abc\ndef\r\nghi\rjkl"));
    REQUIRE(Text(slices.second) == Text(u""));
  }
}

TEST_CASE("Text::concat") {
  Text text {u"abc\ndef\r\nghi\rjkl"};
  TextSlice base_slice {text};

  REQUIRE(Text::concat(base_slice, base_slice) == Text(u"abc\ndef\r\nghi\rjklabc\ndef\r\nghi\rjkl"));

  {
    auto prefix = base_slice.prefix({0, 2});
    auto suffix = base_slice.suffix({2, 2});
    REQUIRE(Text::concat(prefix, suffix) == Text(u"abi\rjkl"));
  }

  {
    auto prefix = base_slice.prefix({1, 3});
    auto suffix = base_slice.suffix({2, 3});
    REQUIRE(Text::concat(prefix, suffix) == Text(u"abc\ndef\rjkl"));
  }

  {
    auto prefix = base_slice.prefix({1, 3});
    auto suffix = base_slice.suffix({3, 0});
    REQUIRE(Text::concat(prefix, suffix) == Text(u"abc\ndefjkl"));
  }
}

TEST_CASE("Text::splice") {
  Text text {u"abc\ndef\r\nghi\rjkl"};
  text.splice({1, 2}, {1, 1}, Text {u"mno\npq\r\nst"});
  REQUIRE(text == Text {u"abc\ndemno\npq\r\nsthi\rjkl"});
  text.splice({2, 1}, {2, 1}, Text {u""});
  REQUIRE(text == Text {u"abc\ndemno\npkl"});
  text.splice({1, 1}, {0, 0}, Text {u"uvw"});
  REQUIRE(text == Text {u"abc\nduvwemno\npkl"});
  text.splice(text.extent(), {0, 0}, Text {u"\nxyz\r\nabc"});
  REQUIRE(text == Text {u"abc\nduvwemno\npkl\nxyz\r\nabc"});
  text.splice({0, 0}, {0, 0}, Text {u"def\nghi"});
  REQUIRE(text == Text {u"def\nghiabc\nduvwemno\npkl\nxyz\r\nabc"});
}

TEST_CASE("Text::offset_for_position") {
  Text text {u"abc\ndefg\rhijkl\r\nmno"};

  REQUIRE(text.offset_for_position({0, 2}) == 2);
  REQUIRE(text.offset_for_position({0, 3}) == 3);
  REQUIRE(text.offset_for_position({0, 4}) == 3);
  REQUIRE(text.offset_for_position({0, 8}) == 3);

  REQUIRE(text.offset_for_position({1, 1}) == 5);
  REQUIRE(text.offset_for_position({1, 4}) == 8);
  REQUIRE(text.offset_for_position({1, 5}) == 8);

  REQUIRE(text.offset_for_position({2, 0}) == 9);
  REQUIRE(text.offset_for_position({2, 5}) == 14);
  REQUIRE(text.offset_for_position({2, 6}) == 14);
  REQUIRE(text.offset_for_position({2, 7}) == 14);

  REQUIRE(text.offset_for_position({3, 0}) == 16);
  REQUIRE(text.offset_for_position({3, 20}) == 19);
}
