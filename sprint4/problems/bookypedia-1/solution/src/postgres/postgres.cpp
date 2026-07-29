#include "postgres.h"

#include <pqxx/zview.hxx>

namespace postgres {

    using namespace std::literals;
    using pqxx::operator"" _zv;

    void AuthorRepositoryImpl::Save(const domain::Author& author) {
        pqxx::work work{ connection_ };
        work.exec_params(
            R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
author.GetId().ToString(), author.GetName());
        work.commit();
    }

    void BookRepositoryImpl::Save(const domain::Book& book) {
        pqxx::work work{ connection_ };
        work.exec_params(
            R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET author_id=$2, title=$3, publication_year=$4;
)"_zv,
book.GetId().ToString(),
/* author_id */ "", // Эту часть мы вынесем в UseCases, т.к. Book не хранит author_id
book.GetTitle(),
book.GetPublicationYear());
        work.commit();
    }

    Database::Database(pqxx::connection connection)
        : connection_{ std::move(connection) } {
        pqxx::work work{ connection_ };
        work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
        work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL,
    title varchar(100) NOT NULL,
    publication_year integer NOT NULL
);
)"_zv);
        work.commit();
    }

}  // namespace postgres