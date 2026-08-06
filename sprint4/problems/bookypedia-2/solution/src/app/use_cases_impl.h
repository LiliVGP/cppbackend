#pragma once
#include "../domain/author_fwd.h"
#include "../domain/book.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books, domain::BookTagRepository& tags)
        : authors_{authors}
        , books_{books}
        , tags_{tags} {
    }

    void AddAuthor(const std::string& name) override;
    void AddBook(const std::string& author_id, const std::string& title, int publication_year, const std::vector<std::string>& tags) override;
    void UpdateAuthor(const std::string& author_id, const std::string& new_name) override;
    void DeleteAuthor(const std::string& author_id) override;
    void UpdateBook(const std::string& book_id, const std::string& new_title, int new_year, const std::vector<std::string>& tags) override;
    void DeleteBook(const std::string& book_id) override;
    std::vector<AuthorInfo> GetAuthors() override;
    std::vector<AuthorInfo> SearchAuthors(const std::string& name) override;
    std::vector<BookInfo> GetBooks() override;
    std::vector<BookInfo> GetBooksByAuthor(const std::string& author_id) override;
    std::vector<BookInfo> SearchBooks(const std::string& title) override;
    std::optional<BookInfo> GetBookDetail(const std::string& book_id) override;
    std::vector<std::string> GetBookTags(const std::string& book_id) override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
    domain::BookTagRepository& tags_;
};

}  // namespace app
