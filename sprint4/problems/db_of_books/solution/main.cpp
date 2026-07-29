#include <iostream>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <optional>
#include <pqxx/pqxx>

using namespace std::literals;
using pqxx::operator"" _zv;

// Простая утилита для экранирования JSON-строк (кавычки и управляющие символы)
std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size());
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
                out += "\\u";
                char buf[5];
                snprintf(buf, sizeof(buf), "%04X", static_cast<int>(c));
                out += buf;
            }
            else {
                out += c;
            }
            break;
        }
    }
    return out;
}

// Парсинг JSON-строки вручную. Предполагаем корректный ввод.
// Возвращает пару (action, payload_string).
std::pair<std::string, std::string> parse_line(const std::string& line) {
    size_t action_pos = line.find("\"action\"");
    if (action_pos == std::string::npos) return {};

    // Ищем значение action
    size_t colon = line.find(':', action_pos);
    size_t quote1 = line.find('"', colon);
    size_t quote2 = line.find('"', quote1 + 1);
    std::string action = line.substr(quote1 + 1, quote2 - quote1 - 1);

    // Ищем payload
    size_t payload_pos = line.find("\"payload\"");
    if (payload_pos == std::string::npos) return {};
    colon = line.find(':', payload_pos);
    // Находим начало объекта payload (после пробелов и двоеточия)
    size_t start_payload = colon + 1;
    while (start_payload < line.size() && (line[start_payload] == ' ' || line[start_payload] == '\t')) {
        start_payload++;
    }
    // Находим конец payload (баланс скобок)
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
    return { action, payload };
}

// Достать строковое значение из JSON-объекта (без кавычек)
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

// Достать число из JSON
std::optional<int> get_int(const std::string& payload, const std::string& key) {
    std::string key_str = "\"" + key + "\"";
    size_t pos = payload.find(key_str);
    if (pos == std::string::npos) return std::nullopt;
    size_t colon = payload.find(':', pos);
    size_t start = colon + 1;
    while (start < payload.size() && (payload[start] == ' ' || payload[start] == '\t')) start++;
    size_t end = start;
    while (end < payload.size() && (isdigit(payload[end]) || payload[end] == '-')) end++;
    if (start == end) return std::nullopt;
    return std::stoi(payload.substr(start, end - start));
}

// Проверить, является ли значение null в JSON
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
        pqxx::connection conn{ argv[1] };

        // Подготавливаем запрос для вставки книги
        constexpr auto tag_ins_book = "ins_book"_zv;
        conn.prepare(tag_ins_book,
            "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)"_zv);

        // Создаём таблицу при первом запуске
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

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            auto [action, payload] = parse_line(line);

            if (action == "add_book") {
                // Извлекаем данные из JSON
                auto title = get_string(payload, "title");
                auto author = get_string(payload, "author");
                auto year = get_int(payload, "year");
                bool isbn_null = is_null(payload, "ISBN");
                std::string isbn;
                if (!isbn_null) {
                    auto isbn_opt = get_string(payload, "ISBN");
                    if (isbn_opt) isbn = *isbn_opt;
                }

                bool success = false;
                if (title && author && year) {
                    try {
                        pqxx::work w(conn);
                        if (isbn_null) {
                            // Вставляем NULL для ISBN
                            w.exec_prepared(tag_ins_book, *title, *author, *year, nullptr);
                        }
                        else {
                            w.exec_prepared(tag_ins_book, *title, *author, *year, isbn);
                        }
                        w.commit();
                        success = true;
                    }
                    catch (const pqxx::sql_error&) {
                        // Ошибка уникальности ISBN или другая ошибка БД
                        success = false;
                    }
                    catch (const std::exception&) {
                        success = false;
                    }
                }
                std::cout << "{\"result\":" << (success ? "true" : "false") << "}\n";

            }
            else if (action == "all_books") {
                std::vector<std::string> books_json;
                {
                    pqxx::read_transaction r(conn);
                    // Сортируем: year DESC, title ASC, author ASC, ISBN ASC
                    // NULLs в ISBN сортируются последними (по умолчанию в PG NULLs последние для ASC)
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
                // Выводим массив JSON
                std::cout << "[";
                for (size_t i = 0; i < books_json.size(); ++i) {
                    if (i > 0) std::cout << ",";
                    std::cout << books_json[i];
                }
                std::cout << "]\n";

            }
            else if (action == "exit") {
                break;
            }
            else {
                // Неизвестное действие – ничего не делаем, но не ломаемся
                std::cout << "{\"result\":false}\n";
            }
        }

    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}