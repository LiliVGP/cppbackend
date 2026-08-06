#pragma once
#include <string>
#include <vector>
#include <optional>

#include "../app/use_cases.h"
#include "../util/tagged_uuid.h"

namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
public:
    Book(BookId id, std::string author_id, std::string title, int publication_year)
        : id_(std::move(id))
        , author_id_(std::move(author_id))
        , title_(std::move(title))
        , publication_year_(publication_year) {
    }

    const BookId& GetId() const noexcept {
        return id_;
    }

    const std::string& GetAuthorId() const noexcept {
        return author_id_;
    }

    const std::string& GetTitle() const noexcept {
        return title_;
    }

    int GetPublicationYear() const noexcept {
        return publication_year_;
    }

private:
    BookId id_;
    std::string author_id_;
    std::string title_;
    int publication_year_;
};

class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual void Update(const Book& book) = 0;
    virtual void Delete(const std::string& book_id) = 0;
    virtual std::vector<app::BookInfo> GetAll() = 0;
    virtual std::vector<app::BookInfo> GetByAuthorId(const std::string& author_id) = 0;
    virtual std::vector<app::BookInfo> SearchByTitle(const std::string& title) = 0;
    virtual std::optional<app::BookInfo> GetBookDetail(const std::string& book_id) = 0;

protected:
    virtual ~BookRepository() = default;
};

class BookTagRepository {
public:
    virtual void SaveTags(const std::string& book_id, const std::vector<std::string>& tags) = 0;
    virtual std::vector<std::string> GetTags(const std::string& book_id) = 0;
    virtual void DeleteTags(const std::string& book_id) = 0;

protected:
    virtual ~BookTagRepository() = default;
};

}  // namespace domain