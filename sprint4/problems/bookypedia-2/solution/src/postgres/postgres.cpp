#include "postgres.h"

#include <pqxx/pqxx>
#include <pqxx/zview.hxx>
#include <algorithm>
#include <cctype>

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

void AuthorRepositoryImpl::Update(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        "UPDATE authors SET name = $1 WHERE id = $2"_zv,
        author.GetName(), author.GetId().ToString());
    work.commit();
}

void AuthorRepositoryImpl::Delete(const std::string& author_id) {
    pqxx::work work{connection_};
    // Delete tags of books belonging to this author
    work.exec_params(
        "DELETE FROM book_tags WHERE book_id IN (SELECT id FROM books WHERE author_id = $1)"_zv,
        author_id);
    // Delete books belonging to this author
    work.exec_params(
        "DELETE FROM books WHERE author_id = $1"_zv,
        author_id);
    // Delete the author
    work.exec_params(
        "DELETE FROM authors WHERE id = $1"_zv,
        author_id);
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

std::vector<app::AuthorInfo> AuthorRepositoryImpl::SearchByName(const std::string& name) {
    pqxx::read_transaction txn{connection_};
    pqxx::result result = txn.exec_params(
        "SELECT id, name FROM authors WHERE name = $1 ORDER BY name;"_zv,
        name);

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

void BookRepositoryImpl::Update(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        "UPDATE books SET title = $1, publication_year = $2 WHERE id = $3"_zv,
        book.GetTitle(), book.GetPublicationYear(), book.GetId().ToString());
    work.commit();
}

void BookRepositoryImpl::Delete(const std::string& book_id) {
    pqxx::work work{connection_};
    work.exec_params(
        "DELETE FROM books WHERE id = $1"_zv,
        book_id);
    work.commit();
}

std::vector<app::BookInfo> BookRepositoryImpl::GetAll() {
    pqxx::read_transaction txn{connection_};
    pqxx::result result = txn.exec(
        "SELECT b.id, b.title, b.publication_year, a.name as author_name "
        "FROM books b JOIN authors a ON b.author_id = a.id "
        "ORDER BY b.title, a.name, b.publication_year;"_zv
    );

    std::vector<app::BookInfo> books;
    for (const auto& row : result) {
        app::BookInfo info;
        info.id = row["id"].as<std::string>();
        info.title = row["title"].as<std::string>();
        info.publication_year = row["publication_year"].as<int>();
        info.author_name = row["author_name"].as<std::string>();
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
        info.author_name = "";
        books.push_back(std::move(info));
    }

    return books;
}

std::vector<app::BookInfo> BookRepositoryImpl::SearchByTitle(const std::string& title) {
    pqxx::read_transaction txn{connection_};
    pqxx::result result = txn.exec_params(
        "SELECT b.id, b.title, b.publication_year, a.name as author_name "
        "FROM books b JOIN authors a ON b.author_id = a.id "
        "WHERE b.title ILIKE $1 "
        "ORDER BY b.title, a.name, b.publication_year;"_zv,
        "%" + title + "%");

    std::vector<app::BookInfo> books;
    for (const auto& row : result) {
        app::BookInfo info;
        info.id = row["id"].as<std::string>();
        info.title = row["title"].as<std::string>();
        info.publication_year = row["publication_year"].as<int>();
        info.author_name = row["author_name"].as<std::string>();
        books.push_back(std::move(info));
    }

    return books;
}

std::optional<app::BookInfo> BookRepositoryImpl::GetBookDetail(const std::string& book_id) {
    pqxx::read_transaction txn{connection_};
    pqxx::result result = txn.exec_params(
        "SELECT b.id, b.title, b.publication_year, a.name as author_name "
        "FROM books b JOIN authors a ON b.author_id = a.id "
        "WHERE b.id = $1;"_zv,
        book_id);

    if (result.empty()) {
        return std::nullopt;
    }

    app::BookInfo info;
    info.id = result[0]["id"].as<std::string>();
    info.title = result[0]["title"].as<std::string>();
    info.publication_year = result[0]["publication_year"].as<int>();
    info.author_name = result[0]["author_name"].as<std::string>();
    return info;
}

void BookTagRepositoryImpl::SaveTags(const std::string& book_id, const std::vector<std::string>& tags) {
    pqxx::work work{connection_};
    work.exec_params(
        "DELETE FROM book_tags WHERE book_id = $1"_zv,
        book_id);

    for (const auto& tag : tags) {
        work.exec_params(
            "INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)"_zv,
            book_id, tag);
    }

    work.commit();
}

std::vector<std::string> BookTagRepositoryImpl::GetTags(const std::string& book_id) {
    pqxx::read_transaction txn{connection_};
    pqxx::result result = txn.exec_params(
        "SELECT tag FROM book_tags WHERE book_id = $1 ORDER BY tag;"_zv,
        book_id);

    std::vector<std::string> tags;
    for (const auto& row : result) {
        tags.push_back(row["tag"].as<std::string>());
    }

    return tags;
}

void BookTagRepositoryImpl::DeleteTags(const std::string& book_id) {
    pqxx::work work{connection_};
    work.exec_params(
        "DELETE FROM book_tags WHERE book_id = $1"_zv,
        book_id);
    work.commit();
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
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL,
    tag varchar(30) NOT NULL,
    CONSTRAINT fk_book
        FOREIGN KEY(book_id)
        REFERENCES books(id)
        ON DELETE CASCADE
);
)"_zv);
    work.commit();
}

}  // namespace postgres