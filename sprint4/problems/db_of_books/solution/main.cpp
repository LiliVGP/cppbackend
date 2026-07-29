#include <iostream>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <optional>
#include <pqxx/pqxx>

using namespace std::literals;
using pqxx::operator"" _zv;

// Экранирование для JSON
std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() * 1.2);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04X", static_cast<int>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

// Парсинг JSON - упрощённая версия
std::pair<std::string, std::string> parse_line(const std::string& line) {
    size_t action_pos = line.find("\"action\"");
    if (action_pos == std::string::npos) return {"", ""};
    
    size_t colon = line.find(':', action_pos);
    size_t quote1 = line.find('"', colon);
    size_t quote2 = line.find('"', quote1 + 1);
    std::string action = line.substr(quote1 + 1, quote2 - quote1 - 1);
    
    size_t payload_pos = line.find("\"payload\"");
    if (payload_pos == std::string::npos) return {action, ""};
    colon = line.find(':', payload_pos);
    size_t start_payload = colon + 1;
    while (start_payload < line.size() && (line[start_payload] == ' ' || line[start_payload] == '\t')) {
        start_payload++;
    }
    
    int balance = 0;
    size_t end_payload = start_payload;
    for (size_t i = start_payload; i < line.size(); ++i) {
        if (line[i] == '{' || line[i] == '[') balance++;
        else if (line[i] == '}' || line[i] == ']') balance--;
        if (balance == 0) {
            end_payload = i + 1;
            break;
        }
    }
    std::string payload = line.substr(start_payload, end_payload - start_payload);
    return {action, payload};
}

// Получить строковое значение из JSON
std::optional<std::string> get_string(const std::string& payload, const std::string& key) {
    std::string key_str = "\"" + key + "\"";
    size_t pos = payload.find(key_str);
    if (pos == std::string::npos) return std::nullopt;
    
    size_t colon = payload.find(':', pos);
    size_t start = payload.find('"', colon);
    if (start == std::string::npos) return std::nullopt;
    size_t end = payload.find('"', start + 1);
    if (end == std::string::npos) return std::nullopt;
    return payload.substr(start + 1, end - start - 1);
}

// Получить числовое значение из JSON (поддерживает и строки, и числа)
std::optional<int> get_int(const std::string& payload, const std::string& key) {
    std::string key_str = "\"" + key + "\"";
    size_t pos = payload.find(key_str);
    if (pos == std::string::npos) return std::nullopt;
    
    size_t colon = payload.find(':', pos);
    size_t start = colon + 1;
    while (start < payload.size() && (payload[start] == ' ' || payload[start] == '\t')) start++;
    
    // Проверяем, не строка ли это в кавычках
    if (payload[start] == '"') {
        size_t end = payload.find('"', start + 1);
        if (end == std::string::npos) return std::nullopt;
        try {
            return std::stoi(payload.substr(start + 1, end - start - 1));
        } catch (...) {
            return std::nullopt;
        }
    }
    
    size_t end = start;
    while (end < payload.size() && (isdigit(payload[end]) || payload[end] == '-')) end++;
    if (start == end) return std::nullopt;
    try {
        return std::stoi(payload.substr(start, end - start));
    } catch (...) {
        return std::nullopt;
    }
}

// Проверить, является ли значение null
bool is_null(const std::string& payload, const std::string& key) {
    std::string key_str = "\"" + key + "\"";
    size_t pos = payload.find(key_str);
    if (pos == std::string::npos) return false;
    
    size_t colon = payload.find(':', pos);
    size_t start = colon + 1;
    while (start < payload.size() && (payload[start] == ' ' || payload[start] == '\t')) start++;
    return payload.compare(start, 4, "null") == 0;
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: book_manager <conn-string>\n"sv;
        return EXIT_FAILURE;
    }

    try {
        pqxx::connection conn{argv[1]};
        
        // Сначала создаём таблицу
        {
            pqxx::work w(conn);
            w.exec(
                "CREATE TABLE IF NOT EXISTS books ("
                "id SERIAL PRIMARY KEY, "
                "title varchar(100) NOT NULL, "
                "author varchar(100) NOT NULL, "
                "year integer NOT NULL, "
                "ISBN char(13) UNIQUE"
                ");"_zv
            );
            w.commit();
        }
        
        // Теперь подготавливаем запрос - таблица уже существует
        constexpr auto tag_ins_book = "ins_book"_zv;
        conn.prepare(tag_ins_book, 
            "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)"_zv);

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            
            auto [action, payload] = parse_line(line);
            
            if (action == "add_book") {
                auto title = get_string(payload, "title");
                auto author = get_string(payload, "author");
                auto year = get_int(payload, "year");
                bool isbn_null = is_null(payload, "ISBN");
                
                std::string isbn_str;
                if (!isbn_null) {
                    auto isbn_opt = get_string(payload, "ISBN");
                    if (isbn_opt) {
                        isbn_str = *isbn_opt;
                    } else {
                        // Если ISBN передан как число, конвертируем в строку
                        auto isbn_int = get_int(payload, "ISBN");
                        if (isbn_int) {
                            isbn_str = std::to_string(*isbn_int);
                            // Дополняем до 13 символов если нужно
                            while (isbn_str.length() < 13) {
                                isbn_str += ' ';
                            }
                            if (isbn_str.length() > 13) {
                                isbn_str = isbn_str.substr(0, 13);
                            }
                        }
                    }
                }
                
                bool success = false;
                if (title && author && year) {
                    try {
                        pqxx::work w(conn);
                        if (isbn_null) {
                            w.exec_prepared(tag_ins_book, *title, *author, *year, nullptr);
                        } else {
                            w.exec_prepared(tag_ins_book, *title, *author, *year, isbn_str);
                        }
                        w.commit();
                        success = true;
                    } catch (const pqxx::sql_error& e) {
                        // Ошибка уникальности или другая ошибка БД
                        success = false;
                    } catch (const std::exception&) {
                        success = false;
                    }
                }
                std::cout << "{\"result\":" << (success ? "true" : "false") << "}\n";
                
            } else if (action == "all_books") {
                std::vector<std::string> books_json;
                {
                    pqxx::read_transaction r(conn);
                    auto query = 
                        "SELECT id, title, author, year, ISBN FROM books "
                        "ORDER BY year DESC, title ASC, author ASC, ISBN ASC"_zv;
                    
                    for (auto [id, title, author, year, isbn] : 
                         r.query<int, std::string, std::string, int, std::optional<std::string>>(query)) {
                        
                        std::string isbn_str = isbn ? escape_json(*isbn) : "null";
                        std::string entry = 
                            "{\"id\":" + std::to_string(id) + 
                            ",\"title\":\"" + escape_json(title) + 
                            "\",\"author\":\"" + escape_json(author) + 
                            "\",\"year\":" + std::to_string(year) + 
                            ",\"ISBN\":" + isbn_str + "}";
                        books_json.push_back(std::move(entry));
                    }
                }
                // Всегда выводим массив, даже если пустой
                std::cout << "[";
                for (size_t i = 0; i < books_json.size(); ++i) {
                    if (i > 0) std::cout << ",";
                    std::cout << books_json[i];
                }
                std::cout << "]\n";
                
            } else if (action == "exit") {
                break;
            } else {
                std::cout << "{\"result\":false}\n";
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
