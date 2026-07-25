#pragma once

#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/optional.hpp>

#include "model.h"
#include "geom.h"
#include "types.h"

namespace geom {

    template <typename Archive>
    void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
        ar& point.x;
        ar& point.y;
    }

    template <typename Archive>
    void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
        ar& vec.x;
        ar& vec.y;
    }

} // namespace geom

namespace model {

    template <typename Archive>
    void serialize(Archive& ar, FoundObject& obj, [[maybe_unused]] const unsigned version) {
        ar&* obj.id;
        ar& obj.type;
        ar& obj.position;
    }

} // namespace model

namespace serialization {

    // Репрезентация собаки
    class DogRepr {
    public:
        DogRepr() = default;

        explicit DogRepr(const model::Dog& dog)
            : id_(dog.GetId())
            , name_(dog.GetName())
            , pos_(dog.GetPosition())
            , bag_capacity_(dog.GetBagCapacity())
            , speed_(dog.GetSpeed())
            , direction_(dog.GetDirection())
            , score_(dog.GetScore())
            , bag_content_(dog.GetBagContent()) {
        }

        [[nodiscard]] model::Dog Restore() const {
            model::Dog dog{ id_, name_, pos_, bag_capacity_ };
            dog.SetSpeed(speed_);
            dog.SetDirection(direction_);
            dog.AddScore(score_);
            for (const auto& item : bag_content_) {
                if (!dog.PutToBag(item)) {
                    throw std::runtime_error("Failed to put bag content");
                }
            }
            return dog;
        }

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar&* id_;
            ar& name_;
            ar& pos_;
            ar& bag_capacity_;
            ar& speed_;
            ar& direction_;
            ar& score_;
            ar& bag_content_;
        }

    private:
        model::Dog::Id id_ = model::Dog::Id{ 0u };
        std::string name_;
        geom::Point2D pos_;
        size_t bag_capacity_ = 0;
        geom::Vec2D speed_;
        model::Direction direction_ = model::Direction::NORTH;
        model::Score score_ = 0;
        model::Dog::BagContent bag_content_;
    };

    // Репрезентация потерянного предмета (с позицией)
    struct LootRepr {
        model::FoundObject::Id id = model::FoundObject::Id{ 0u };
        model::LostObjectType type = 0u;
        geom::Point2D position; // позиция предмета на карте

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar&* id;
            ar& type;
            ar& position;
        }
    };

    // Репрезентация игрока (сессии)
    struct PlayerRepr {
        app::PlayerId player_id = app::PlayerId{ 0u };
        model::Dog::Id dog_id = model::Dog::Id{ 0u };
        app::Token token = app::Token{ "" };
        model::Map::Id map_id = model::Map::Id{ "" };

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar&* player_id;
            ar&* dog_id;
            ar&* token;
            ar&* map_id;
        }
    };

    // Репрезентация состояния одной карты
    struct MapStateRepr {
        model::Map::Id map_id = model::Map::Id{ "" };
        std::vector<DogRepr> dogs;
        std::vector<LootRepr> loot_items;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar&* map_id;
            ar& dogs;
            ar& loot_items;
        }
    };

    // Полное состояние игры
    class GameStateRepr {
    public:
        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& maps;
            ar& players;
        }

        std::vector<MapStateRepr> maps;
        std::vector<PlayerRepr> players;
    };

} // namespace serialization