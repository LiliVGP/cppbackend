#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"
#include <pqxx/pqxx>
#include <algorithm>

namespace app {
    using namespace domain;

    void UseCasesImpl::AddAuthor(const std::string& name) {
        authors_.Save({ AuthorId::New(), name });
    }

    std::vector<AuthorInfo> UseCasesImpl::GetAuthors() const {
        std::vector<AuthorInfo> result;
        // Здесь нужно реализовать получение авторов через репозиторий
        // Но в текущей реализации репозиторий имеет только Save
        // Для полноценной работы нужно добавить GetAll() в репозиторий
        return result;
    }

    void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, int publication_year) {
        books_.Save({ BookId::New(), AuthorId::FromString(author_id), title, publication_year });
    }

    std::vector<BookInfo> UseCasesImpl::GetBooks() const {
        return {};
    }

    std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) const {
        return {};
    }

}  // namespace app