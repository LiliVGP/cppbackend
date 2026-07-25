#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <boost/signals2.hpp>
#include "model.h"

namespace infra {

    using Milliseconds = std::chrono::milliseconds;

    class StateSerializer {
    public:
        StateSerializer(model::Game& game,
            std::filesystem::path state_file,
            std::optional<Milliseconds> save_period);

        // Запускает подписку на тики (если задан период)
        void Start();

        // Принудительное сохранение (для сигналов завершения)
        void SaveNow();

    private:
        model::Game& game_;
        std::filesystem::path state_file_;
        std::optional<Milliseconds> save_period_;
        Milliseconds elapsed_since_last_save_{ 0 };
        boost::signals2::scoped_connection tick_connection_;

        void OnTick(Milliseconds delta);
        void SaveState();
        void LoadState();
    };

} // namespace infra