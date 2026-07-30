#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/tag.h"
#include <pqxx/pqxx>
#include <algorithm>

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
}

std::vector<AuthorInfo> UseCasesImpl::GetAuthors() const {
    std::vector<AuthorInfo> result;
    // Этот метод будет реализован через репозиторий с прямым SQL
    return result;
}

std::optional<AuthorInfo> UseCasesImpl::FindAuthorByName(const std::string& name) const {
    return std::nullopt;
}

void UseCasesImpl::DeleteAuthor(const std::string& author_id) {
    authors_.Delete(author_id);
}

void UseCasesImpl::EditAuthor(const std::string& author_id, const std::string& new_name) {
    authors_.UpdateName(author_id, new_name);
}

void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, int publication_year) {
    books_.Save({BookId::New(), AuthorId::FromString(author_id), title, publication_year});
}

void UseCasesImpl::AddBookWithTags(const std::string& author_id, const std::string& title, int publication_year, const std::vector<std::string>& tags) {
    auto book_id = BookId::New();
    books_.Save({book_id, AuthorId::FromString(author_id), title, publication_year});
    
    for (const auto& tag : tags) {
        tags_.Save(book_id.ToString(), tag);
    }
}

std::vector<BookInfo> UseCasesImpl::GetBooks() const {
    return {};
}

std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) const {
    return {};
}

std::optional<BookDetail> UseCasesImpl::GetBookDetail(const std::string& book_id) const {
    return std::nullopt;
}

std::vector<BookInfo> UseCasesImpl::FindBooksByTitle(const std::string& title) const {
    return {};
}

void UseCasesImpl::DeleteBook(const std::string& book_id) {
    books_.Delete(book_id);
}

void UseCasesImpl::EditBook(const std::string& book_id, const std::string& new_title, int new_year, const std::vector<std::string>& tags) {
    // Сначала удаляем старые теги
    tags_.DeleteBookTags(book_id);
    
    // Обновляем книгу
    // Здесь нужно получить текущую книгу, обновить и сохранить
    // В реальном приложении нужно добавить метод GetBookById в репозиторий
    
    // Для простоты создадим новую книгу с теми же данными, но новым названием и годом
    // В реальном приложении нужно обновлять существующую запись
}

}  // namespace app