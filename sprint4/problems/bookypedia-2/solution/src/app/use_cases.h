#pragma once

#include <string>
#include <vector>
#include <optional>

namespace app {

struct AuthorInfo {
    std::string id;
    std::string name;
};

struct BookInfo {
    std::string id;
    std::string title;
    int publication_year = 0;
    std::string author_name;
};

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual void AddBook(const std::string& author_id, const std::string& title, int publication_year, const std::vector<std::string>& tags) = 0;
    virtual void UpdateAuthor(const std::string& author_id, const std::string& new_name) = 0;
    virtual void DeleteAuthor(const std::string& author_id) = 0;
    virtual void UpdateBook(const std::string& book_id, const std::string& new_title, int new_year, const std::vector<std::string>& tags) = 0;
    virtual void DeleteBook(const std::string& book_id) = 0;
    virtual std::vector<AuthorInfo> GetAuthors() = 0;
    virtual std::vector<AuthorInfo> SearchAuthors(const std::string& name) = 0;
    virtual std::vector<BookInfo> GetBooks() = 0;
    virtual std::vector<BookInfo> GetBooksByAuthor(const std::string& author_id) = 0;
    virtual std::vector<BookInfo> SearchBooks(const std::string& title) = 0;
    virtual std::optional<BookInfo> GetBookDetail(const std::string& book_id) = 0;
    virtual std::vector<std::string> GetBookTags(const std::string& book_id) = 0;

protected:
    virtual ~UseCases() = default;
};

}  // namespace app
