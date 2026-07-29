#pragma once
#include <string>

#include "../util/tagged_uuid.h"

namespace domain {

    namespace detail {
        struct BookTag {};
    }  // namespace detail

    using BookId = util::TaggedUUID<detail::BookTag>;

    class Book {
    public:
        Book(BookId id, std::string title, int publication_year)
            : id_(std::move(id))
            , title_(std::move(title))
            , publication_year_(publication_year) {
        }

        const BookId& GetId() const noexcept {
            return id_;
        }

        const std::string& GetTitle() const noexcept {
            return title_;
        }

        int GetPublicationYear() const noexcept {
            return publication_year_;
        }

    private:
        BookId id_;
        std::string title_;
        int publication_year_;
    };

    class BookRepository {
    public:
        virtual void Save(const Book& book) = 0;

    protected:
        ~BookRepository() = default;
    };

}  // namespace domain