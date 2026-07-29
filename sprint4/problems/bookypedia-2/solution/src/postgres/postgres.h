#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"

namespace postgres {

    class AuthorRepositoryImpl : public domain::AuthorRepository {
    public:
        explicit AuthorRepositoryImpl(pqxx::connection& connection)
            : connection_{ connection } {
        }

        void Save(const domain::Author& author) override;

    private:
        pqxx::connection& connection_;
    };

    class BookRepositoryImpl : public domain::BookRepository {
    public:
        explicit BookRepositoryImpl(pqxx::connection& connection)
            : connection_{ connection } {
        }

        void Save(const domain::Book& book) override;

    private:
        pqxx::connection& connection_;
    };

    class Database {
    public:
        explicit Database(pqxx::connection connection);

        AuthorRepositoryImpl& GetAuthors()& {
            return authors_;
        }

        BookRepositoryImpl& GetBooks()& {
            return books_;
        }

        pqxx::connection& GetConnection()& {
            return connection_;
        }

    private:
        pqxx::connection connection_;
        AuthorRepositoryImpl authors_{ connection_ };
        BookRepositoryImpl books_{ connection_ };
    };

}  // namespace postgres