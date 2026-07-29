#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/tag.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Author& author) override;
    void Delete(const std::string& author_id) override;
    void UpdateName(const std::string& author_id, const std::string& new_name) override;

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Book& book) override;
    void Delete(const std::string& book_id) override;
    void DeleteAuthorBooks(const std::string& author_id) override;

private:
    pqxx::connection& connection_;
};

class TagRepositoryImpl : public domain::TagRepository {
public:
    explicit TagRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const std::string& book_id, const std::string& tag) override;
    void DeleteBookTags(const std::string& book_id) override;
    std::vector<std::string> GetBookTags(const std::string& book_id) override;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    AuthorRepositoryImpl& GetAuthors() & {
        return authors_;
    }

    BookRepositoryImpl& GetBooks() & {
        return books_;
    }

    TagRepositoryImpl& GetTags() & {
        return tags_;
    }

    pqxx::connection& GetConnection() & {
        return connection_;
    }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
    TagRepositoryImpl tags_{connection_};
};

}  // namespace postgres