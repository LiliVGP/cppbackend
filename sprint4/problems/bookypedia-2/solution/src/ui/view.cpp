#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <cassert>
#include <iostream>
#include <pqxx/pqxx>
#include <sstream>
#include <regex>

#include "../app/use_cases.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << " by " << book.author_name << ", " << book.publication_year;
    return out;
}

}  // namespace detail

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output,
           pqxx::connection& connection)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output}
    , connection_{connection} {
    // Добавляем только команды, специфичные для книг и авторов
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s, std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s, std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("ShowBook"s, "title"s, "Show book details"s, std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("DeleteAuthor"s, "name"s, "Delete author"s, std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "name"s, "Edit author"s, std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "title"s, "Delete book"s, std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "title"s, "Edit book"s, std::bind(&View::EditBook, this, ph::_1));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        if (name.empty()) {
            throw std::runtime_error("Empty name");
        }
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        detail::AddBookParams params;
        cmd_input >> params.publication_year;
        std::getline(cmd_input, params.title);
        boost::algorithm::trim(params.title);

        if (params.title.empty()) {
            throw std::runtime_error("Empty title");
        }

        // Запрашиваем автора
        output_ << "Enter author name or empty line to select from list:\n";
        std::string author_name;
        std::getline(input_, author_name);
        boost::algorithm::trim(author_name);

        std::string author_id;
        if (author_name.empty()) {
            // Выбираем из списка
            auto selected = SelectAuthor();
            if (!selected) {
                throw std::runtime_error("No author selected");
            }
            author_id = *selected;
        } else {
            // Ищем автора по имени
            std::string query = "SELECT id FROM authors WHERE name = $1";
            pqxx::work w{connection_};
            auto res = w.exec_params(query, author_name);
            if (res.size() > 0) {
                author_id = res[0][0].as<std::string>();
            } else {
                // Предлагаем добавить автора
                output_ << "No author found. Do you want to add " << author_name << " (y/n)?\n";
                std::string answer;
                std::getline(input_, answer);
                boost::algorithm::trim(answer);
                if (answer == "y" || answer == "Y") {
                    use_cases_.AddAuthor(author_name);
                    // Получаем id нового автора
                    pqxx::work w2{connection_};
                    auto res2 = w2.exec_params("SELECT id FROM authors WHERE name = $1", author_name);
                    if (res2.size() > 0) {
                        author_id = res2[0][0].as<std::string>();
                    } else {
                        throw std::runtime_error("Failed to add author");
                    }
                } else {
                    throw std::runtime_error("Cancelled by user");
                }
            }
        }

        // Запрашиваем теги
        output_ << "Enter tags (comma separated):\n";
        std::string tags_input;
        std::getline(input_, tags_input);
        auto tags = NormalizeTags(tags_input);

        // Добавляем книгу с тегами
        use_cases_.AddBookWithTags(author_id, params.title, params.publication_year, tags);
    } catch (const std::exception& e) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    PrintVector(output_, GetAuthors());
    return true;
}

bool View::ShowBooks() const {
    PrintVector(output_, GetBooks());
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        if (auto author_id = SelectAuthor()) {
            PrintVector(output_, GetAuthorBooks(*author_id));
        }
    } catch (const std::exception&) {
        // Ничего не выводим при ошибке
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::vector<detail::BookInfo> books;
        if (title.empty()) {
            // Показываем все книги и просим выбрать
            books = GetBooks();
            if (books.empty()) {
                return true;
            }
            output_ << "Select book:\n";
            PrintVector(output_, books);
            output_ << "Enter the book # or empty line to cancel:\n";
            std::string line;
            std::getline(input_, line);
            if (line.empty()) {
                return true;
            }
            int idx = std::stoi(line) - 1;
            if (idx < 0 || idx >= static_cast<int>(books.size())) {
                return true;
            }
            title = books[idx].title;
        }

        // Ищем книги с таким названием
        books = FindBooksByTitle(title);
        if (books.empty()) {
            return true;
        }

        int idx = 0;
        if (books.size() > 1) {
            PrintVector(output_, books);
            output_ << "Enter the book # or empty line to cancel:\n";
            std::string line;
            std::getline(input_, line);
            if (line.empty()) {
                return true;
            }
            idx = std::stoi(line) - 1;
            if (idx < 0 || idx >= static_cast<int>(books.size())) {
                return true;
            }
        }

        // Получаем детали книги
        auto detail = GetBookDetail(books[idx].id);
        if (!detail) {
            return true;
        }

        output_ << "Title: " << detail->title << "\n";
        output_ << "Author: " << detail->author_name << "\n";
        output_ << "Publication year: " << detail->publication_year << "\n";
        if (!detail->tags.empty()) {
            output_ << "Tags: ";
            for (size_t i = 0; i < detail->tags.size(); ++i) {
                if (i > 0) output_ << ", ";
                output_ << detail->tags[i];
            }
            output_ << "\n";
        }
    } catch (const std::exception&) {
        // Ничего не выводим при ошибке
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::string author_id;
        if (name.empty()) {
            // Выбираем из списка
            auto selected = SelectAuthor();
            if (!selected) {
                throw std::runtime_error("No author selected");
            }
            author_id = *selected;
        } else {
            // Ищем автора по имени
            pqxx::work w{connection_};
            auto res = w.exec_params("SELECT id FROM authors WHERE name = $1", name);
            if (res.size() > 0) {
                author_id = res[0][0].as<std::string>();
            } else {
                throw std::runtime_error("Author not found");
            }
        }

        // Удаляем автора (каскадно удалит книги и теги)
        use_cases_.DeleteAuthor(author_id);
    } catch (const std::exception&) {
        output_ << "Failed to delete author"sv << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::string author_id;
        if (name.empty()) {
            // Выбираем из списка
            auto selected = SelectAuthor();
            if (!selected) {
                throw std::runtime_error("No author selected");
            }
            author_id = *selected;
        } else {
            // Ищем автора по имени
            pqxx::work w{connection_};
            auto res = w.exec_params("SELECT id FROM authors WHERE name = $1", name);
            if (res.size() > 0) {
                author_id = res[0][0].as<std::string>();
            } else {
                throw std::runtime_error("Author not found");
            }
        }

        output_ << "Enter new name:\n";
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        if (new_name.empty()) {
            throw std::runtime_error("Empty name");
        }

        use_cases_.EditAuthor(author_id, new_name);
    } catch (const std::exception&) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::vector<detail::BookInfo> books;
        if (title.empty()) {
            // Показываем все книги и просим выбрать
            books = GetBooks();
            if (books.empty()) {
                return true;
            }
            output_ << "Select book:\n";
            PrintVector(output_, books);
            output_ << "Enter the book # or empty line to cancel:\n";
            std::string line;
            std::getline(input_, line);
            if (line.empty()) {
                return true;
            }
            int idx = std::stoi(line) - 1;
            if (idx < 0 || idx >= static_cast<int>(books.size())) {
                return true;
            }
            title = books[idx].title;
        }

        // Ищем книги с таким названием
        books = FindBooksByTitle(title);
        if (books.empty()) {
            throw std::runtime_error("Book not found");
        }

        int idx = 0;
        if (books.size() > 1) {
            PrintVector(output_, books);
            output_ << "Enter the book # or empty line to cancel:\n";
            std::string line;
            std::getline(input_, line);
            if (line.empty()) {
                return true;
            }
            idx = std::stoi(line) - 1;
            if (idx < 0 || idx >= static_cast<int>(books.size())) {
                return true;
            }
        }

        use_cases_.DeleteBook(books[idx].id);
    } catch (const std::exception&) {
        output_ << "Failed to delete book"sv << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::vector<detail::BookInfo> books;
        if (title.empty()) {
            // Показываем все книги и просим выбрать
            books = GetBooks();
            if (books.empty()) {
                return true;
            }
            output_ << "Select book:\n";
            PrintVector(output_, books);
            output_ << "Enter the book # or empty line to cancel:\n";
            std::string line;
            std::getline(input_, line);
            if (line.empty()) {
                return true;
            }
            int idx = std::stoi(line) - 1;
            if (idx < 0 || idx >= static_cast<int>(books.size())) {
                return true;
            }
            title = books[idx].title;
        }

        // Ищем книги с таким названием
        books = FindBooksByTitle(title);
        if (books.empty()) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }

        int idx = 0;
        if (books.size() > 1) {
            PrintVector(output_, books);
            output_ << "Enter the book # or empty line to cancel:\n";
            std::string line;
            std::getline(input_, line);
            if (line.empty()) {
                return true;
            }
            idx = std::stoi(line) - 1;
            if (idx < 0 || idx >= static_cast<int>(books.size())) {
                return true;
            }
        }

        auto book_id = books[idx].id;
        auto current_title = books[idx].title;
        auto current_year = books[idx].publication_year;
        
        // Получаем текущие теги
        auto detail = GetBookDetail(book_id);
        std::vector<std::string> current_tags;
        if (detail) {
            current_tags = detail->tags;
        }

        // Редактируем название
        output_ << "Enter new title or empty line to use the current one (" << current_title << "):\n";
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        if (new_title.empty()) {
            new_title = current_title;
        }

        // Редактируем год
        output_ << "Enter publication year or empty line to use the current one (" << current_year << "):\n";
        std::string year_str;
        std::getline(input_, year_str);
        boost::algorithm::trim(year_str);
        int new_year = current_year;
        if (!year_str.empty()) {
            try {
                new_year = std::stoi(year_str);
            } catch (...) {
                throw std::runtime_error("Invalid year");
            }
        }

        // Редактируем теги
        std::string tags_str;
        if (current_tags.empty()) {
            output_ << "Enter tags (comma separated):\n";
        } else {
            output_ << "Enter tags (current tags: ";
            for (size_t i = 0; i < current_tags.size(); ++i) {
                if (i > 0) output_ << ", ";
                output_ << current_tags[i];
            }
            output_ << "):\n";
        }
        std::getline(input_, tags_str);
        auto new_tags = NormalizeTags(tags_str);

        use_cases_.EditBook(book_id, new_title, new_year, new_tags);
    } catch (const std::exception& e) {
        output_ << "Failed to edit book"sv << std::endl;
    }
    return true;
}

std::optional<std::string> View::SelectAuthor() const {
    output_ << "Select author:" << std::endl;
    auto authors = GetAuthors();
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int author_idx;
    try {
        author_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid author num");
    }

    --author_idx;
    if (author_idx < 0 or author_idx >= static_cast<int>(authors.size())) {
        throw std::runtime_error("Invalid author num");
    }

    return authors[author_idx].id;
}

std::optional<std::string> View::SelectBook(const std::vector<detail::BookInfo>& books) const {
    if (books.empty()) {
        return std::nullopt;
    }
    
    if (books.size() == 1) {
        return books[0].id;
    }
    
    PrintVector(output_, books);
    output_ << "Enter the book # or empty line to cancel:\n";
    
    std::string line;
    std::getline(input_, line);
    if (line.empty()) {
        return std::nullopt;
    }
    
    int idx;
    try {
        idx = std::stoi(line) - 1;
    } catch (...) {
        return std::nullopt;
    }
    
    if (idx < 0 || idx >= static_cast<int>(books.size())) {
        return std::nullopt;
    }
    
    return books[idx].id;
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    std::vector<detail::AuthorInfo> result;
    pqxx::work w{connection_};
    auto query = "SELECT id, name FROM authors ORDER BY name ASC";
    for (auto [id, name] : w.query<std::string, std::string>(query)) {
        result.push_back({id, name});
    }
    return result;
}

std::vector<detail::BookInfo> View::GetBooks() const {
    std::vector<detail::BookInfo> result;
    pqxx::work w{connection_};
    auto query = 
        "SELECT b.id, b.title, b.publication_year, a.name, a.id as author_id "
        "FROM books b JOIN authors a ON b.author_id = a.id "
        "ORDER BY b.title ASC, a.name ASC, b.publication_year ASC";
    for (auto [id, title, year, author_name, author_id] : 
         w.query<std::string, std::string, int, std::string, std::string>(query)) {
        result.push_back({id, title, year, author_name, author_id});
    }
    return result;
}

std::vector<detail::BookInfo> View::GetAuthorBooks(const std::string& author_id) const {
    std::vector<detail::BookInfo> result;
    pqxx::work w{connection_};
    auto query = 
        "SELECT b.id, b.title, b.publication_year, a.name, a.id as author_id "
        "FROM books b JOIN authors a ON b.author_id = a.id "
        "WHERE b.author_id = $1 "
        "ORDER BY b.publication_year ASC, b.title ASC";
    auto res = w.exec_params(query, author_id);
    for (const auto& row : res) {
        std::string id = row[0].as<std::string>();
        std::string title = row[1].as<std::string>();
        int year = row[2].as<int>();
        std::string author_name = row[3].as<std::string>();
        std::string author_id2 = row[4].as<std::string>();
        result.push_back({id, title, year, author_name, author_id2});
    }
    return result;
}

std::optional<detail::BookDetail> View::GetBookDetail(const std::string& book_id) const {
    pqxx::work w{connection_};
    auto query = 
        "SELECT b.title, b.publication_year, a.name "
        "FROM books b JOIN authors a ON b.author_id = a.id "
        "WHERE b.id = $1";
    auto res = w.exec_params(query, book_id);
    if (res.empty()) {
        return std::nullopt;
    }
    
    detail::BookDetail detail;
    detail.title = res[0][0].as<std::string>();
    detail.publication_year = res[0][1].as<int>();
    detail.author_name = res[0][2].as<std::string>();
    
    // Получаем теги
    auto tags_query = "SELECT tag FROM book_tags WHERE book_id = $1 ORDER BY tag ASC";
    auto tags_res = w.exec_params(tags_query, book_id);
    for (const auto& row : tags_res) {
        detail.tags.push_back(row[0].as<std::string>());
    }
    
    return detail;
}

std::vector<detail::BookInfo> View::FindBooksByTitle(const std::string& title) const {
    std::vector<detail::BookInfo> result;
    pqxx::work w{connection_};
    auto query = 
        "SELECT b.id, b.title, b.publication_year, a.name, a.id as author_id "
        "FROM books b JOIN authors a ON b.author_id = a.id "
        "WHERE b.title = $1 "
        "ORDER BY b.title ASC, a.name ASC, b.publication_year ASC";
    auto res = w.exec_params(query, title);
    for (const auto& row : res) {
        std::string id = row[0].as<std::string>();
        std::string title2 = row[1].as<std::string>();
        int year = row[2].as<int>();
        std::string author_name = row[3].as<std::string>();
        std::string author_id = row[4].as<std::string>();
        result.push_back({id, title2, year, author_name, author_id});
    }
    return result;
}

std::vector<std::string> View::NormalizeTags(const std::string& tags_input) const {
    std::vector<std::string> result;
    if (tags_input.empty()) {
        return result;
    }
    
    std::vector<std::string> parts;
    boost::algorithm::split(parts, tags_input, boost::algorithm::is_any_of(","));
    
    for (auto& part : parts) {
        boost::algorithm::trim(part);
        if (part.empty()) {
            continue;
        }
        
        // Убираем лишние пробелы между словами
        std::regex re("\\s+");
        std::string normalized = std::regex_replace(part, re, " ");
        boost::algorithm::trim(normalized);
        
        if (!normalized.empty()) {
            result.push_back(normalized);
        }
    }
    
    // Убираем дубликаты
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    
    return result;
}

}  // namespace ui