#include "test-helpers.h"
#include <sstream>
#include "flat-text.h"

using std::string;
using std::stringstream;

TEST_CASE("FlatText::line_iterators - returns iterators at the start and end of lines") {
  FlatText text { u"a\nbc\r\nde\rf\r\ng\r" };

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

TEST_CASE("FlatText::build - can build a FlatText from a UTF8 stream") {
  string input = "abγdefg\nhijklmnop";
  stringstream stream(input, std::ios_base::in);

  vector<size_t> progress_reports;
  FlatText text = FlatText::build(stream, input.size(), "UTF8", 3, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  REQUIRE(text == FlatText { u"abγdefg\nhijklmnop" });
  REQUIRE(progress_reports == vector<size_t>({3, 5, 8, 11, 14, 17, 18}));
}

TEST_CASE("FlatText::build - replaces invalid byte sequences in the middle of the stream with the Unicode replacement character") {
  string input = "ab" "\xc0" "\xc1" "de";
  stringstream stream(input, std::ios_base::in);

  vector<size_t> progress_reports;
  FlatText text = FlatText::build(stream, input.size(), "UTF8", 3, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  REQUIRE(text == FlatText { u"ab" "\ufffd" "\ufffd" "de" });
  REQUIRE(progress_reports == vector<size_t>({ 3, 6 }));

}

TEST_CASE("FlatText::build - replaces invalid byte sequences at the end of the stream with the Unicode replacement characters") {
  string input = "ab" "\xf0\x9f"; // incomplete 4-byte code point for '😁' at the end of the stream
  stringstream stream(input, std::ios_base::in);

  FlatText text = FlatText::build(stream, input.size(), "UTF8", 5, [&](size_t percent_done) {});
  REQUIRE(text == FlatText { u"ab" "\ufffd" "\ufffd" });
}

TEST_CASE("FlatText::build - handles characters that require two 16-bit code units") {
  string input = "ab" "\xf0\x9f" "\x98\x81" "cd"; // 'ab😁cd'
  stringstream stream(input, std::ios_base::in);

  FlatText text = FlatText::build(stream, input.size(), "UTF8", 5, [&](size_t percent_done) {});
  REQUIRE(text == FlatText { u"ab" "\xd83d" "\xde01" "cd" });
}

TEST_CASE("FlatText::build - resizes the buffer if the encoding conversion runs out of room") {
  string input = "abcdef";
  stringstream stream(input, std::ios_base::in);

  FlatText text = FlatText::build(stream, 3, "UTF8", 5, [&](size_t percent_done) {});
  REQUIRE(text == FlatText { u"abcdef" });
}

TEST_CASE("FlatText::build - handles CRLF newlines") {
  string input = "abc\r\nde\rf\r\ng\r";
  stringstream stream(input, std::ios_base::in);

  FlatText text = FlatText::build(stream, input.size(), "UTF8", 4, [&](size_t percent_done) {});
  REQUIRE(text == FlatText { u"abc\r\nde\rf\r\ng\r" });
}
