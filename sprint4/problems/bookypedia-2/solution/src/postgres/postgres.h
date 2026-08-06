#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Author& author) override;
    void Update(const domain::Author& author) override;
    void Delete(const std::string& author_id) override;
    std::vector<app::AuthorInfo> GetAll() override;
    std::vector<app::AuthorInfo> SearchByName(const std::string& name) override;

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Book& book) override;
    void Update(const domain::Book& book) override;
    void Delete(const std::string& book_id) override;
    std::vector<app::BookInfo> GetAll() override;
    std::vector<app::BookInfo> GetByAuthorId(const std::string& author_id) override;
    std::vector<app::BookInfo> SearchByTitle(const std::string& title) override;
    std::optional<app::BookInfo> GetBookDetail(const std::string& book_id) override;

private:
    pqxx::connection& connection_;
};

class BookTagRepositoryImpl : public domain::BookTagRepository {
public:
    explicit BookTagRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void SaveTags(const std::string& book_id, const std::vector<std::string>& tags) override;
    std::vector<std::string> GetTags(const std::string& book_id) override;
    void DeleteTags(const std::string& book_id) override;

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

    BookTagRepositoryImpl& GetBookTags() & {
        return tags_;
    }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
    BookTagRepositoryImpl tags_{connection_};
};

}  // namespace postgres