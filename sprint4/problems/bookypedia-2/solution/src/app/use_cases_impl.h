#pragma once
#include "../domain/author_fwd.h"
#include "../domain/book_fwd.h"
#include "../domain/tag_fwd.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books, domain::TagRepository& tags)
        : authors_{authors}
        , books_{books}
        , tags_{tags} {
    }

    void AddAuthor(const std::string& name) override;
    std::vector<AuthorInfo> GetAuthors() const override;
    std::optional<AuthorInfo> FindAuthorByName(const std::string& name) const override;
    void DeleteAuthor(const std::string& author_id) override;
    void EditAuthor(const std::string& author_id, const std::string& new_name) override;
    
    void AddBook(const std::string& author_id, const std::string& title, int publication_year) override;
    void AddBookWithTags(const std::string& author_id, const std::string& title, int publication_year, const std::vector<std::string>& tags) override;
    std::vector<BookInfo> GetBooks() const override;
    std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) const override;
    std::optional<BookDetail> GetBookDetail(const std::string& book_id) const override;
    std::vector<BookInfo> FindBooksByTitle(const std::string& title) const override;
    void DeleteBook(const std::string& book_id) override;
    void EditBook(const std::string& book_id, const std::string& new_title, int new_year, const std::vector<std::string>& tags) override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
    domain::TagRepository& tags_;
};

}  // namespace app