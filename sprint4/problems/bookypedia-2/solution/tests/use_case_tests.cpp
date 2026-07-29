#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"
#include "../src/domain/tag.h"

#include <vector>
#include <string>
#include <utility>

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;
    std::vector<std::string> deleted_authors;
    std::vector<std::pair<std::string, std::string>> updated_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }
    void Delete(const std::string& author_id) override {
        deleted_authors.push_back(author_id);
    }
    void UpdateName(const std::string& author_id, const std::string& new_name) override {
        updated_authors.push_back({author_id, new_name});
    }
};

struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> saved_books;
    std::vector<std::string> deleted_books;
    std::vector<std::string> deleted_author_books;

    void Save(const domain::Book& book) override {
        saved_books.emplace_back(book);
    }
    void Delete(const std::string& book_id) override {
        deleted_books.push_back(book_id);
    }
    void DeleteAuthorBooks(const std::string& author_id) override {
        deleted_author_books.push_back(author_id);
    }
};

struct MockTagRepository : domain::TagRepository {
    // Явно используем вектор пар
    std::vector<std::pair<std::string, std::string>> saved_tags;
    std::vector<std::string> deleted_book_tags;
    std::vector<std::string> book_tags_requests;

    void Save(const std::string& book_id, const std::string& tag) override {
        saved_tags.push_back(std::make_pair(book_id, tag));
    }
    void DeleteBookTags(const std::string& book_id) override {
        deleted_book_tags.push_back(book_id);
    }
    std::vector<std::string> GetBookTags(const std::string& book_id) override {
        book_tags_requests.push_back(book_id);
        return {};
    }
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books;
    MockTagRepository tags;
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding with Tags") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{authors, books, tags};

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }

        WHEN("Adding a book with tags") {
            const auto author_id = "123";
            const auto title = "Harry Potter";
            const auto year = 1997;
            const std::vector<std::string> tags = {"fantasy", "adventure"};

            use_cases.AddBookWithTags(author_id, title, year, tags);

            THEN("book and tags are saved") {
                REQUIRE(books.saved_books.size() == 1);
                CHECK(books.saved_books.at(0).GetTitle() == title);
                CHECK(books.saved_books.at(0).GetPublicationYear() == year);

                const auto book_id = books.saved_books.at(0).GetId().ToString();

                // Проверяем количество сохранённых тегов
                REQUIRE(tags.saved_tags.size() == 2);

                // Проверяем, что оба тега привязаны к правильной книге
                bool found_fantasy = false;
                bool found_adventure = false;
                for (std::size_t i = 0; i < tags.saved_tags.size(); ++i) {
                    const auto& pair = tags.saved_tags[i];
                    const auto& stored_book_id = pair.first;
                    const auto& stored_tag = pair.second;
                    if (stored_book_id == book_id) {
                        if (stored_tag == "fantasy") found_fantasy = true;
                        if (stored_tag == "adventure") found_adventure = true;
                    }
                }
                CHECK(found_fantasy);
                CHECK(found_adventure);
            }
        }

        WHEN("Deleting an author") {
            const auto author_id = "456";
            use_cases.DeleteAuthor(author_id);

            THEN("author is deleted") {
                REQUIRE(authors.deleted_authors.size() == 1);
                CHECK(authors.deleted_authors[0] == author_id);
            }
        }

        WHEN("Editing an author") {
            const auto author_id = "789";
            const auto new_name = "J.K. Rowling";
            use_cases.EditAuthor(author_id, new_name);

            THEN("author name is updated") {
                REQUIRE(authors.updated_authors.size() == 1);
                CHECK(authors.updated_authors[0].first == author_id);
                CHECK(authors.updated_authors[0].second == new_name);
            }
        }

        WHEN("Deleting a book") {
            const auto book_id = "101";
            use_cases.DeleteBook(book_id);

            THEN("book is deleted") {
                REQUIRE(books.deleted_books.size() == 1);
                CHECK(books.deleted_books[0] == book_id);
            }
        }
    }
}