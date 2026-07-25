#pragma once

#include <memory>
#include <string>
#include <vector>
#include <compare>
#include <cstdint>
#include <unordered_map>

#include "geom.h"
#include "tagged.h"

// Предварительное объявление для сериализации
namespace serialization {
    class GameStateRepr;
}

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

        Id id{ 0u };
        LostObjectType type{ 0u };
        geom::Point2D position;  // позиция предмета на карте

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

        Dog() = default;
        Dog(Id id, std::string name, geom::Point2D pos, size_t bag_cap)
            : id_(std::move(id))
            , name_(std::move(name))
            , position_(pos)
            , bag_cap_(bag_cap) {
            bag_.reserve(bag_cap);
        }

        const Id& GetId() const noexcept {
            return id_;
        }

        const std::string& GetName() const noexcept {
            return name_;
        }

        const geom::Point2D& GetPosition() const noexcept {
            return position_;
        }

        const geom::Vec2D& GetSpeed() const noexcept {
            return speed_;
        }

        void SetSpeed(geom::Vec2D speed) noexcept {
            speed_ = speed;
        }

        void SetPosition(geom::Point2D position) noexcept {
            position_ = position;
        }

        void SetDirection(Direction direction) noexcept {
            direction_ = direction;
        }

        size_t GetBagCapacity() const noexcept {
            return bag_cap_;
        }

        Direction GetDirection() const noexcept {
            return direction_;
        }

        Score GetScore() const noexcept {
            return score_;
        }

        [[nodiscard]] bool PutToBag(FoundObject item) {
            if (IsBagFull()) {
                return false;
            }

            bag_.push_back(item);
            return true;
        }

        size_t EmptyBag() noexcept {
            auto res = bag_.size();
            bag_.clear();

            return res;
        }

        bool IsBagFull() const noexcept {
            return bag_.size() >= bag_cap_;
        }

        const BagContent& GetBagContent() const noexcept {
            return bag_;
        }

        void AddScore(Score score) noexcept {
            score_ += score;
        }

    private:
        Id id_{ 0u };
        std::string name_;
        geom::Point2D position_;
        geom::Vec2D speed_;
        Direction direction_{ Direction::NORTH };
        std::vector<FoundObject> bag_;
        size_t bag_cap_{ 0 };
        Score score_{ 0 };
    };

    using DogPtr = std::shared_ptr<Dog>;
    using ConstDogPtr = std::shared_ptr<const Dog>;

    class Map {
    public:
        using Id = util::Tagged<std::string, Map>;

        Map() = default;
        Map(Id id, std::string name)
            : id_(std::move(id))
            , name_(std::move(name)) {
        }

        const Id& GetId() const noexcept {
            return id_;
        }

        const std::string& GetName() const noexcept {
            return name_;
        }

    private:
        Id id_{ "" };
        std::string name_;
    };

    // Класс Game - основное игровое состояние
    class Game {
    public:
        // Конструктор
        Game() = default;
        explicit Game(std::vector<Map> maps);

        // --- Методы для управления картами ---

        // Получить все карты
        const std::vector<Map>& GetMaps() const noexcept { return maps_; }

        // Найти карту по ID
        const Map* FindMap(const Map::Id& id) const;
        Map* FindMap(const Map::Id& id);

        // --- Методы для управления собаками ---

        // Добавить собаку на карту (возвращает ID собаки)
        Dog::Id AddDog(const Map::Id& map_id, const std::string& name, geom::Point2D pos);

        // Получить собаку по ID
        Dog* GetDog(const Dog::Id& dog_id);
        const Dog* GetDog(const Dog::Id& dog_id) const;

        // Получить всех собак на карте
        std::vector<Dog*> GetDogsOnMap(const Map::Id& map_id);
        std::vector<const Dog*> GetDogsOnMap(const Map::Id& map_id) const;

        // --- Методы для управления потерянными предметами ---

        // Добавить потерянный предмет на карту
        void AddLoot(const Map::Id& map_id, FoundObject loot);

        // Получить все предметы на карте
        const std::vector<FoundObject>& GetLootOnMap(const Map::Id& map_id) const;

        // --- Сериализация/восстановление состояния ---

        // Получить полное состояние игры для сериализации
        serialization::GameStateRepr GetGameStateRepr() const;

        // Восстановить состояние из репрезентации
        void RestoreFromRepr(const serialization::GameStateRepr& repr);

    private:
        struct MapState {
            Map map;                        // статическая информация о карте
            std::vector<Dog> dogs;          // все собаки на карте
            std::vector<FoundObject> loot;  // все потерянные предметы
        };

        std::vector<Map> maps_;                    // статические карты
        std::unordered_map<Map::Id, MapState> map_states_;  // динамическое состояние карт

        // Вспомогательные методы
        MapState& GetMapState(const Map::Id& map_id);
        const MapState& GetMapState(const Map::Id& map_id) const;
    };

} // namespace model