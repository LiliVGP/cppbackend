#include <iostream>
#include <filesystem>
#include <chrono>
#include <string>
#include <optional>
#include <thread>
#include <vector>
#include <fstream>

#include <boost/asio.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/signals2.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>

#include "model.h"
#include "model_serialization.h"

namespace net = boost::asio;
namespace sys = boost::system;

using namespace std::literals;

// Состояние игры - полный набор данных для сохранения
struct GameState {
    std::vector<model::Dog> dogs;
    // Здесь будут другие компоненты: предметы, токены игроков и т.д.
    // Пока оставляем заглушку, вы добавите свои компоненты

    // Для тестов - метод проверки равенства
    bool operator==(const GameState& other) const {
        if (dogs.size() != other.dogs.size()) return false;
        for (size_t i = 0; i < dogs.size(); ++i) {
            if (dogs[i].GetId() != other.dogs[i].GetId()) return false;
            if (dogs[i].GetName() != other.dogs[i].GetName()) return false;
            if (dogs[i].GetPosition() != other.dogs[i].GetPosition()) return false;
            if (dogs[i].GetSpeed() != other.dogs[i].GetSpeed()) return false;
            if (dogs[i].GetBagCapacity() != other.dogs[i].GetBagCapacity()) return false;
            if (dogs[i].GetDirection() != other.dogs[i].GetDirection()) return false;
            if (dogs[i].GetScore() != other.dogs[i].GetScore()) return false;
            if (dogs[i].GetBagContent() != other.dogs[i].GetBagContent()) return false;
        }
        return true;
    }
};

// Сериализованное представление состояния игры
class GameStateRepr {
public:
    GameStateRepr() = default;

    explicit GameStateRepr(const GameState& state) {
        for (const auto& dog : state.dogs) {
            dogs_.emplace_back(dog);
        }
        // Добавьте сериализацию других компонентов
    }

    GameState Restore() const {
        GameState state;
        for (const auto& dog_repr : dogs_) {
            state.dogs.push_back(dog_repr.Restore());
        }
        // Восстановите другие компоненты
        return state;
    }

    template <class Archive>
    void serialize(Archive& ar, unsigned) {
        ar& dogs_;
        // Добавьте сериализацию других полей
    }

private:
    std::vector<serialization::DogRepr> dogs_;
    // Добавьте другие компоненты
};

// Функции сохранения и загрузки
void SaveState(const GameState& state, const std::string& path) {
    std::string temp_path = path + ".tmp";
    {
        std::ofstream ofs(temp_path);
        if (!ofs) {
            throw std::runtime_error("Cannot open file for writing: " + temp_path);
        }
        boost::archive::text_oarchive oa(ofs);
        GameStateRepr repr(state);
        oa << repr;
    }
    std::filesystem::rename(temp_path, path);
}

GameState LoadState(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("Cannot open file for reading: " + path);
    }
    boost::archive::text_iarchive ia(ifs);
    GameStateRepr repr;
    ia >> repr;
    return repr.Restore();
}

// Класс-наблюдатель для автоматического сохранения
class SerializingListener {
public:
    SerializingListener(const std::string& path,
        std::chrono::milliseconds period)
        : path_(path)
        , period_(period)
        , last_save_time_(std::chrono::milliseconds::zero()) {
    }

    void OnTick(std::chrono::milliseconds game_time) {
        if (period_ != std::chrono::milliseconds::max() &&
            game_time - last_save_time_ >= period_) {
            SaveState(state_, path_);
            last_save_time_ = game_time;
            std::cout << "State saved to: " << path_ << std::endl;
        }
    }

    void SaveOnShutdown() {
        SaveState(state_, path_);
        std::cout << "State saved on shutdown to: " << path_ << std::endl;
    }

    void SetState(const GameState& state) {
        state_ = state;
    }

    const GameState& GetState() const {
        return state_;
    }

    // Для тестов
    std::chrono::milliseconds GetLastSaveTime() const {
        return last_save_time_;
    }

private:
    std::string path_;
    std::chrono::milliseconds period_;
    std::chrono::milliseconds last_save_time_;
    GameState state_;
};

// Простой класс приложения (заглушка для тестов)
class Application {
public:
    using TickSignal = boost::signals2::signal<void(std::chrono::milliseconds)>;

    boost::signals2::connection DoOnTick(const TickSignal::slot_type& handler) {
        return tick_signal_.connect(handler);
    }

    void Tick(std::chrono::milliseconds delta) {
        game_time_ += delta;
        tick_signal_(delta);
    }

    void SetState(const GameState& state) {
        state_ = state;
    }

    GameState GetState() const {
        return state_;
    }

    std::chrono::milliseconds GetGameTime() const {
        return game_time_;
    }

private:
    GameState state_;
    TickSignal tick_signal_;
    std::chrono::milliseconds game_time_{ 0 };
};

int main(int argc, char* argv[]) {
    // 1. Парсинг аргументов командной строки
    std::string state_file_path;
    std::optional<std::chrono::milliseconds> save_period;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--state-file" && i + 1 < argc) {
            state_file_path = argv[++i];
        }
        else if (arg == "--save-state-period" && i + 1 < argc) {
            save_period = std::chrono::milliseconds(std::stoll(argv[++i]));
        }
    }

    // 2. Создание приложения и восстановление состояния
    Application app;
    bool should_save = !state_file_path.empty();

    if (should_save && std::filesystem::exists(state_file_path)) {
        try {
            GameState state = LoadState(state_file_path);
            app.SetState(state);
            std::cout << "State loaded from: " << state_file_path << std::endl;
            std::cout << "Loaded " << state.dogs.size() << " dogs" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error loading state: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    }
    else if (should_save) {
        std::cout << "State file not found, starting with empty state" << std::endl;
    }
    else {
        std::cout << "Starting with empty state (no state file)" << std::endl;
    }

    // 3. Создание слушателя для сохранения
    std::unique_ptr<SerializingListener> listener;
    if (should_save) {
        auto period = save_period.value_or(std::chrono::milliseconds::max());
        listener = std::make_unique<SerializingListener>(state_file_path, period);
        listener->SetState(app.GetState());

        // Подписка на тики
        app.DoOnTick([&listener](std::chrono::milliseconds delta) {
            listener->OnTick(delta);
            });
    }

    // 4. Запуск ASIO
    try {
        net::io_context ioc;

        // Создаём несколько тиков для теста (можно заменить на реальный сервер)
        auto tick_timer = std::make_shared<net::steady_timer>(ioc);

        std::function<void()> tick_handler = [&app, tick_timer, &ioc]() {
            app.Tick(std::chrono::milliseconds(1000)); // тик каждую секунду

            // Продолжаем тикать
            tick_timer->expires_after(std::chrono::milliseconds(1000));
            tick_timer->async_wait([&](const sys::error_code& ec) {
                if (!ec) {
                    tick_handler();
                }
                });
            };

        tick_timer->expires_after(std::chrono::milliseconds(1000));
        tick_timer->async_wait([&](const sys::error_code& ec) {
            if (!ec) {
                tick_handler();
            }
            });

        // 5. Подписка на сигналы завершения
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const sys::error_code& ec, int) {
            if (!ec) {
                std::cout << "Shutting down..." << std::endl;
                if (listener) {
                    listener->SetState(app.GetState());
                    listener->SaveOnShutdown();
                }
                ioc.stop();
            }
            });

        std::cout << "Server started. Press Ctrl+C to stop." << std::endl;
        if (should_save) {
            std::cout << "State will be saved to: " << state_file_path << std::endl;
            if (save_period) {
                std::cout << "Auto-save period: " << save_period->count() << " ms" << std::endl;
            }
        }

        // 6. Запуск воркеров
        unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        std::vector<std::thread> workers;
        for (unsigned i = 0; i < num_threads; ++i) {
            workers.emplace_back([&ioc] { ioc.run(); });
        }

        for (auto& t : workers) {
            t.join();
        }

        return EXIT_SUCCESS;

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}