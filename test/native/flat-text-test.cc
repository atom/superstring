#include "test-helpers.h"
#include <sstream>
#include "flat-text.h"

using std::string;
using std::stringstream;

TEST_CASE("can build a FlatText from a UTF8 stream") {
  string input = "abγdefg\nhijklmnop";
  stringstream stream(input, std::ios_base::in);

  vector<size_t> progress_reports;
  FlatText text = FlatText::build(stream, input.size(), "UTF8", 3, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  REQUIRE(text == FlatText { u"abγdefg\nhijklmnop" });
  REQUIRE(progress_reports == vector<size_t>({3, 5, 8, 11, 14, 17, 18}));
}

TEST_CASE("replaces invalid byte sequences with the Unicode replacement character") {
  string input = "ab" "\xc0" "\xc1" "de";
  stringstream stream(input, std::ios_base::in);

  vector<size_t> progress_reports;
  FlatText text = FlatText::build(stream, input.size(), "UTF8", 3, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  REQUIRE(text == FlatText { u"ab" "\uFFFD" "\uFFFD" "de" });
  REQUIRE(progress_reports == vector<size_t>({ 3, 6 }));
}
