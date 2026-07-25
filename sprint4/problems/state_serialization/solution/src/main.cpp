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
namespace po = boost::program_options;

// Глобальные переменные для доступа к сериализатору
std::unique_ptr<infra::StateSerializer> g_serializer;
net::io_context* g_ioc = nullptr;

void signal_handler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    if (g_serializer) {
        std::cout << "Saving state before exit..." << std::endl;
        g_serializer->SaveNow();
    }
    if (g_ioc) {
        g_ioc->stop();
    }
}

int main(int argc, char* argv[]) {
    try {
        // Парсинг аргументов командной строки
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "produce help message")
            ("config-file,c", po::value<std::string>(), "path to config file")
            ("www-root,w", po::value<std::string>(), "path to static files directory")
            ("state-file", po::value<std::string>(), "path to state file")
            ("save-state-period", po::value<int>(), "period in milliseconds for auto-save")
            ("tick-period", po::value<int>(), "period in milliseconds for auto-tick");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        // Проверка обязательных параметров
        if (!vm.count("config-file")) {
            std::cerr << "Error: --config-file is required" << std::endl;
            return 1;
        }
        if (!vm.count("www-root")) {
            std::cerr << "Error: --www-root is required" << std::endl;
            return 1;
        }

        std::string config_file = vm["config-file"].as<std::string>();
        std::string www_root = vm["www-root"].as<std::string>();

        // Проверяем существование файла конфигурации
        if (!std::filesystem::exists(config_file)) {
            std::cerr << "Error: config file not found: " << config_file << std::endl;
            return 1;
        }

        // Проверяем существование директории www-root
        if (!std::filesystem::exists(www_root)) {
            std::cerr << "Error: www-root directory not found: " << www_root << std::endl;
            return 1;
        }

        // Загрузка карт из конфига (здесь упрощённо)
        std::vector<model::Map> maps;
        // В реальном проекте нужно загружать из JSON
        maps.emplace_back(model::Map::Id{ "map1" }, "Map 1");
        maps.emplace_back(model::Map::Id{ "town" }, "Town");
        maps.emplace_back(model::Map::Id{ "map3" }, "Map 3");

        // Создание приложения
        app::Application app(std::move(maps));

        // Настройка сериализатора, если указан файл состояния
        std::string state_file_path;
        std::optional<app::Milliseconds> save_period;

        if (vm.count("state-file")) {
            state_file_path = vm["state-file"].as<std::string>();
            if (vm.count("save-state-period")) {
                int period_ms = vm["save-state-period"].as<int>();
                if (period_ms > 0) {
                    save_period = app::Milliseconds(period_ms);
                }
            }

            // Создаём сериализатор
            g_serializer = std::make_unique<infra::StateSerializer>(
                app, std::filesystem::path(state_file_path), save_period);

            // Запускаем автосохранение
            g_serializer->Start();
        }

        // Установка обработчиков сигналов
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        // Запуск сетевого сервера
        net::io_context ioc(4); // 4 потока
        g_ioc = &ioc;

        // Создаём HTTP-сервер
        // В реальном проекте здесь должен быть ваш HTTP-сервер
        // Например, с использованием Boost.Beast или вашего фреймворка

        std::cout << "Starting server on port 8080..." << std::endl;
        std::cout << "Config file: " << config_file << std::endl;
        std::cout << "WWW root: " << www_root << std::endl;
        if (!state_file_path.empty()) {
            std::cout << "State file: " << state_file_path << std::endl;
            if (save_period) {
                std::cout << "Save period: " << save_period->count() << " ms" << std::endl;
            }
        }

        // Запускаем обработку сигналов
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const sys::error_code& ec, int signal) {
            if (!ec) {
                std::cout << "Signal received, stopping..." << std::endl;
                ioc.stop();
            }
            });

        // Запускаем несколько потоков для обработки запросов
        std::vector<std::thread> threads;
        unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        for (unsigned i = 0; i < num_threads; ++i) {
            threads.emplace_back([&ioc]() {
                ioc.run();
                });
        }

        // Ждём завершения всех потоков
        for (auto& t : threads) {
            t.join();
        }

        // После остановки io_context сохраняем состояние
        if (g_serializer) {
            std::cout << "Saving final state..." << std::endl;
            g_serializer->SaveNow();
        }

        std::cout << "Server stopped normally" << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}