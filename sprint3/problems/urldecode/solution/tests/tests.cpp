#define BOOST_TEST_MODULE urldecode tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

BOOST_AUTO_TEST_CASE(UrlDecode_tests) {
    using namespace std::literals;

    // 1. Пустая строка
    BOOST_TEST(UrlDecode(""sv) == ""s);

    // 2. Строка без %-последовательностей
    BOOST_TEST(UrlDecode("Hello World!"sv) == "Hello World!"s);
    BOOST_TEST(UrlDecode("abc123-_.~"sv) == "abc123-_.~"s);
    BOOST_TEST(UrlDecode("Hello+World"sv) == "Hello World"s); // плюс -> пробел

    // 3. Валидные %-последовательности (разный регистр)
    BOOST_TEST(UrlDecode("Hello%20World%21"sv) == "Hello World !"s);
    BOOST_TEST(UrlDecode("%41%42%43"sv) == "ABC"s);
    BOOST_TEST(UrlDecode("%61%62%63"sv) == "abc"s);
    BOOST_TEST(UrlDecode("%7E"sv) == "~"s); // тильда

    // 4. Невалидные %-последовательности (не шестнадцатеричные символы)
    BOOST_CHECK_THROW(UrlDecode("%G1"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%1G"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%zZ"sv), std::invalid_argument);

    // 5. Неполные %-последовательности (меньше двух символов после %)
    BOOST_CHECK_THROW(UrlDecode("%"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%1"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("abc%"sv), std::invalid_argument);

    // 6. Строка с символом + (уже проверено выше, но добавим отдельно)
    BOOST_TEST(UrlDecode("a+b+c"sv) == "a b c"s);

    // 7. Смешанный случай
    BOOST_TEST(UrlDecode("Hello%20World%21%20%2B"sv) == "Hello World ! +"s);

    // 8. Зарезервированные символы без кодирования должны оставаться как есть
    BOOST_TEST(UrlDecode("!$&'()*+,/:;=?@[]"sv) == "!$&'()*+,/:;=?@[]"s);
}