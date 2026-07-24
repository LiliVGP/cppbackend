#pragma once
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/signals2.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <chrono>

#include "model.h"

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar & point.x;
    ar & point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar & vec.x;
    ar & vec.y;
}

}  // namespace geom

namespace model {

template <typename Archive>
void serialize(Archive& ar, Point& point, [[maybe_unused]] const unsigned version) {
    ar & point.x;
    ar & point.y;
}

template <typename Archive>
void serialize(Archive& ar, FoundObject& obj, [[maybe_unused]] const unsigned version) {
    ar & *obj.id;
    ar & obj.type;
}

template <typename Archive>
void serialize(Archive& ar, FoundObject::Id& id, [[maybe_unused]] const unsigned version) {
    uint32_t val = *id;
    ar & val;
    id = FoundObject::Id{val};
}

template <typename Archive>
void serialize(Archive& ar, Dog::Id& id, [[maybe_unused]] const unsigned version) {
    uint32_t val = *id;
    ar & val;
    id = Dog::Id{val};
}

// Интерфейс наблюдателя (должен быть объявлен до StateManager)
class ApplicationObserver {
public:
    virtual ~ApplicationObserver() = default;
    virtual void OnTick(std::chrono::milliseconds delta) = 0;
    virtual void OnSave() = 0;
};

// Сериализация Dog через DogRepr
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
        , bag_content_(dog.GetBagContent()) {}

    Dog Restore() const {
        Dog dog{id_, name_, pos_, bag_capacity_};
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
        ar & id_;
        ar & name_;
        ar & pos_;
        ar & bag_capacity_;
        ar & speed_;
        ar & direction_;
        ar & score_;
        ar & bag_content_;
    }

private:
    Dog::Id id_ = Dog::Id{0u};
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
    std::map<std::string, std::string> tokens; // token -> player_id
    // Добавьте сюда другие объекты: карты, потерянные предметы

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & dog_reprs;
        ar & tokens;
    }

    void LoadToModel(GameModel& model) {
        for (const auto& repr : dog_reprs) {
            auto dog = std::make_shared<Dog>(repr.Restore());
            model.AddDog(dog);
        }
        // Загрузите токены
        for (const auto& [token, player_id] : tokens) {
            model.AddToken(token, player_id);
        }
    }

    void SaveFromModel(const GameModel& model) {
        dog_reprs.clear();
        for (const auto& dog : model.GetDogs()) {
            dog_reprs.emplace_back(*dog);
        }
        tokens = model.GetTokens();
    }
};

// Менеджер состояния - реализует ApplicationObserver
class StateManager : public ApplicationObserver {
public:
    StateManager(GameModel& model, const std::string& state_file, std::chrono::milliseconds save_period = std::chrono::milliseconds{0})
        : model_(model)
        , state_file_(state_file)
        , save_period_(save_period)
        , last_save_time_(std::chrono::milliseconds{0}) {}

    void OnTick(std::chrono::milliseconds delta) override {
        if (state_file_.empty()) return;
        elapsed_time_ += delta;
        if (save_period_.count() > 0 && elapsed_time_ - last_save_time_ >= save_period_) {
            SaveState();
            last_save_time_ = elapsed_time_;
        }
    }

    void OnSave() override {
        if (!state_file_.empty()) {
            SaveState();
        }
    }

    bool TryLoadState() {
        if (state_file_.empty()) return false;
        if (!std::filesystem::exists(state_file_)) return false;

        try {
            std::ifstream file(state_file_);
            if (!file.is_open()) {
                std::cerr << "Failed to open state file: " << state_file_ << std::endl;
                return false;
            }
            
            boost::archive::text_iarchive ar(file);
            GameState state;
            ar & state;
            state.LoadToModel(model_);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to load state: " << e.what() << std::endl;
            throw;
        }
    }

    void SaveState() const {
        if (state_file_.empty()) return;

        std::string temp_file = state_file_ + ".tmp";
        try {
            GameState state;
            state.SaveFromModel(model_);
            
            std::ofstream file(temp_file);
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open temporary file: " + temp_file);
            }
            
            boost::archive::text_oarchive ar(file);
            ar & state;
            file.close();
            
            // Атомарное переименование
            if (std::filesystem::exists(state_file_)) {
                std::filesystem::remove(state_file_);
            }
            std::filesystem::rename(temp_file, state_file_);
            std::cout << "State saved to " << state_file_ << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to save state: " << e.what() << std::endl;
            // Удаляем временный файл в случае ошибки
            try {
                if (std::filesystem::exists(temp_file)) {
                    std::filesystem::remove(temp_file);
                }
            } catch (...) {
                // Игнорируем ошибки удаления временного файла
            }
            throw;
        }
    }

private:
    GameModel& model_;
    std::string state_file_;
    std::chrono::milliseconds save_period_;
    std::chrono::milliseconds elapsed_time_{0};
    std::chrono::milliseconds last_save_time_{0};
};

// Класс Application с сигналами
class Application {
public:
    using TickSignal = boost::signals2::signal<void(std::chrono::milliseconds delta)>;
    using SaveSignal = boost::signals2::signal<void()>;

    Application() = default;

    boost::signals2::connection DoOnTick(const TickSignal::slot_type& handler) {
        return tick_signal_.connect(handler);
    }

    boost::signals2::connection DoOnSave(const SaveSignal::slot_type& handler) {
        return save_signal_.connect(handler);
    }

    void Tick(std::chrono::milliseconds delta) {
        // Обновление игрового состояния...
        tick_signal_(delta);
    }

    void SaveState() {
        save_signal_();
    }

    GameModel& GetGameModel() { return game_model_; }
    const GameModel& GetGameModel() const { return game_model_; }

private:
    TickSignal tick_signal_;
    SaveSignal save_signal_;
    GameModel game_model_;
};

}  // namespace model