#include "htmldecode.h"

#include <string>
#include <string_view>
#include <unordered_map>

std::string HtmlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    // Словарь мнемоник: ключ - мнемоника (без &), значение - символ
    static const std::unordered_map<std::string, char> entities = {
        {"lt", '<'},   {"LT", '<'},
        {"gt", '>'},   {"GT", '>'},
        {"amp", '&'},  {"AMP", '&'},
        {"apos", '\''},{"APOS", '\''},
        {"quot", '"'}, {"QUOT", '"'}
    };

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '&') {
            // Проверяем, есть ли после & символы
            if (i + 1 < str.size()) {
                // Находим конец мнемоники (до ';' или до следующего '&')
                size_t j = i + 1;
                while (j < str.size() && str[j] != ';' && str[j] != '&') {
                    ++j;
                }

                // Мнемоника - это последовательность от i+1 до j (без ';' если есть)
                std::string_view maybe_entity = str.substr(i + 1, j - i - 1);

                // Проверяем, есть ли точка с запятой
                bool has_semicolon = (j < str.size() && str[j] == ';');

                // Ищем мнемонику в словаре
                auto it = entities.find(std::string(maybe_entity));
                if (it != entities.end()) {
                    // Нашли мнемонику - заменяем на символ
                    result.push_back(it->second);
                    // Пропускаем всю мнемонику (включая ';' если есть)
                    i = has_semicolon ? j : j - 1;
                    continue;
                }
            }
            // Если не нашли мнемонику или не хватило символов, просто добавляем '&'
            result.push_back('&');
        }
        else {
            result.push_back(str[i]);
        }
    }

    return result;
}