#pragma once
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

#include "model.h"
#include "tagged.h"
#include "application.h"

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

}  // namespace geom

namespace model {

    template <typename Archive>
    void serialize(Archive& ar, Point& point, [[maybe_unused]] const unsigned version) {
        ar& point.x;
        ar& point.y;
    }

    template <typename Archive>
    void serialize(Archive& ar, FoundObject& obj, [[maybe_unused]] const unsigned version) {
        ar&* obj.id;
        ar& obj.type;
    }

    template <typename Archive>
    void serialize(Archive& ar, FoundObject::Id& id, [[maybe_unused]] const unsigned version) {
        uint32_t val = *id;
        ar& val;
        id = FoundObject::Id{ val };
    }

    template <typename Archive>
    void serialize(Archive& ar, Dog::Id& id, [[maybe_unused]] const unsigned version) {
        uint32_t val = *id;
        ar& val;
        id = Dog::Id{ val };
    }

    // Сериализация Dog через DogRepr (временное представление)
    class DogRepr {
    public:
        DogRepr() = default;
        explicit DogRepr(const Dog& dog)
            : id_(dog.GetId())
            , name_(dog.GetName())
            , pos_(dog.GetPosition())
            , bag_capacity_(dog.GetBagCapacity())
            , speed_(dog.GetSpeed())
            , direction_(dog.GetDirection())
            , score_(dog.GetScore())
            , bag_content_(dog.GetBagContent()) {
        }

        Dog Restore() const {
            Dog dog{ id_, name_, pos_, bag_capacity_ };
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
        Dog::Id id_ = Dog::Id{ 0u };
        std::string name_;
        geom::Point2D pos_;
        size_t bag_capacity_ = 0;
        geom::Vec2D speed_;
        Direction direction_ = Direction::NORTH;
        Score score_ = 0;
        Dog::BagContent bag_content_;
    };

    // Сериализация всей игровой модели
    struct GameState {
        std::vector<DogRepr> dog_reprs;
        // Добавьте сюда другие объекты: карты, потерянные предметы, токены игроков

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& dog_reprs;
            // ar & cards; // сериализуйте остальные поля
        }

        void LoadToModel(GameModel& model) {
            for (const auto& repr : dog_reprs) {
                auto dog = std::make_shared<Dog>(repr.Restore());
                model.AddDog(dog);
            }
            // Загрузите остальные объекты
        }

        void SaveFromModel(const GameModel& model) {
            dog_reprs.clear();
            for (const auto& dog : model.GetDogs()) {
                dog_reprs.emplace_back(*dog);
            }
            // Сохраните остальные объекты
        }
    };

    // Класс для управления сериализацией
    class StateManager {
    public:
        StateManager(const std::string& state_file, std::chrono::milliseconds save_period = std::chrono::milliseconds{ 0 })
            : state_file_(state_file)
            , save_period_(save_period)
            , last_save_time_(std::chrono::milliseconds{ 0 }) {
        }

        // Подключить к сигналам Application
        void ConnectToApplication(app::Application& app) {
            app.DoOnTick([this](app::Milliseconds delta) {
                OnTick(delta);
                });
            app.DoOnSave([this]() {
                OnSave();
                });
        }

        void OnTick(app::Milliseconds delta) {
            if (state_file_.empty()) return;
            elapsed_time_ += delta;
            if (save_period_.count() > 0 && elapsed_time_ - last_save_time_ >= save_period_) {
                SaveState();
                last_save_time_ = elapsed_time_;
            }
        }

        void OnSave() {
            if (!state_file_.empty()) {
                SaveState();
            }
        }

        bool TryLoadState(GameModel& model) {
            if (state_file_.empty()) return false;
            if (!std::filesystem::exists(state_file_)) return false;

            try {
                std::ifstream file(state_file_);
                boost::archive::text_iarchive ar(file);
                GameState state;
                ar& state;
                state.LoadToModel(model);
                return true;
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to load state: " << e.what() << std::endl;
                throw;
            }
        }

        void SaveState() const {
            if (state_file_.empty()) return;

            std::string temp_file = state_file_ + ".tmp";
            try {
                // Сохраняем состояние из модели
                GameState state;
                state.SaveFromModel(model_);

                std::ofstream file(temp_file);
                boost::archive::text_oarchive ar(file);
                ar& state;

                std::filesystem::rename(temp_file, state_file_);
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to save state: " << e.what() << std::endl;
                throw;
            }
        }

        void SetModel(const GameModel& model) { model_ = model; }

    private:
        std::string state_file_;
        std::chrono::milliseconds save_period_;
        std::chrono::milliseconds elapsed_time_{ 0 };
        std::chrono::milliseconds last_save_time_{ 0 };
        GameModel model_;  // ссылка на модель, которую будем сохранять
    };

}  // namespace model