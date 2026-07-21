#include "urlencode.h"

#include <cctype>
#include <iomanip>
#include <sstream>

std::string UrlEncode(std::string_view str) {
    std::ostringstream encoded;
    encoded << std::hex << std::uppercase;

    for (char c : str) {
        bool is_safe = false;

        if (std::isalnum(static_cast<unsigned char>(c))) {
            is_safe = true;
        }
        else if (c == '-' || c == '.' || c == '_' || c == '~') {
            is_safe = true;
        }
        else if (c == ' ') {
            encoded << '+';
            continue;
        }

        if (is_safe) {
            encoded << c;
        }
        else {
            encoded << '%' << std::setw(2) << std::setfill('0')
                << static_cast<int>(static_cast<unsigned char>(c));
        }
    }

    return encoded.str();
}