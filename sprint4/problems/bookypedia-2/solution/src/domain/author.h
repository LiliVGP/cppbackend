#pragma once
#include <string>
#include <vector>

#include "../app/use_cases.h"
#include "../util/tagged_uuid.h"

namespace domain {

namespace detail {
struct AuthorTag {};
}  // namespace detail

using AuthorId = util::TaggedUUID<detail::AuthorTag>;

class Author {
public:
    Author(AuthorId id, std::string name)
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const AuthorId& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

private:
    AuthorId id_;
    std::string name_;
};

class AuthorRepository {
public:
    virtual void Save(const Author& author) = 0;
    virtual void Update(const Author& author) = 0;
    virtual void Delete(const std::string& author_id) = 0;
    virtual std::vector<app::AuthorInfo> GetAll() = 0;
    virtual std::vector<app::AuthorInfo> SearchByName(const std::string& name) = 0;

protected:
    virtual ~AuthorRepository() = default;
};

}  // namespace domain
