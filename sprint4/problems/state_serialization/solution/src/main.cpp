#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <filesystem>
#include <csignal>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "application.h"
#include "model.h"
#include "state_serializer.h"

namespace net = boost::asio;
namespace sys = boost::system;

// Глобальные переменные для доступа к сериализатору (упрощённо)
std::unique_ptr<infra::StateSerializer> g_serializer;

void signal_handler(int signal) {
    if (g_serializer) {
        std::cout << "Saving state before exit..." << std::endl;
        g_serializer->SaveNow();
    }
    std::exit(0);
}

int main(int argc, char* argv[]) {
    try {
        // Парсинг аргументов командной строки
        std::string state_file_path;
        int save_period_ms = 0;

        // Простой парсинг (можно заменить на boost::program_options)
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--state-file" && i + 1 < argc) {
                state_file_path = argv[++i];
            }
            else if (arg == "--save-state-period" && i + 1 < argc) {
                save_period_ms = std::stoi(argv[++i]);
            }
        }

        // Загрузка карт из конфига (здесь упрощённо)
        std::vector<model::Map> maps;
        maps.emplace_back(model::Map::Id{ "map1" }, "Map 1");
        maps.emplace_back(model::Map::Id{ "town" }, "Town");
        maps.emplace_back(model::Map::Id{ "map3" }, "Map 3");

        // Создание приложения
        app::Application app(std::move(maps));

        // Настройка сериализатора, если указан файл состояния
        if (!state_file_path.empty()) {
            std::optional<app::Milliseconds> period;
            if (save_period_ms > 0) {
                period = app::Milliseconds(save_period_ms);
            }
            g_serializer = std::make_unique<infra::StateSerializer>(
                app, std::filesystem::path(state_file_path), period);
            g_serializer->Start();
        }

        // Установка обработчиков сигналов
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        // Запуск сетевого сервера (упрощённо)
        net::io_context ioc(1);
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const sys::error_code& ec, int signal) {
            if (!ec) {
                ioc.stop();
            }
            });

        std::cout << "Server started. Press Ctrl+C to exit." << std::endl;
        ioc.run();

        // После остановки io_context сохраняем состояние
        if (g_serializer) {
            g_serializer->SaveNow();
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}