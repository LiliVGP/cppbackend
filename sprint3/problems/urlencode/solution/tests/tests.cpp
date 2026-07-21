#include <gtest/gtest.h>

#include "../src/urlencode.h"

using namespace std::literals;

TEST(UrlEncodeTestSuite, EmptyString) {
    EXPECT_EQ(UrlEncode(""sv), ""s);
}

TEST(UrlEncodeTestSuite, OrdinaryCharsAreNotEncoded) {
    EXPECT_EQ(UrlEncode("hello"sv), "hello"s);
    EXPECT_EQ(UrlEncode("HELLO"sv), "HELLO"s);
    EXPECT_EQ(UrlEncode("12345"sv), "12345"s);
    EXPECT_EQ(UrlEncode("abc123"sv), "abc123"s);
}

TEST(UrlEncodeTestSuite, SafeSpecialCharsAreNotEncoded) {
    EXPECT_EQ(UrlEncode("-._~"sv), "-._~"s);
    EXPECT_EQ(UrlEncode("hello-world"sv), "hello-world"s);
    EXPECT_EQ(UrlEncode("hello.world"sv), "hello.world"s);
    EXPECT_EQ(UrlEncode("hello_world"sv), "hello_world"s);
    EXPECT_EQ(UrlEncode("hello~world"sv), "hello~world"s);
}

TEST(UrlEncodeTestSuite, ReservedCharsAreEncoded) {
    EXPECT_EQ(UrlEncode("!\"#$&'()*+,/:;=?@[]"sv),
        "%21%22%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"s);
    EXPECT_EQ(UrlEncode("!"sv), "%21"s);
    EXPECT_EQ(UrlEncode("#"sv), "%23"s);
    EXPECT_EQ(UrlEncode("$"sv), "%24"s);
    EXPECT_EQ(UrlEncode("&"sv), "%26"s);
    EXPECT_EQ(UrlEncode("'"sv), "%27"s);
    EXPECT_EQ(UrlEncode("("sv), "%28"s);
    EXPECT_EQ(UrlEncode(")"sv), "%29"s);
    EXPECT_EQ(UrlEncode("*"sv), "%2A"s);
    EXPECT_EQ(UrlEncode("+"sv), "%2B"s);
    EXPECT_EQ(UrlEncode(","sv), "%2C"s);
    EXPECT_EQ(UrlEncode("/"sv), "%2F"s);
    EXPECT_EQ(UrlEncode(":"sv), "%3A"s);
    EXPECT_EQ(UrlEncode(";"sv), "%3B"s);
    EXPECT_EQ(UrlEncode("="sv), "%3D"s);
    EXPECT_EQ(UrlEncode("?"sv), "%3F"s);
    EXPECT_EQ(UrlEncode("@"sv), "%40"s);
    EXPECT_EQ(UrlEncode("["sv), "%5B"s);
    EXPECT_EQ(UrlEncode("]"sv), "%5D"s);
}

TEST(UrlEncodeTestSuite, SpacesAreEncodedAsPlus) {
    EXPECT_EQ(UrlEncode(" "sv), "+"s);
    EXPECT_EQ(UrlEncode("Hello World"sv), "Hello+World"s);
    EXPECT_EQ(UrlEncode("Hello World!"sv), "Hello+World%21"s);
    EXPECT_EQ(UrlEncode("  "sv), "++"s);
    EXPECT_EQ(UrlEncode("a b c"sv), "a+b+c"s);
}

TEST(UrlEncodeTestSuite, ControlCharsAreEncoded) {
    EXPECT_EQ(UrlEncode("\x01"sv), "%01"s);
    EXPECT_EQ(UrlEncode("\x1F"sv), "%1F"s);
    EXPECT_EQ(UrlEncode("Hello\x07World"sv), "Hello%07World"s);
}

TEST(UrlEncodeTestSuite, HighAsciiCharsAreEncoded) {
    EXPECT_EQ(UrlEncode("\x80"sv), "%80"s);
    EXPECT_EQ(UrlEncode("\xFF"sv), "%FF"s);
    EXPECT_EQ(UrlEncode("Hello\xA9World"sv), "Hello%A9World"s);
}

TEST(UrlEncodeTestSuite, MixedCharacters) {
    EXPECT_EQ(UrlEncode("Hello World! @#$"sv), "Hello+World%21+%40%23%24"s);
    EXPECT_EQ(UrlEncode("abc*def"sv), "abc%2Adef"s);
    EXPECT_EQ(UrlEncode("hello world 123"sv), "hello+world+123"s);
    EXPECT_EQ(UrlEncode("foo@bar.com"sv), "foo%40bar.com"s);
    EXPECT_EQ(UrlEncode("тест"sv), "%D1%82%D0%B5%D1%81%D1%82"s);
}

TEST(UrlEncodeTestSuite, PercentEncodingIsUpperCase) {
    EXPECT_EQ(UrlEncode("!"sv), "%21"s);
    EXPECT_EQ(UrlEncode("#"sv), "%23"s);
    EXPECT_EQ(UrlEncode("$"sv), "%24"s);
    EXPECT_EQ(UrlEncode("&"sv), "%26"s);
    EXPECT_EQ(UrlEncode("*"sv), "%2A"s);
    EXPECT_EQ(UrlEncode("/"sv), "%2F"s);
    EXPECT_EQ(UrlEncode("?"sv), "%3F"s);
    EXPECT_EQ(UrlEncode("["sv), "%5B"s);
    EXPECT_EQ(UrlEncode("]"sv), "%5D"s);
}