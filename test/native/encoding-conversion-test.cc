#include "test-helpers.h"
#include <sstream>
#include "text.h"
#include "encoding-conversion.h"

using std::string;
using std::stringstream;
using std::vector;
using std::u16string;
using String = Text::String;

TEST_CASE("EncodingConversion::decode - can decode a UTF8 stream") {
  auto conversion = transcoding_from("UTF8");
  stringstream stream("abγdefg\nhijklmnop", std::ios_base::in);

  String string;
  vector<char> encoding_buffer(3);
  vector<size_t> progress_reports;
  conversion->decode(string, stream, encoding_buffer, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  REQUIRE(string == u"abγdefg\nhijklmnop");
  REQUIRE(progress_reports == vector<size_t>({2, 5, 8, 11, 14, 17, 18}));
}

TEST_CASE("EncodingConversion::decode - replaces invalid byte sequences in the middle of the stream with the Unicode replacement character") {
  auto conversion = transcoding_from("UTF8");
  stringstream stream("ab" "\xc0" "\xc1" "de", std::ios_base::in);

  String string;
  vector<char> encoding_buffer(3);
  vector<size_t> progress_reports;
  conversion->decode(string, stream, encoding_buffer, [&](size_t percent_done) {
    progress_reports.push_back(percent_done);
  });

  REQUIRE(string == u"ab" "\ufffd" "\ufffd" "de");
  REQUIRE(progress_reports == vector<size_t>({ 3, 6 }));
}

TEST_CASE("EncodingConversion::decode - replaces invalid byte sequences at the end of the stream with the Unicode replacement characters") {
  auto conversion = transcoding_from("UTF8");
  stringstream stream("ab" "\xf0\x9f", std::ios_base::in); // incomplete 4-byte code point for '😁' at the end of the stream

  String string;
  vector<char> encoding_buffer(5);
  conversion->decode(string, stream, encoding_buffer, [&](size_t percent_done) {});

  REQUIRE(string == u"ab" "\ufffd" "\ufffd");
}

TEST_CASE("EncodingConversion::decode - handles characters that require two 16-bit code units") {
  auto conversion = transcoding_from("UTF8");
  stringstream stream("ab" "\xf0\x9f" "\x98\x81" "cd", std::ios_base::in); // 'ab😁cd'

  String string;
  vector<char> encoding_buffer(5);
  conversion->decode(string, stream, encoding_buffer, [&](size_t percent_done) {});

  REQUIRE(string == u"ab" "\xd83d" "\xde01" "cd");
}

TEST_CASE("EncodingConversion::decode - resizes the buffer if the encoding conversion runs out of room") {
  auto conversion = transcoding_from("UTF8");
  stringstream stream("abcdef", std::ios_base::in);

  String string;
  vector<char> encoding_buffer(5);
  conversion->decode(string, stream, encoding_buffer, [&](size_t percent_done) {});

  REQUIRE(string == u"abcdef");
}

TEST_CASE("EncodingConversion::decode - handles CRLF newlines") {
  auto conversion = transcoding_from("UTF8");
  stringstream stream("abc\r\nde\rf\r\ng\r", std::ios_base::in);

  String string;
  vector<char> encoding_buffer(4);
  conversion->decode(string, stream, encoding_buffer, [&](size_t percent_done) {});

  REQUIRE(string == u"abc\r\nde\rf\r\ng\r");
}

TEST_CASE("EncodingConversion::encode - basic") {
  auto conversion = transcoding_to("UTF8");

  u16string content = u"abγdefg\nhijklmnop";
  String string(content.begin(), content.end());
  vector<char> encoding_buffer(3);

  stringstream stream;
  conversion->encode(string, 0, string.size(), stream, encoding_buffer);
  REQUIRE(stream.str() == "abγdefg\nhijklmnop");

  stringstream stream2;
  conversion->encode(string, 1, string.size(), stream2, encoding_buffer);
  REQUIRE(stream2.str() == "bγdefg\nhijklmnop");
}

TEST_CASE("EncodingConversion::encode - invalid characters") {
  auto conversion = transcoding_to("UTF8");

  u16string content = u"abc" "\xD800" "def";
  String string(content.begin(), content.end());
  vector<char> encoding_buffer(3);

  stringstream stream;
  conversion->encode(string, 0, string.size(), stream, encoding_buffer);
  REQUIRE(stream.str() == "abc" "\ufffd" "def");

  stringstream stream2;
  conversion->encode(string, 1, string.size(), stream2, encoding_buffer);
  REQUIRE(stream2.str() == "bc" "\ufffd" "def");

  stringstream stream3;
  conversion->encode(string, 2, string.size(), stream3, encoding_buffer);
  REQUIRE(stream3.str() == "c" "\ufffd" "def");
}

TEST_CASE("EncodingConversion::encode - invalid characters at the end of the slice") {
  auto conversion = transcoding_to("UTF8");

  u16string content = u"abc" "\xD800";
  String string(content.begin(), content.end());
  vector<char> encoding_buffer(3);

  stringstream stream;
  conversion->encode(string, 0, string.size(), stream, encoding_buffer);
  REQUIRE(stream.str() == "abc" "\ufffd");

  stringstream stream2;
  conversion->encode(string, 1, string.size(), stream2, encoding_buffer);
  REQUIRE(stream2.str() == "bc" "\ufffd");

  stringstream stream3;
  conversion->encode(string, 2, string.size(), stream3, encoding_buffer);
  REQUIRE(stream3.str() == "c" "\ufffd");
}
