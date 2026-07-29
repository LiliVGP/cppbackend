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
    };

    class UseCases {
    public:
        virtual void AddAuthor(const std::string& name) = 0;
        virtual std::vector<AuthorInfo> GetAuthors() const = 0;
        virtual void AddBook(const std::string& author_id, const std::string& title, int publication_year) = 0;
        virtual std::vector<BookInfo> GetBooks() const = 0;
        virtual std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) const = 0;

    protected:
        ~UseCases() = default;
    };

}  // namespace app