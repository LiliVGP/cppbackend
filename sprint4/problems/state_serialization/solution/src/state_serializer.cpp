#include "state_serializer.h"
#include "model_serialization.h"
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <fstream>
#include <iostream>

namespace infra {

    StateSerializer::StateSerializer(model::Game& game,
        std::filesystem::path state_file,
        std::optional<app::Milliseconds> save_period)
        : game_(game), state_file_(std::move(state_file)), save_period_(save_period) {
        // Если файл существует, восстанавливаем состояние
        if (std::filesystem::exists(state_file_)) {
            try {
                LoadState();
                std::cout << "State restored from: " << state_file_ << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "Error restoring state: " << e.what() << std::endl;
                std::exit(EXIT_FAILURE);
            }
        }
    }

    void StateSerializer::Start() {
        // Подписываемся на тики только если задан период автосохранения
        if (save_period_.has_value()) {
            // В вашем проекте нужно получать тики из Application или Game
            // Можно использовать сигналы или вызывать OnTick из main.cpp
            // Пока оставляем заглушку
        }
    }

    void StateSerializer::OnTick(app::Milliseconds delta) {
        elapsed_since_last_save_ += delta;
        if (elapsed_since_last_save_ >= *save_period_) {
            SaveState();
            elapsed_since_last_save_ = app::Milliseconds{ 0 };
        }
    }

    void StateSerializer::SaveNow() {
        SaveState();
    }

    void StateSerializer::SaveState() {
        // Создаём временный файл
        auto tmp_path = state_file_;
        tmp_path += ".tmp";

        std::ofstream ofs(tmp_path);
        if (!ofs) {
            std::cerr << "Cannot open temporary state file: " << tmp_path << std::endl;
            return;
        }

        try {
            serialization::GameStateRepr repr = game_.GetGameStateRepr();
            boost::archive::text_oarchive oa(ofs);
            oa << repr;
            ofs.close();

            // Атомарно переименовываем временный файл в целевой
            std::error_code ec;
            std::filesystem::rename(tmp_path, state_file_, ec);
            if (ec) {
                std::cerr << "Failed to rename state file: " << ec.message() << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error during state serialization: " << e.what() << std::endl;
        }
    }

    void StateSerializer::LoadState() {
        std::ifstream ifs(state_file_);
        if (!ifs) {
            throw std::runtime_error("Cannot open state file: " + state_file_.string());
        }

        serialization::GameStateRepr repr;
        boost::archive::text_iarchive ia(ifs);
        ia >> repr;

        game_.RestoreFromRepr(repr);
    }

} // namespace infra