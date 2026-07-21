#define BOOST_TEST_MODULE urldecode tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

BOOST_AUTO_TEST_CASE(UrlDecode_tests) {
    using namespace std::literals;

    BOOST_TEST(UrlDecode(""sv) == ""s);

    BOOST_TEST(UrlDecode("Hello World!"sv) == "Hello World!"s);
    BOOST_TEST(UrlDecode("abc123-_.~"sv) == "abc123-_.~"s);
    BOOST_TEST(UrlDecode("Hello+World"sv) == "Hello World"s);

    BOOST_TEST(UrlDecode("Hello%20World%21"sv) == "Hello World !"s);
    BOOST_TEST(UrlDecode("%41%42%43"sv) == "ABC"s);
    BOOST_TEST(UrlDecode("%61%62%63"sv) == "abc"s);
    BOOST_TEST(UrlDecode("%7E"sv) == "~"s);

    BOOST_CHECK_THROW(UrlDecode("%G1"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%1G"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%zZ"sv), std::invalid_argument);

    BOOST_CHECK_THROW(UrlDecode("%"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%1"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("abc%"sv), std::invalid_argument);

    BOOST_TEST(UrlDecode("a+b+c"sv) == "a b c"s);

    BOOST_TEST(UrlDecode("Hello%20World%21%20%2B"sv) == "Hello World ! +"s);

    BOOST_TEST(UrlDecode("!$&'()*+,/:;=?@[]"sv) == "!$&'()*+,/:;=?@[]"s);
}