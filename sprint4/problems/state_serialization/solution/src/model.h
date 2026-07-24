#pragma once
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <chrono>

#include "geom.h"
#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
    constexpr auto operator<=>(const Point&) const = default;
};

struct Size {
    Dimension width, height;
};

using LostObjectType = unsigned;
using Score = unsigned;

struct FoundObject {
    using Id = util::Tagged<uint32_t, FoundObject>;
    Id id{0u};
    LostObjectType type{0u};

    [[nodiscard]] auto operator<=>(const FoundObject&) const = default;
};

enum class Direction {
    NORTH,
    EAST,
    WEST,
    SOUTH,
};

class Dog {
public:
    using Id = util::Tagged<uint32_t, Dog>;
    using BagContent = std::vector<FoundObject>;

    Dog(Id id, std::string name, geom::Point2D pos, size_t bag_cap)
        : id_(std::move(id))
        , name_(std::move(name))
        , position_(pos)
        , bag_cap_(bag_cap) {
        bag_.reserve(bag_cap);
    }

    const Id& GetId() const noexcept { return id_; }
    const std::string GetName() const noexcept { return name_; }
    const geom::Point2D& GetPosition() const noexcept { return position_; }
    const geom::Vec2D& GetSpeed() const noexcept { return speed_; }
    void SetSpeed(geom::Vec2D speed) noexcept { speed_ = speed; }
    void SetPosition(geom::Point2D position) noexcept { position_ = position; }
    void SetDirection(Direction direction) noexcept { direction_ = direction; }
    size_t GetBagCapacity() const noexcept { return bag_cap_; }
    Direction GetDirection() const noexcept { return direction_; }
    Score GetScore() const noexcept { return score_; }

    [[nodiscard]] bool PutToBag(FoundObject item) {
        if (IsBagFull()) return false;
        bag_.push_back(item);
        return true;
    }

    size_t EmptyBag() noexcept {
        auto res = bag_.size();
        bag_.clear();
        return res;
    }

    bool IsBagFull() const noexcept { return bag_.size() >= bag_cap_; }
    const BagContent& GetBagContent() const noexcept { return bag_; }
    void AddScore(Score score) noexcept { score_ += score; }

private:
    Id id_;
    std::string name_;
    geom::Point2D position_;
    geom::Vec2D speed_;
    Direction direction_{Direction::NORTH};
    std::vector<FoundObject> bag_;
    size_t bag_cap_;
    Score score_{};
};

using DogPtr = std::shared_ptr<Dog>;
using ConstDogPtr = std::shared_ptr<const Dog>;

// Игровая модель - основной контейнер всех объектов
class GameModel {
public:
    GameModel() = default;

    void AddDog(DogPtr dog) { dogs_.push_back(dog); }
    const std::vector<DogPtr>& GetDogs() const noexcept { return dogs_; }
    std::vector<DogPtr>& GetDogs() noexcept { return dogs_; }

    void AddToken(const std::string& token, const std::string& player_id) {
        tokens_[token] = player_id;
    }
    
    const std::map<std::string, std::string>& GetTokens() const noexcept { return tokens_; }
    std::map<std::string, std::string>& GetTokens() noexcept { return tokens_; }

    // Добавьте другие методы для управления объектами

private:
    std::vector<DogPtr> dogs_;
    std::map<std::string, std::string> tokens_; // token -> player_id
    // Здесь будут карты, предметы и т.д.
};

}  // namespace model