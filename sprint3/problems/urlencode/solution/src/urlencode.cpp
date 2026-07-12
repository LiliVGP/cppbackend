#include "urlencode.h"

#include <cctype>
#include <iomanip>
#include <sstream>

std::string UrlEncode(std::string_view str) {
    std::ostringstream encoded;
    encoded << std::hex << std::uppercase;

    for (char c : str) {
        // Проверяем, является ли символ безопасным (не требует кодирования)
        bool is_safe = false;

        // Буквы английского алфавита и цифры
        if (std::isalnum(static_cast<unsigned char>(c))) {
            is_safe = true;
        }
        // Специальные разрешённые символы: -._~
        else if (c == '-' || c == '.' || c == '_' || c == '~') {
            is_safe = true;
        }
        // Зарезервированные символы: !#$&'()*+,/:;=?@[]
        // Пробел кодируется как + (особый случай)
        // Все остальные символы (коды < 32 или >= 128) кодируются
        else if (c == ' ') {
            encoded << '+';
            continue;
        }

        if (is_safe) {
            encoded << c;
        }
        else {
            // Кодируем в %XX
            encoded << '%' << std::setw(2) << std::setfill('0')
                << static_cast<int>(static_cast<unsigned char>(c));
        }
    }

    return encoded.str();
}