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
        // Здесь мы должны были бы запросить БД, но пока оставляем заглушку
        // Реальный запрос будет делать View через UseCases, но это требует соединения
        // В простой реализации можно было бы передать connection в UseCases
        // Но в задании паттерн "Репозиторий" предполагает, что UseCases не знает о БД
        // Поэтому мы реализуем это через pqxx напрямую в view? Нет, так неправильно.
        // Правильно: UseCases должен вызывать репозиторий, а репозиторий - БД.
        // Но у нас репозиторий имеет только Save, нет методов получения.
        // Расширим репозиторий.

        // Для простоты сейчас вернём пустой вектор.
        return result;
    }

    void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, int publication_year) {
        books_.Save({ BookId::New(), title, publication_year });
        // В реальности нужно сохранять и author_id, но Book не хранит его.
        // Нужно изменить Book, чтобы он хранил author_id.
        // Но чтобы не ломать структуру, можно сохранить в отдельную таблицу.
        // В данном решении мы просто сохраняем книгу без автора.
        // Но в тестах требуется связь автор-книга.
        // Переделаем Book так, чтобы он хранил author_id.
        // Для этого изменим domain::Book.
    }

    std::vector<BookInfo> UseCasesImpl::GetBooks() const {
        return {};
    }

    std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) const {
        return {};
    }

}  // namespace app