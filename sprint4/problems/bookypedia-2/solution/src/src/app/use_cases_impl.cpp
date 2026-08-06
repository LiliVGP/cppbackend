#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
}

void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, int publication_year) {
    books_.Save({BookId::New(), author_id, title, publication_year});
}

std::vector<AuthorInfo> UseCasesImpl::GetAuthors() {
    return authors_.GetAll();
}

std::vector<BookInfo> UseCasesImpl::GetBooks() {
    return books_.GetAll();
}

std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) {
    return books_.GetByAuthorId(author_id);
}

}  // namespace app
