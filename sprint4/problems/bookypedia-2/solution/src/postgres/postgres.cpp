#include "postgres.h"

#include <pqxx/zview.hxx>
#include <pqxx/result.hxx>
#include <sstream>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    try {
        pqxx::work work{connection_};
        work.exec_params(
            R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=EXCLUDED.name;
)"_zv,
            author.GetId().ToString(), author.GetName());
        work.commit();
    } catch (const std::exception& e) {
        // Log error
        throw;
    }
}

void AuthorRepositoryImpl::Delete(const std::string& author_id) {
    try {
        pqxx::work work{connection_};
        
        // Удаляем теги книг автора
        work.exec_params(
            "DELETE FROM book_tags WHERE book_id IN (SELECT id FROM books WHERE author_id = $1)",
            author_id);
        
        // Удаляем книги автора
        work.exec_params("DELETE FROM books WHERE author_id = $1", author_id);
        
        // Удаляем автора
        work.exec_params("DELETE FROM authors WHERE id = $1", author_id);
        
        work.commit();
    } catch (const std::exception& e) {
        // Log error
        throw;
    }
}

void AuthorRepositoryImpl::UpdateName(const std::string& author_id, const std::string& new_name) {
    try {
        pqxx::work work{connection_};
        work.exec_params("UPDATE authors SET name = $1 WHERE id = $2", new_name, author_id);
        work.commit();
    } catch (const std::exception& e) {
        // Log error
        throw;
    }
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    try {
        pqxx::work work{connection_};
        work.exec_params(
            R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET author_id=EXCLUDED.author_id, title=EXCLUDED.title, publication_year=EXCLUDED.publication_year;
)"_zv,
            book.GetId().ToString(),
            book.GetAuthorId().ToString(),
            book.GetTitle(),
            book.GetPublicationYear());
        work.commit();
    } catch (const std::exception& e) {
        // Log error
        throw;
    }
}

void BookRepositoryImpl::Delete(const std::string& book_id) {
    try {
        pqxx::work work{connection_};
        // Сначала удаляем теги
        work.exec_params("DELETE FROM book_tags WHERE book_id = $1", book_id);
        // Потом книгу
        work.exec_params("DELETE FROM books WHERE id = $1", book_id);
        work.commit();
    } catch (const std::exception& e) {
        // Log error
        throw;
    }
}

void BookRepositoryImpl::DeleteAuthorBooks(const std::string& author_id) {
    try {
        pqxx::work work{connection_};
        work.exec_params("DELETE FROM books WHERE author_id = $1", author_id);
        work.commit();
    } catch (const std::exception& e) {
        // Log error
        throw;
    }
}

void TagRepositoryImpl::Save(const std::string& book_id, const std::string& tag) {
    try {
        pqxx::work work{connection_};
        work.exec_params(
            R"(
INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)
ON CONFLICT (book_id, tag) DO NOTHING;
)"_zv,
            book_id, tag);
        work.commit();
    } catch (const std::exception& e) {
        // Log error
        throw;
    }
}

void TagRepositoryImpl::DeleteBookTags(const std::string& book_id) {
    try {
        pqxx::work work{connection_};
        work.exec_params("DELETE FROM book_tags WHERE book_id = $1", book_id);
        work.commit();
    } catch (const std::exception& e) {
        // Log error
        throw;
    }
}

std::vector<std::string> TagRepositoryImpl::GetBookTags(const std::string& book_id) {
    std::vector<std::string> result;
    try {
        pqxx::work work{connection_};
        auto query = "SELECT tag FROM book_tags WHERE book_id = $1 ORDER BY tag ASC";
        auto res = work.exec_params(query, book_id);
        
        for (size_t i = 0; i < res.size(); ++i) {
            result.push_back(res[i][0].as<std::string>());
        }
    } catch (const std::exception& e) {
        // Log error
        throw;
    }
    return result;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    try {
        pqxx::work work{connection_};
        work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
        work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID PRIMARY KEY,
    author_id UUID NOT NULL,
    title varchar(100) NOT NULL,
    publication_year integer NOT NULL,
    FOREIGN KEY (author_id) REFERENCES authors(id) ON DELETE CASCADE
);
)"_zv);
        work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL,
    tag varchar(30) NOT NULL,
    PRIMARY KEY (book_id, tag),
    FOREIGN KEY (book_id) REFERENCES books(id) ON DELETE CASCADE
);
)"_zv);
        work.commit();
    } catch (const std::exception& e) {
        // Log error
        throw;
    }
}

}  // namespace postgres