#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <iostream>
#include <sstream>

#include "../app/use_cases.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {

// Helper to trim \r from end of string (Windows line endings)
static void trim_cr(std::string& s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
}

std::ostream& operator<<(std::ostream& out, const app::AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const app::BookInfo& book) {
    out << book.title << " by " << book.author_name << ", " << book.publication_year;
    return out;
}

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (const auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s,
                    std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s,
                    std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s,
                    std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s,
                    std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                    std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("ShowBook"s, "<title>"s, "Show book details"s,
                    std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("DeleteAuthor"s, "<name>"s, "Delete author"s,
                    std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "<name>"s, "Edit author"s,
                    std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "<title>"s, "Delete book"s,
                    std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "<title>"s, "Edit book"s,
                    std::bind(&View::EditBook, this, ph::_1));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        trim_cr(name);
        if (name.empty()) {
            output_ << "Failed to add author"sv << std::endl;
            return true;
        }
        use_cases_.AddAuthor(name);
    } catch (...) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        int year;
        std::string title;
        cmd_input >> year;
        if (cmd_input.fail()) return true;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        trim_cr(title);
        if (title.empty()) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }

        std::string author_name;
        output_ << "Enter author name or empty line to select from list: "sv << std::endl;
        std::getline(input_, author_name);
        boost::algorithm::trim(author_name);
        trim_cr(author_name);

        std::string author_id;
        if (author_name.empty()) {
            // Select from list by index
            output_ << "Select author:" << std::endl;
            auto authors = use_cases_.GetAuthors();
            PrintVector(output_, authors);
            output_ << "Enter author # or empty line to cancel" << std::endl;
            std::string sel;
            std::getline(input_, sel);
            boost::algorithm::trim(sel);
            trim_cr(sel);
            if (sel.empty()) return true;
            int idx;
            try { idx = std::stoi(sel); } catch (...) { return true; }
            if (idx < 1 || idx > (int)authors.size()) return true;
            author_id = authors[idx-1].id;
        } else {
            // Search by name
            auto authors = use_cases_.SearchAuthors(author_name);
            if (authors.empty()) {
                output_ << "No author found. Do you want to add " << author_name
                        << " (y/n)?"sv << std::endl;
                std::string resp;
                std::getline(input_, resp);
                boost::algorithm::trim(resp);
                trim_cr(resp);
                if (resp != "y" && resp != "Y") {
                    output_ << "Failed to add book"sv << std::endl;
                    return true;
                }
                use_cases_.AddAuthor(author_name);
                auto new_authors = use_cases_.GetAuthors();
                for (const auto& a : new_authors) {
                    if (a.name == author_name) { author_id = a.id; break; }
                }
                if (author_id.empty()) return true;
            } else if (authors.size() == 1) {
                author_id = authors[0].id;
            } else {
                // Multiple matches - show list and pick
                output_ << "Select author:" << std::endl;
                PrintVector(output_, authors);
                output_ << "Enter author # or empty line to cancel" << std::endl;
                std::string sel;
                std::getline(input_, sel);
                boost::algorithm::trim(sel);
                trim_cr(sel);
                if (sel.empty()) return true;
                int idx;
                try { idx = std::stoi(sel); } catch (...) { return true; }
                if (idx < 1 || idx > (int)authors.size()) return true;
                author_id = authors[idx-1].id;
            }
        }

        // Get tags
        output_ << "Enter tags (comma separated): "sv << std::endl;
        std::string tags_str;
        std::getline(input_, tags_str);
        auto tags = ParseTags(tags_str);
        use_cases_.AddBook(author_id, title, year, tags);
    } catch (...) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    PrintVector(output_, use_cases_.GetAuthors());
    return true;
}

bool View::ShowBooks() const {
    PrintVector(output_, use_cases_.GetBooks());
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        output_ << "Select author:" << std::endl;
        auto authors = use_cases_.GetAuthors();
        PrintVector(output_, authors);
        output_ << "Enter author # or empty line to cancel" << std::endl;
        std::string sel;
        std::getline(input_, sel);
        boost::algorithm::trim(sel);
        trim_cr(sel);
        if (sel.empty()) return true;
        int idx;
        try { idx = std::stoi(sel); } catch (...) { return true; }
        if (idx < 1 || idx > (int)authors.size()) return true;
        PrintVector(output_, use_cases_.GetBooksByAuthor(authors[idx-1].id));
    } catch (...) {}
    return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        trim_cr(title);

        if (title.empty()) {
            // Select by index
            output_ << "Select book:" << std::endl;
            auto books = use_cases_.GetBooks();
            PrintVector(output_, books);
            output_ << "Enter the book # or empty line to cancel: "sv;
            std::string sel;
            std::getline(input_, sel);
            boost::algorithm::trim(sel);
            trim_cr(sel);
            if (sel.empty()) return true;
            int idx;
            try { idx = std::stoi(sel); } catch (...) { return true; }
            if (idx < 1 || idx > (int)books.size()) return true;
            auto detail = use_cases_.GetBookDetail(books[idx-1].id);
            if (detail) {
                output_ << "Title: " << detail->title << std::endl;
                output_ << "Author: " << detail->author_name << std::endl;
                output_ << "Publication year: " << detail->publication_year << std::endl;
                auto tags = use_cases_.GetBookTags(books[idx-1].id);
                if (!tags.empty()) {
                    output_ << "Tags: ";
                    for (size_t i = 0; i < tags.size(); ++i) {
                        if (i > 0) output_ << ", ";
                        output_ << tags[i];
                    }
                    output_ << std::endl;
                }
            }
        } else {
            // Search by title
            auto books = use_cases_.SearchBooks(title);
            if (books.empty()) return true;
            if (books.size() == 1) {
                auto detail = use_cases_.GetBookDetail(books[0].id);
                if (detail) {
                    output_ << "Title: " << detail->title << std::endl;
                    output_ << "Author: " << detail->author_name << std::endl;
                    output_ << "Publication year: " << detail->publication_year << std::endl;
                    auto tags = use_cases_.GetBookTags(books[0].id);
                    if (!tags.empty()) {
                        output_ << "Tags: ";
                        for (size_t i = 0; i < tags.size(); ++i) {
                            if (i > 0) output_ << ", ";
                            output_ << tags[i];
                        }
                        output_ << std::endl;
                    }
                }
            } else {
                // Multiple matches - show list and pick
                PrintVector(output_, books);
                output_ << "Enter the book # or empty line to cancel: "sv;
                std::string sel;
                std::getline(input_, sel);
                boost::algorithm::trim(sel);
                trim_cr(sel);
                if (sel.empty()) return true;
                int idx;
                try { idx = std::stoi(sel); } catch (...) { return true; }
                if (idx < 1 || idx > (int)books.size()) return true;
                auto detail = use_cases_.GetBookDetail(books[idx-1].id);
                if (detail) {
                    output_ << "Title: " << detail->title << std::endl;
                    output_ << "Author: " << detail->author_name << std::endl;
                    output_ << "Publication year: " << detail->publication_year << std::endl;
                    auto tags = use_cases_.GetBookTags(books[idx-1].id);
                    if (!tags.empty()) {
                        output_ << "Tags: ";
                        for (size_t i = 0; i < tags.size(); ++i) {
                            if (i > 0) output_ << ", ";
                            output_ << tags[i];
                        }
                        output_ << std::endl;
                    }
                }
            }
        }
    } catch (...) {}
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        trim_cr(name);

        if (name.empty()) {
            // Select by index
            output_ << "Select author:" << std::endl;
            auto authors = use_cases_.GetAuthors();
            PrintVector(output_, authors);
            output_ << "Enter author # or empty line to cancel" << std::endl;
            std::string sel;
            std::getline(input_, sel);
            boost::algorithm::trim(sel);
            trim_cr(sel);
            if (sel.empty()) return true;
            int idx;
            try { idx = std::stoi(sel); } catch (...) { return true; }
            if (idx < 1 || idx > (int)authors.size()) return true;
            use_cases_.DeleteAuthor(authors[idx-1].id);
        } else {
            // Search by exact name
            auto authors = use_cases_.SearchAuthors(name);
            if (authors.empty()) {
                output_ << "Failed to delete author"sv << std::endl;
                return true;
            }
            if (authors.size() == 1) {
                use_cases_.DeleteAuthor(authors[0].id);
            } else {
                // Multiple matches - show list and pick
                PrintVector(output_, authors);
                output_ << "Enter the author # or empty line to cancel: "sv;
                std::string sel;
                std::getline(input_, sel);
                boost::algorithm::trim(sel);
                trim_cr(sel);
                if (sel.empty()) return true;
                int idx;
                try { idx = std::stoi(sel); } catch (...) { return true; }
                if (idx < 1 || idx > (int)authors.size()) return true;
                use_cases_.DeleteAuthor(authors[idx-1].id);
            }
        }
    } catch (...) {
        output_ << "Failed to delete author"sv << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        trim_cr(name);

        if (name.empty()) {
            // Select by index
            output_ << "Select author:" << std::endl;
            auto authors = use_cases_.GetAuthors();
            PrintVector(output_, authors);
            output_ << "Enter author # or empty line to cancel" << std::endl;
            std::string sel;
            std::getline(input_, sel);
            boost::algorithm::trim(sel);
            trim_cr(sel);
            if (sel.empty()) return true;
            int idx;
            try { idx = std::stoi(sel); } catch (...) { return true; }
            if (idx < 1 || idx > (int)authors.size()) return true;
            output_ << "Enter new name: "sv;
            std::string new_name;
            std::getline(input_, new_name);
            boost::algorithm::trim(new_name);
            trim_cr(new_name);
            use_cases_.UpdateAuthor(authors[idx-1].id, new_name);
        } else {
            // Search by exact name
            auto authors = use_cases_.SearchAuthors(name);
            if (authors.empty()) {
                output_ << "Failed to edit author"sv << std::endl;
                return true;
            }
            if (authors.size() == 1) {
                output_ << "Enter new name: "sv;
                std::string new_name;
                std::getline(input_, new_name);
                boost::algorithm::trim(new_name);
                trim_cr(new_name);
                use_cases_.UpdateAuthor(authors[0].id, new_name);
            } else {
                // Multiple matches - show list and pick
                PrintVector(output_, authors);
                output_ << "Enter the author # or empty line to cancel: "sv;
                std::string sel;
                std::getline(input_, sel);
                boost::algorithm::trim(sel);
                trim_cr(sel);
                if (sel.empty()) return true;
                int idx;
                try { idx = std::stoi(sel); } catch (...) { return true; }
                if (idx < 1 || idx > (int)authors.size()) return true;
                output_ << "Enter new name: "sv;
                std::string new_name;
                std::getline(input_, new_name);
                boost::algorithm::trim(new_name);
                trim_cr(new_name);
                use_cases_.UpdateAuthor(authors[idx-1].id, new_name);
            }
        }
    } catch (...) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        trim_cr(title);

        if (title.empty()) {
            // Select by index
            output_ << "Select book:" << std::endl;
            auto books = use_cases_.GetBooks();
            PrintVector(output_, books);
            output_ << "Enter the book # or empty line to cancel: "sv;
            std::string sel;
            std::getline(input_, sel);
            boost::algorithm::trim(sel);
            trim_cr(sel);
            if (sel.empty()) return true;
            int idx;
            try { idx = std::stoi(sel); } catch (...) { return true; }
            if (idx < 1 || idx > (int)books.size()) return true;
            use_cases_.DeleteBook(books[idx-1].id);
        } else {
            // Search by title
            auto books = use_cases_.SearchBooks(title);
            if (books.empty()) return true;
            if (books.size() == 1) {
                use_cases_.DeleteBook(books[0].id);
            } else {
                // Multiple matches - show list and pick
                PrintVector(output_, books);
                output_ << "Enter the book # or empty line to cancel: "sv;
                std::string sel;
                std::getline(input_, sel);
                boost::algorithm::trim(sel);
                trim_cr(sel);
                if (sel.empty()) return true;
                int idx;
                try { idx = std::stoi(sel); } catch (...) { return true; }
                if (idx < 1 || idx > (int)books.size()) return true;
                use_cases_.DeleteBook(books[idx-1].id);
            }
        }
    } catch (...) {
        return true;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        trim_cr(title);

        if (title.empty()) {
            // Select by index
            output_ << "Select book:" << std::endl;
            auto books = use_cases_.GetBooks();
            PrintVector(output_, books);
            output_ << "Enter the book # or empty line to cancel: "sv;
            std::string sel;
            std::getline(input_, sel);
            boost::algorithm::trim(sel);
            trim_cr(sel);
            if (sel.empty()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            int idx;
            try { idx = std::stoi(sel); } catch (...) { return true; }
            if (idx < 1 || idx > (int)books.size()) return true;
            auto book = use_cases_.GetBookDetail(books[idx-1].id);
            if (!book) return true;
            output_ << "Enter new title or empty line to use the current one ("
                    << book->title << "): "sv << std::endl;
            std::string new_title;
            std::getline(input_, new_title);
            boost::algorithm::trim(new_title);
            trim_cr(new_title);

            output_ << "Enter publication year or empty line to use the current one ("
                    << book->publication_year << "): "sv << std::endl;
            std::string year_str;
            std::getline(input_, year_str);
            boost::algorithm::trim(year_str);
            trim_cr(year_str);

            output_ << "Enter tags (current tags: ): "sv << std::endl;
            std::string tags_str;
            std::getline(input_, tags_str);
            auto tags = ParseTags(tags_str);

            int new_year = book->publication_year;
            if (!year_str.empty()) {
                try { new_year = std::stoi(year_str); } catch (...) {}
            }
            std::string final_title = new_title.empty() ? book->title : new_title;
            use_cases_.UpdateBook(books[idx-1].id, final_title, new_year, tags);
        } else {
            // Search by title
            auto books = use_cases_.SearchBooks(title);
            if (books.empty()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            if (books.size() == 1) {
                output_ << "Enter new title or empty line to use the current one ("
                        << books[0].title << "): "sv << std::endl;
                std::string new_title;
                std::getline(input_, new_title);
                boost::algorithm::trim(new_title);
                trim_cr(new_title);

                output_ << "Enter publication year or empty line to use the current one ("
                        << books[0].publication_year << "): "sv << std::endl;
                std::string year_str;
                std::getline(input_, year_str);
                boost::algorithm::trim(year_str);
                trim_cr(year_str);

                output_ << "Enter tags (current tags: ): "sv << std::endl;
                std::string tags_str;
                std::getline(input_, tags_str);
                auto tags = ParseTags(tags_str);

                int new_year = books[0].publication_year;
                if (!year_str.empty()) {
                    try { new_year = std::stoi(year_str); } catch (...) {}
                }
                std::string final_title = new_title.empty() ? books[0].title : new_title;
                use_cases_.UpdateBook(books[0].id, final_title, new_year, tags);
            } else {
                // Multiple matches - show list and pick
                PrintVector(output_, books);
                output_ << "Enter the book # or empty line to cancel: "sv;
                std::string sel;
                std::getline(input_, sel);
                boost::algorithm::trim(sel);
                trim_cr(sel);
                if (sel.empty()) {
                    output_ << "Book not found"sv << std::endl;
                    return true;
                }
                int book_idx;
                try { book_idx = std::stoi(sel); } catch (...) { return true; }
                if (book_idx < 1 || book_idx > (int)books.size()) return true;
output_ << "Enter new title or empty line to use the current one ("
                        << books[book_idx-1].title << "): "sv << std::endl;
                std::string new_title;
                std::getline(input_, new_title);
                boost::algorithm::trim(new_title);
                trim_cr(new_title);

                output_ << "Enter publication year or empty line to use the current one ("
                        << books[book_idx-1].publication_year << "): "sv << std::endl;
                std::string year_str;
                std::getline(input_, year_str);
                boost::algorithm::trim(year_str);
                trim_cr(year_str);

                output_ << "Enter tags (current tags: ): "sv << std::endl;
                std::string tags_str;
                std::getline(input_, tags_str);
                auto tags = ParseTags(tags_str);

                int new_year = books[book_idx-1].publication_year;
                if (!year_str.empty()) {
                    try { new_year = std::stoi(year_str); } catch (...) {}
                }
                std::string final_title = new_title.empty() ? books[book_idx-1].title : new_title;
                use_cases_.UpdateBook(books[book_idx-1].id, final_title, new_year, tags);
            }
        }
    } catch (...) {
        return true;
    }
    return true;
}

std::vector<std::string> View::ParseTags(const std::string& tags_str) const {
    std::vector<std::string> tags;
    if (tags_str.empty()) return tags;

    std::stringstream ss(tags_str);
    std::string tag;
    while (std::getline(ss, tag, ',')) {
        boost::algorithm::trim(tag);
        trim_cr(tag);
        if (!tag.empty()) {
            // Normalize multiple spaces
            std::string normalized;
            bool prev_space = false;
            for (char c : tag) {
                if (std::isspace(static_cast<unsigned char>(c))) {
                    if (!prev_space && !normalized.empty()) {
                        normalized += ' ';
                        prev_space = true;
                    }
                } else {
                    normalized += c;
                    prev_space = false;
                }
            }
            tag = normalized;

            // Check for duplicates
            if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
                tags.push_back(tag);
            }
        }
    }

    // Sort tags alphabetically
    std::sort(tags.begin(), tags.end());
    return tags;
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    return use_cases_.GetAuthors();
}

std::vector<detail::BookInfo> View::GetBooks() const {
    return use_cases_.GetBooks();
}

std::vector<detail::BookInfo> View::GetAuthorBooks(const std::string& author_id) const {
    return use_cases_.GetBooksByAuthor(author_id);
}

}  // namespace ui
