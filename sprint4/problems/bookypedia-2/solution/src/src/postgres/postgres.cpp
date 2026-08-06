#include "postgres.h"

#include <pqxx/pqxx>
#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<app::AuthorInfo> AuthorRepositoryImpl::GetAll() {
    pqxx::read_transaction txn{connection_};
    pqxx::result result = txn.exec(
        "SELECT id, name FROM authors ORDER BY name;"_zv
    );

    std::vector<app::AuthorInfo> authors;
    for (const auto& row : result) {
        app::AuthorInfo info;
        info.id = row["id"].as<std::string>();
        info.name = row["name"].as<std::string>();
        authors.push_back(std::move(info));
    }

    return authors;
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);
)"_zv,
        book.GetId().ToString(), book.GetAuthorId(), book.GetTitle(), book.GetPublicationYear());
    work.commit();
}

std::vector<app::BookInfo> BookRepositoryImpl::GetAll() {
    pqxx::read_transaction txn{connection_};
    pqxx::result result = txn.exec(
        "SELECT title, publication_year FROM books ORDER BY title;"_zv
    );

    std::vector<app::BookInfo> books;
    for (const auto& row : result) {
        app::BookInfo info;
        info.title = row["title"].as<std::string>();
        info.publication_year = row["publication_year"].as<int>();
        books.push_back(std::move(info));
    }

    return books;
}

std::vector<app::BookInfo> BookRepositoryImpl::GetByAuthorId(const std::string& author_id) {
    pqxx::read_transaction txn{connection_};
    pqxx::result result = txn.exec_params(
        "SELECT title, publication_year FROM books WHERE author_id = $1 "
        "ORDER BY publication_year, title;"_zv,
        author_id
    );

    std::vector<app::BookInfo> books;
    for (const auto& row : result) {
        app::BookInfo info;
        info.title = row["title"].as<std::string>();
        info.publication_year = row["publication_year"].as<int>();
        books.push_back(std::move(info));
    }

    return books;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
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
    publication_year integer,
    CONSTRAINT fk_author
        FOREIGN KEY(author_id)
        REFERENCES authors(id)
);
)"_zv);
    work.commit();
}

}  // namespace postgres