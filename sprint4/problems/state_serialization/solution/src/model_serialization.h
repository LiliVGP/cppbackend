#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>

#include "model.h"

namespace geom {

    template <typename Archive>
    void serialize(Archive& ar, geom::Point2D& point, [[maybe_unused]] const unsigned version) {
        ar& point.x;
        ar& point.y;
    }

    template <typename Archive>
    void serialize(Archive& ar, geom::Vec2D& vec, [[maybe_unused]] const unsigned version) {
        ar& vec.x;
        ar& vec.y;
    }

}  // namespace geom

namespace model {

    // Сериализация FoundObject
    template <typename Archive>
    void serialize(Archive& ar, FoundObject& obj, [[maybe_unused]] const unsigned version) {
        // Сериализуем значение внутри Tagged, а не сам Tagged
        uint32_t id_value = *obj.id;
        ar& id_value;
        if constexpr (!Archive::is_saving::value) {
            obj.id = FoundObject::Id{id_value};
        }
        ar& obj.type;
    }

    // Сериализация Direction (enum)
    template <typename Archive>
    void serialize(Archive& ar, Direction& dir, [[maybe_unused]] const unsigned version) {
        ar& dir;
    }

}  // namespace model

namespace serialization {

    // DogRepr - сериализованное представление класса Dog
    class DogRepr {
    public:
        DogRepr() = default;

        explicit DogRepr(const model::Dog& dog)
            : id_(*dog.GetId())
            , name_(dog.GetName())
            , pos_(dog.GetPosition())
            , bag_capacity_(dog.GetBagCapacity())
            , speed_(dog.GetSpeed())
            , direction_(dog.GetDirection())
            , score_(dog.GetScore())
            , bag_content_(dog.GetBagContent()) {
        }

        [[nodiscard]] model::Dog Restore() const {
            model::Dog::Id dog_id{ id_ };
            model::Dog dog{ dog_id, name_, pos_, bag_capacity_ };
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
            ar& id_;
            ar& name_;
            ar& pos_;
            ar& bag_capacity_;
            ar& speed_;
            ar& direction_;
            ar& score_;
            ar& bag_content_;
        }

    private:
        uint32_t id_ = 0;
        std::string name_;
        geom::Point2D pos_;
        size_t bag_capacity_ = 0;
        geom::Vec2D speed_;
        model::Direction direction_ = model::Direction::NORTH;
        model::Score score_ = 0;
        model::Dog::BagContent bag_content_;
    };

}  // namespace serialization
