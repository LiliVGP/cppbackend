#include "htmldecode.h"

#include <string>
#include <string_view>
#include <unordered_map>

std::string HtmlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    static const std::unordered_map<std::string, char> entities = {
        {"lt", '<'},   {"LT", '<'},
        {"gt", '>'},   {"GT", '>'},
        {"amp", '&'},  {"AMP", '&'},
        {"apos", '\''},{"APOS", '\''},
        {"quot", '"'}, {"QUOT", '"'}
    };

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '&') {
            if (i + 1 < str.size()) {
                size_t j = i + 1;
                while (j < str.size() && str[j] != ';' && str[j] != '&') {
                    ++j;
                }

                std::string_view maybe_entity = str.substr(i + 1, j - i - 1);

                bool has_semicolon = (j < str.size() && str[j] == ';');

                auto it = entities.find(std::string(maybe_entity));
                if (it != entities.end()) {
                    result.push_back(it->second);
                    i = has_semicolon ? j : j - 1;
                    continue;
                }
            }
            result.push_back('&');
        }
        else {
            result.push_back(str[i]);
        }
    }

    return result;
}