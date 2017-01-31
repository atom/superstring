#include "test-helpers.h"
#include <sstream>
#include "text-buffer.h"

using std::string;
using std::stringstream;

TEST_CASE("TextBuffer::build - can build a TextBuffer from a UTF8 stream") {
  string input = "abγdefg\nhijklmnop";
  stringstream stream(input, std::ios_base::in);

  vector<size_t> progress_reports;
  TextBuffer buffer = TextBuffer::build(stream, input.size(), "UTF8", 3, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  REQUIRE(buffer.text() == Text {u"abγdefg\nhijklmnop"});
  REQUIRE(progress_reports == vector<size_t>({3, 5, 8, 11, 14, 17, 18}));
}
