#pragma once
#include <string>

namespace domain {

class Tag {
public:
    Tag(std::string name)
        : name_(std::move(name)) {
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

private:
    std::string name_;
};

class TagRepository {
public:
    virtual void Save(const std::string& book_id, const std::string& tag) = 0;
    virtual void DeleteBookTags(const std::string& book_id) = 0;
    virtual std::vector<std::string> GetBookTags(const std::string& book_id) = 0;

protected:
    ~TagRepository() = default;
};

}  // namespace domain