#include "urldecode.h"

#include <charconv>
#include <stdexcept>
#include <sstream>

std::string UrlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];

        if (c == '+') {
            result.push_back(' ');
        }
        else if (c == '%') {
            if (i + 2 >= str.size()) {
                throw std::invalid_argument("Incomplete percent-encoding");
            }

            char hex[3] = { str[i + 1], str[i + 2], '\0' };
            unsigned int value;
            auto [ptr, ec] = std::from_chars(hex, hex + 2, value, 16);

            if (ec != std::errc() || ptr != hex + 2) {
                throw std::invalid_argument("Invalid percent-encoding");
            }

            result.push_back(static_cast<char>(value));
            i += 2;
        }
        else {
            result.push_back(c);
        }
    }

    return result;
}