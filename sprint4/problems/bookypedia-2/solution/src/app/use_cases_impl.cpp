#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
}

void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, int publication_year, const std::vector<std::string>& tags) {
    auto book = BookId::New();
    books_.Save({book, author_id, title, publication_year});
    tags_.SaveTags(book.ToString(), tags);
}

void UseCasesImpl::UpdateAuthor(const std::string& author_id, const std::string& new_name) {
    auto author = Author{AuthorId::FromString(author_id), new_name};
    authors_.Update(author);
}

void UseCasesImpl::DeleteAuthor(const std::string& author_id) {
    authors_.Delete(author_id);
}

void UseCasesImpl::UpdateBook(const std::string& book_id, const std::string& new_title, int new_year, const std::vector<std::string>& tags) {
    auto book = Book{BookId::FromString(book_id), "", new_title, new_year};
    books_.Update(book);
    tags_.SaveTags(book_id, tags);
}

void UseCasesImpl::DeleteBook(const std::string& book_id) {
    tags_.DeleteTags(book_id);
    books_.Delete(book_id);
}

std::vector<AuthorInfo> UseCasesImpl::GetAuthors() {
    return authors_.GetAll();
}

std::vector<AuthorInfo> UseCasesImpl::SearchAuthors(const std::string& name) {
    return authors_.SearchByName(name);
}

std::vector<BookInfo> UseCasesImpl::GetBooks() {
    return books_.GetAll();
}

std::vector<BookInfo> UseCasesImpl::GetBooksByAuthor(const std::string& author_id) {
    return books_.GetByAuthorId(author_id);
}

std::vector<BookInfo> UseCasesImpl::SearchBooks(const std::string& title) {
    return books_.SearchByTitle(title);
}

std::optional<BookInfo> UseCasesImpl::GetBookDetail(const std::string& book_id) {
    return books_.GetBookDetail(book_id);
}

std::vector<std::string> UseCasesImpl::GetBookTags(const std::string& book_id) {
    return tags_.GetTags(book_id);
}

}  // namespace app
