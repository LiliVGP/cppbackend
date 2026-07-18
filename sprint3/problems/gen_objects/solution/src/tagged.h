#pragma once
#include <cstdint>
#include <functional>

template <typename Tag>
class Tagged {
public:
    using IdType = uint32_t;

    Tagged() : id_(0) {}
    explicit Tagged(IdType id) : id_(id) {}

    IdType GetId() const { return id_; }

    bool operator==(const Tagged& other) const { return id_ == other.id_; }
    bool operator!=(const Tagged& other) const { return id_ != other.id_; }

    Tagged& operator++() {
        ++id_;
        return *this;
    }

    Tagged operator++(int) {
        Tagged tmp = *this;
        ++id_;
        return tmp;
    }

private:
    IdType id_;
};

namespace std {
    template <typename Tag>
    struct hash<Tagged<Tag>> {
        size_t operator()(const Tagged<Tag>& tagged) const {
            return hash<typename Tagged<Tag>::IdType>()(tagged.GetId());
        }
    };
}  // namespace std