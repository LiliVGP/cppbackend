#include <catch2/catch_test_macros.hpp>

#include "../src/htmldecode.h"

using namespace std::literals;

TEST_CASE("Text without mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode(""sv) == ""s);
    CHECK(HtmlDecode("hello"sv) == "hello"s);
    CHECK(HtmlDecode("hello world"sv) == "hello world"s);
    CHECK(HtmlDecode("12345"sv) == "12345"s);
    CHECK(HtmlDecode("Hello World!"sv) == "Hello World!"s);
}

TEST_CASE("Text with mnemonics", "[HtmlDecode]") {
    // Основные мнемоники
    CHECK(HtmlDecode("&lt;"sv) == "<"s);
    CHECK(HtmlDecode("&gt;"sv) == ">"s);
    CHECK(HtmlDecode("&amp;"sv) == "&"s);
    CHECK(HtmlDecode("&apos;"sv) == "'"s);
    CHECK(HtmlDecode("&quot;"sv) == "\""s);

    // Мнемоники без точки с запятой
    CHECK(HtmlDecode("&lt"sv) == "<"s);
    CHECK(HtmlDecode("&gt"sv) == ">"s);
    CHECK(HtmlDecode("&amp"sv) == "&"s);
    CHECK(HtmlDecode("&apos"sv) == "'"s);
    CHECK(HtmlDecode("&quot"sv) == "\""s);
}

TEST_CASE("Mnemonics in context", "[HtmlDecode]") {
    // В начале строки
    CHECK(HtmlDecode("&lt;hello"sv) == "<hello"s);
    CHECK(HtmlDecode("&lthello"sv) == "<hello"s);

    // В конце строки
    CHECK(HtmlDecode("hello&lt;"sv) == "hello<"s);
    CHECK(HtmlDecode("hello&lt"sv) == "hello<"s);

    // В середине строки
    CHECK(HtmlDecode("hello&lt;world"sv) == "hello<world"s);
    CHECK(HtmlDecode("hello&ltworld"sv) == "hello<world"s);

    // Несколько мнемоник подряд
    CHECK(HtmlDecode("&lt;&gt;&amp;"sv) == "<>&"s);
    CHECK(HtmlDecode("&lt&gt&amp"sv) == "<>&"s);
}

TEST_CASE("Uppercase mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode("&LT;"sv) == "<"s);
    CHECK(HtmlDecode("&GT;"sv) == ">"s);
    CHECK(HtmlDecode("&AMP;"sv) == "&"s);
    CHECK(HtmlDecode("&APOS;"sv) == "'"s);
    CHECK(HtmlDecode("&QUOT;"sv) == "\""s);

    // Без точки с запятой
    CHECK(HtmlDecode("&LT"sv) == "<"s);
    CHECK(HtmlDecode("&GT"sv) == ">"s);
    CHECK(HtmlDecode("&AMP"sv) == "&"s);
    CHECK(HtmlDecode("&APOS"sv) == "'"s);
    CHECK(HtmlDecode("&QUOT"sv) == "\""s);
}

TEST_CASE("Mixed case mnemonics are not decoded", "[HtmlDecode]") {
    // Смешанный регистр не должен декодироваться
    CHECK(HtmlDecode("&lT;"sv) == "&lT;"s);
    CHECK(HtmlDecode("&Gt;"sv) == "&Gt;"s);
    CHECK(HtmlDecode("&aMp;"sv) == "&aMp;"s);
    CHECK(HtmlDecode("&aPos;"sv) == "&aPos;"s);
    CHECK(HtmlDecode("&Quot;"sv) == "&Quot;"s);

    // Без точки с запятой
    CHECK(HtmlDecode("&lT"sv) == "&lT"s);
    CHECK(HtmlDecode("&Gt"sv) == "&Gt"s);
    CHECK(HtmlDecode("&aMp"sv) == "&aMp"s);
    CHECK(HtmlDecode("&aPos"sv) == "&aPos"s);
    CHECK(HtmlDecode("&Quot"sv) == "&Quot"s);
}

TEST_CASE("Incomplete mnemonics", "[HtmlDecode]") {
    // Неполные мнемоники (нет символов после &)
    CHECK(HtmlDecode("&"sv) == "&"s);
    CHECK(HtmlDecode("hello &"sv) == "hello &"s);
    CHECK(HtmlDecode("& world"sv) == "& world"s);

    // Только начало мнемоники
    CHECK(HtmlDecode("&l"sv) == "&l"s);
    CHECK(HtmlDecode("&am"sv) == "&am"s);
    CHECK(HtmlDecode("&qu"sv) == "&qu"s);
}

TEST_CASE("Unknown mnemonics are preserved", "[HtmlDecode]") {
    // Неизвестные мнемоники остаются без изменений
    CHECK(HtmlDecode("&abracadabra;"sv) == "&abracadabra;"s);
    CHECK(HtmlDecode("&abracadabra"sv) == "&abracadabra"s);
    CHECK(HtmlDecode("&unknown;"sv) == "&unknown;"s);
    CHECK(HtmlDecode("&unknown"sv) == "&unknown"s);
    CHECK(HtmlDecode("&123;"sv) == "&123;"s);
    CHECK(HtmlDecode("&123"sv) == "&123"s);
}

TEST_CASE("No double decoding", "[HtmlDecode]") {
    // Декодированные символы не участвуют в дальнейшем декодировании
    CHECK(HtmlDecode("&amp;lt;"sv) == "&lt;"s);
    CHECK(HtmlDecode("&amp;lt"sv) == "&lt"s);
    CHECK(HtmlDecode("&amp;amp;"sv) == "&amp;"s);
    CHECK(HtmlDecode("&amp;amp"sv) == "&amp"s);
}

TEST_CASE("Real world examples", "[HtmlDecode]") {
    // Примеры из задания
    CHECK(HtmlDecode("Johnson&amp;Johnson"sv) == "Johnson&Johnson"s);
    CHECK(HtmlDecode("Johnson&ampJohnson"sv) == "Johnson&Johnson"s);
    CHECK(HtmlDecode("Johnson&AMP;Johnson"sv) == "Johnson&Johnson"s);
    CHECK(HtmlDecode("Johnson&AMPJohnson"sv) == "Johnson&Johnson"s);
    CHECK(HtmlDecode("Johnson&Johnson"sv) == "Johnson&Johnson"s);

    // Пример M&M's
    CHECK(HtmlDecode("M&amp;M&APOSs"sv) == "M&M's"s);
    CHECK(HtmlDecode("M&amp;M&APOS;s"sv) == "M&M's"s);
}

TEST_CASE("Mixed content", "[HtmlDecode]") {
    // Смешанный текст с мнемониками и обычным текстом
    CHECK(HtmlDecode("Hello &lt;world&gt; &amp; &quot;test&quot;"sv) ==
        "Hello <world> & \"test\""s);
    CHECK(HtmlDecode("&lt;div&gt;Hello&lt;/div&gt;"sv) ==
        "<div>Hello</div>"s);
    CHECK(HtmlDecode("&lt;br&gt;"sv) == "<br>"s);
}