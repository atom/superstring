#include "test-helpers.h"
#include <sstream>
#include "text.h"

using std::stringstream;

TEST_CASE("builds a Text from a UTF8 stream") {
  stringstream stream("abcdefg\nhijklmnop", std::ios_base::in);

  Text text(stream, "UTF8", 3);

  REQUIRE(text == Text({
    Line{u"abcdefg", LineEnding::LF},
    Line{u"hijklmnop", LineEnding::NONE}
  }));
}
