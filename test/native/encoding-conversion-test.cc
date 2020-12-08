#include "test-helpers.h"
#include <sstream>
#include "text.h"
#include "encoding-conversion.h"

using std::string;
using std::stringstream;
using std::vector;
using std::u16string;

TEST_CASE("EncodingConversion::decode - basic UTF-8") {
  auto conversion = transcoding_from("UTF-8");
  string input(u8"abγdefg\nhijklmnop");

  u16string string;
  conversion->decode(string, input.data(), input.size());
  REQUIRE(string == u"abγdefg\nhijklmnop");

  // This first chunk ends in the middle of the multi-byte 'γ' character, so
  // decoding stops before that character.
  u16string string2;
  size_t bytes_read = conversion->decode(string2, input.data(), 3);
  REQUIRE(bytes_read == 2);

  // We can pick up where we left off and decode the reset of the input.
  conversion->decode(string2, input.data() + 2, input.size() - 2);
  REQUIRE(string2 == u"abγdefg\nhijklmnop");
}

TEST_CASE("EncodingConversion::decode - basic ISO-8859-1") {
  auto conversion = transcoding_from("ISO-8859-1");
  string input("qrst" "\xfc" "v"); // qrstüv

  u16string string;
  conversion->decode(string, input.data(), input.size());
  REQUIRE(string == u"qrstüv");
}

TEST_CASE("EncodingConversion::decode - invalid byte sequences in the middle of the input") {
  auto conversion = transcoding_from("UTF-8");
  string input("ab" "\xc0" "\xc1" "de");

  u16string string;
  conversion->decode(string, input.data(), input.size());
  REQUIRE(string == u"ab" u"\ufffd" u"\ufffd" u"de");
}

TEST_CASE("EncodingConversion::decode - invalid byte sequences at the end of the input") {
  auto conversion = transcoding_from("UTF-8");
  string input("ab" "\xf0\x9f"); // incomplete 4-byte code point for '😁' at the end of the stream

  u16string string;
  size_t bytes_encoded = conversion->decode(string, input.data(), input.size());
  REQUIRE(bytes_encoded == 2);
  REQUIRE(string == u"ab");

  // Passing the `is_end`
  string.clear();
  bytes_encoded = conversion->decode(string, input.data(), input.size(), true);
  REQUIRE(bytes_encoded == 4);
  REQUIRE(string == u"ab" u"\ufffd" u"\ufffd");
}

TEST_CASE("EncodingConversion::decode - four-byte UTF-16 characters") {
  auto conversion = transcoding_from("UTF-8");
  string input("ab" "\xf0\x9f" "\x98\x81" "cd"); // 'ab😁cd'

  u16string string;
  conversion->decode(string, input.data(), input.size());
  REQUIRE(string == u"ab" u"\xd83d"  u"\xde01" u"cd");
}

TEST_CASE("EncodingConversion::encode - basic") {
  auto conversion = transcoding_to("UTF-8");
  u16string string = u"abγdefg\nhijklmnop";

  vector<char> output(3);
  size_t bytes_encoded = 0, start = 0;

  // The 'γ' requires to UTF-8 bytes, so it doesn't fit in the output buffer
  bytes_encoded = conversion->encode(
    string, &start, string.size(), output.data(), output.size());
  REQUIRE(std::string(output.data(), bytes_encoded) == "ab");

  bytes_encoded = conversion->encode(
    string, &start, string.size(), output.data(), output.size());
  REQUIRE(std::string(output.data(), bytes_encoded) == "γd");

  bytes_encoded = conversion->encode(
    string, &start, string.size(), output.data(), output.size());
  REQUIRE(std::string(output.data(), bytes_encoded) == "efg");
}

TEST_CASE("EncodingConversion::encode - four-byte UTF-16 characters") {
  auto conversion = transcoding_to("UTF-8");
  u16string string = u"ab" u"\xd83d" u"\xde01" u"cd";  // 'ab😁cd'

  vector<char> output(10);
  size_t bytes_encoded = 0, start = 0;

  bytes_encoded = conversion->encode(
    string, &start, string.size(), output.data(), output.size());
  REQUIRE(std::string(output.data(), bytes_encoded) == "ab" "\xf0\x9f" "\x98\x81" "cd");

  // The end offset, 3, is in the middle of the 4-byte character.
  start = 0;
  bytes_encoded = conversion->encode(
    string, &start, 3, output.data(), output.size());
  REQUIRE(std::string(output.data(), bytes_encoded) == "ab");

  // We can pick up where we left off.
  bytes_encoded += conversion->encode(
    string, &start, string.size(), output.data() + bytes_encoded, output.size() - bytes_encoded);
  REQUIRE(std::string(output.data(), bytes_encoded) == "ab" "\xf0\x9f" "\x98\x81" "cd");
}

TEST_CASE("EncodingConversion::encode - invalid characters in the middle of the string") {
  auto conversion = transcoding_to("UTF-8");
  u16string string = u"abc" u"\xD800" u"def";

  vector<char> output(10);
  size_t bytes_encoded = 0, start = 0;

  bytes_encoded = conversion->encode(
    string, &start, string.size(), output.data(), output.size());
  REQUIRE(std::string(output.data(), bytes_encoded) == u8"abc" u8"\ufffd" u8"def");

  // Here, the invalid character occurs at the end of a chunk.
  start = 0;
  bytes_encoded = conversion->encode(
    string, &start, 4, output.data(), output.size());
  bytes_encoded += conversion->encode(
    string, &start, string.size(), output.data() + bytes_encoded, output.size() - bytes_encoded);
  REQUIRE(std::string(output.data(), bytes_encoded) == u8"abc" u8"\ufffd" u8"def");
}

TEST_CASE("EncodingConversion::encode - invalid characters at the end of the string") {
  auto conversion = transcoding_to("UTF-8");
  u16string string = u"abc" u"\xD800";

  vector<char> output(10);
  size_t bytes_encoded = 0, start = 0;

  bytes_encoded = conversion->encode(
    string, &start, string.size(), output.data(), output.size(), true);
  REQUIRE(std::string(output.data(), bytes_encoded) == u8"abc" u8"\ufffd");
}
