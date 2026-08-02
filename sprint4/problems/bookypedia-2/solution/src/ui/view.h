#pragma once
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>
#include <pqxx/pqxx>

namespace menu {
class Menu;
}

namespace app {
class UseCases;
}

namespace ui {
namespace detail {

struct AddBookParams {
    std::string title;
    std::string author_id;
    int publication_year = 0;
    std::vector<std::string> tags;
};

struct AuthorInfo {
    std::string id;
    std::string name;
};

struct BookInfo {
    std::string id;
    std::string title;
    int publication_year;
    std::string author_name;
    std::string author_id;
};

struct BookDetail {
    std::string title;
    std::string author_name;
    int publication_year;
    std::vector<std::string> tags;
};

}  // namespace detail

class View {
public:
    View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output,
         pqxx::connection& connection);

private:
    bool AddAuthor(std::istream& cmd_input) const;
    bool AddBook(std::istream& cmd_input) const;
    bool ShowAuthors() const;
    bool ShowBooks() const;
    bool ShowAuthorBooks() const;
    bool ShowBook(std::istream& cmd_input) const;
    bool DeleteAuthor(std::istream& cmd_input) const;
    bool EditAuthor(std::istream& cmd_input) const;
    bool DeleteBook(std::istream& cmd_input) const;
    bool EditBook(std::istream& cmd_input) const;

    std::optional<std::string> SelectAuthor() const;
    std::vector<detail::AuthorInfo> GetAuthors() const;
    std::vector<detail::BookInfo> GetBooks() const;
    std::vector<detail::BookInfo> GetAuthorBooks(const std::string& author_id) const;
    std::optional<detail::BookDetail> GetBookDetail(const std::string& book_id) const;
    std::vector<detail::BookInfo> FindBooksByTitle(const std::string& title) const;

    std::vector<std::string> NormalizeTags(const std::string& tags_input) const;

    menu::Menu& menu_;
    app::UseCases& use_cases_;
    std::istream& input_;
    std::ostream& output_;
    pqxx::connection& connection_;
};

}  // namespace ui