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

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual std::vector<AuthorInfo> GetAuthors() const = 0;
    virtual std::optional<AuthorInfo> FindAuthorByName(const std::string& name) const = 0;
    virtual void DeleteAuthor(const std::string& author_id) = 0;
    virtual void EditAuthor(const std::string& author_id, const std::string& new_name) = 0;
    
    virtual void AddBook(const std::string& author_id, const std::string& title, int publication_year) = 0;
    virtual void AddBookWithTags(const std::string& author_id, const std::string& title, int publication_year, const std::vector<std::string>& tags) = 0;
    virtual std::vector<BookInfo> GetBooks() const = 0;
    virtual std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) const = 0;
    virtual std::optional<BookDetail> GetBookDetail(const std::string& book_id) const = 0;
    virtual std::vector<BookInfo> FindBooksByTitle(const std::string& title) const = 0;
    virtual void DeleteBook(const std::string& book_id) = 0;
    virtual void EditBook(const std::string& book_id, const std::string& new_title, int new_year, const std::vector<std::string>& tags) = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app