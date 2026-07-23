#pragma once
#include <boost/signals2.hpp>
#include <chrono>
#include <memory>
#include "model.h"

namespace app {

    using Milliseconds = std::chrono::milliseconds;

    class Application {
    public:
        using TickSignal = boost::signals2::signal<void(Milliseconds delta)>;
        using SaveSignal = boost::signals2::signal<void()>;

        Application() = default;

        // Подписка на сигналы
        boost::signals2::connection DoOnTick(const TickSignal::slot_type& handler) {
            return tick_signal_.connect(handler);
        }

        boost::signals2::connection DoOnSave(const SaveSignal::slot_type& handler) {
            return save_signal_.connect(handler);
        }

        // Основной метод обновления времени
        void Tick(Milliseconds delta) {
            // Здесь обновление игрового состояния...

            // Уведомляем подписчиков о тике
            tick_signal_(delta);
        }

        // Сохранение состояния
        void SaveState() {
            save_signal_();
        }

        // Доступ к игровой модели
        model::GameModel& GetGameModel() { return game_model_; }
        const model::GameModel& GetGameModel() const { return game_model_; }

    private:
        TickSignal tick_signal_;
        SaveSignal save_signal_;
        model::GameModel game_model_;
    };

}  // namespace app