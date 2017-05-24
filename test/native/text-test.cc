#include "test-helpers.h"
#include "text.h"
#include "text-slice.h"

TEST_CASE("Text::split") {
  Text text {u"abc\ndef\r\nghi"};
  TextSlice base_slice {text};

  {
    auto slices = base_slice.split({0, 2});
    REQUIRE(Text(slices.first) == Text(u"ab"));
    REQUIRE(Text(slices.second) == Text(u"c\ndef\r\nghi"));
  }

  {
    auto slices = base_slice.split({1, 2});
    REQUIRE(Text(slices.first) == Text(u"abc\nde"));
    REQUIRE(Text(slices.second) == Text(u"f\r\nghi"));
  }

  {
    auto slices = base_slice.split({1, 3});
    REQUIRE(Text(slices.first) == Text(u"abc\ndef"));
    REQUIRE(Text(slices.second) == Text(u"\r\nghi"));
  }

  {
    auto slices = base_slice.split({2, 0});
    REQUIRE(Text(slices.first) == Text(u"abc\ndef\r\n"));
    REQUIRE(Text(slices.second) == Text(u"ghi"));
  }

  {
    auto slices = base_slice.split({2, 3});
    REQUIRE(Text(slices.first) == Text(u"abc\ndef\r\nghi"));
    REQUIRE(Text(slices.second) == Text(u""));
  }
}

TEST_CASE("Text::concat") {
  Text text {u"abc\ndef\r\nghi"};
  TextSlice base_slice {text};

  REQUIRE(Text::concat(base_slice, base_slice) == Text(u"abc\ndef\r\nghiabc\ndef\r\nghi"));

  {
    auto prefix = base_slice.prefix({0, 2});
    auto suffix = base_slice.suffix({2, 2});
    REQUIRE(Text::concat(prefix, suffix) == Text(u"abi"));
  }

  {
    auto prefix = base_slice.prefix({1, 3});
    auto suffix = base_slice.suffix({2, 2});
    REQUIRE(Text::concat(prefix, suffix) == Text(u"abc\ndefi"));
  }

  {
    auto prefix = base_slice.prefix({1, 3});
    auto suffix = base_slice.suffix({2, 3});
    REQUIRE(Text::concat(prefix, suffix) == Text(u"abc\ndef"));
  }
}

TEST_CASE("Text::splice") {
  Text text {u"abc\ndef\r\nghi\njkl"};
  text.splice({1, 2}, {1, 1}, Text {u"mno\npq\r\nst"});
  REQUIRE(text == Text {u"abc\ndemno\npq\r\nsthi\njkl"});
  text.splice({2, 1}, {2, 1}, Text {u""});
  REQUIRE(text == Text {u"abc\ndemno\npkl"});
  text.splice({1, 1}, {0, 0}, Text {u"uvw"});
  REQUIRE(text == Text {u"abc\nduvwemno\npkl"});
  text.splice(text.extent(), {0, 0}, Text {u"\nxyz\r\nabc"});
  REQUIRE(text == Text {u"abc\nduvwemno\npkl\nxyz\r\nabc"});
  text.splice({0, 0}, {0, 0}, Text {u"def\nghi"});
  REQUIRE(text == Text {u"def\nghiabc\nduvwemno\npkl\nxyz\r\nabc"});
}

TEST_CASE("Text::offset_for_position - basic") {
  Text text {u"abc\ndefg\r\nhijkl"};

  REQUIRE(text.offset_for_position({0, 2}) == 2);
  REQUIRE(text.offset_for_position({0, 3}) == 3);
  REQUIRE(text.offset_for_position({0, 4}) == 3);
  REQUIRE(text.offset_for_position({0, 8}) == 3);

  REQUIRE(text.offset_for_position({1, 1}) == 5);
  REQUIRE(text.offset_for_position({1, 4}) == 8);
  REQUIRE(text.offset_for_position({1, 5}) == 8);
  REQUIRE(text.offset_for_position({1, 8}) == 8);

  REQUIRE(text.offset_for_position({2, 0}) == 10);
  REQUIRE(text.offset_for_position({2, 1}) == 11);
  REQUIRE(text.offset_for_position({2, 5}) == 15);
  REQUIRE(text.offset_for_position({2, 6}) == 15);
}

TEST_CASE("Text::offset_for_position - empty lines") {
  Text text {u"a\n\nb\r\rc"};
  TextSlice slice(text);

  REQUIRE(text.offset_for_position({0, 1}) == 1);
  REQUIRE(text.offset_for_position({0, 2}) == 1);
  REQUIRE(text.offset_for_position({0, UINT32_MAX}) == 1);
  REQUIRE(text.offset_for_position({1, 0}) == 2);
  REQUIRE(slice.position_for_offset(1) == Point(0, 1));
  REQUIRE(text.offset_for_position({1, 1}) == 2);
  REQUIRE(text.offset_for_position({1, UINT32_MAX}) == 2);
  REQUIRE(slice.position_for_offset(2) == Point(1, 0));
}
