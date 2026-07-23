#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/signals2.hpp>
#include <iostream>
#include <filesystem>
#include <thread>
#include <vector>
#include <memory>

#include "model.h"
#include "model_serialization.h"

namespace po = boost::program_options;
namespace net = boost::asio;
namespace sys = boost::system;

// Функция для запуска воркеров
void RunWorkers(unsigned num_threads, std::function<void()> worker_func) {
    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    for (unsigned i = 0; i < num_threads; ++i) {
        workers.emplace_back(worker_func);
    }

    for (auto& worker : workers) {
        worker.join();
    }
}

int main(int argc, char* argv[]) {
    try {
        // Парсинг аргументов командной строки
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "Show help")
            ("state-file", po::value<std::string>(), "Path to state file for saving/loading game state")
            ("save-state-period", po::value<int>(), "Save state period in milliseconds (ignored without --state-file)")
            ("tick-period", po::value<int>(), "Automatic tick period in milliseconds")
            ("num-threads", po::value<unsigned>()->default_value(4), "Number of worker threads")
            ("port", po::value<uint16_t>()->default_value(8080), "HTTP server port")
            // Добавьте другие параметры вашего сервера
            ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        }

        // Параметры состояния
        std::string state_file;
        std::chrono::milliseconds save_period{ 0 };
        std::chrono::milliseconds tick_period{ 0 };

        if (vm.count("state-file")) {
            state_file = vm["state-file"].as<std::string>();
            if (vm.count("save-state-period")) {
                save_period = std::chrono::milliseconds(vm["save-state-period"].as<int>());
            }
        }

        if (vm.count("tick-period")) {
            tick_period = std::chrono::milliseconds(vm["tick-period"].as<int>());
        }

        unsigned num_threads = vm["num-threads"].as<unsigned>();
        uint16_t port = vm["port"].as<uint16_t>();

        // Инициализация приложения
        model::Application app;

        // Инициализация менеджера состояния
        model::StateManager state_manager(app.GetGameModel(), state_file, save_period);

        // Подключаем менеджер состояния к сигналам приложения
        boost::signals2::connection tick_connection = app.DoOnTick([&state_manager](std::chrono::milliseconds delta) {
            state_manager.OnTick(delta);
            });

        boost::signals2::connection save_connection = app.DoOnSave([&state_manager]() {
            state_manager.OnSave();
            });

        // Восстановление состояния из файла
        if (!state_file.empty()) {
            if (std::filesystem::exists(state_file)) {
                std::cout << "Loading state from file: " << state_file << std::endl;
                try {
                    if (!state_manager.TryLoadState()) {
                        std::cerr << "Failed to load state from file" << std::endl;
                        return EXIT_FAILURE;
                    }
                    std::cout << "State successfully restored" << std::endl;
                }
                catch (const std::exception& e) {
                    std::cerr << "Error restoring state: " << e.what() << std::endl;
                    return EXIT_FAILURE;
                }
            }
            else {
                std::cout << "State file does not exist, starting with clean state" << std::endl;
            }
        }
        else {
            std::cout << "No state file specified, starting with clean state" << std::endl;
        }

        // Создание и настройка ASIO io_context
        net::io_context ioc(num_threads);

        // Подписываемся на сигналы завершения
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, &state_manager](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                std::cout << "Received shutdown signal, saving state..." << std::endl;
                state_manager.OnSave();
                ioc.stop();
            }
            });

        // Если задан tick-period, запускаем автоматические тики
        boost::asio::steady_timer tick_timer(ioc);
        if (tick_period.count() > 0) {
            std::function<void(const sys::error_code&)> tick_handler;
            tick_handler = [&app, &tick_timer, tick_period, &tick_handler](const sys::error_code& ec) {
                if (!ec) {
                    app.Tick(tick_period);
                    tick_timer.expires_after(tick_period);
                    tick_timer.async_wait(tick_handler);
                }
                };
            tick_timer.expires_after(tick_period);
            tick_timer.async_wait(tick_handler);
            std::cout << "Automatic ticks enabled with period: " << tick_period.count() << "ms" << std::endl;
        }

        // Здесь должен быть код для запуска HTTP-сервера
        // Например:
        // auto server = std::make_shared<http::Server>(ioc, port, app);
        // server->Start();

        std::cout << "Server started on port " << port << " with " << num_threads << " threads" << std::endl;
        std::cout << "State file: " << (state_file.empty() ? "none" : state_file) << std::endl;
        std::cout << "Save period: " << (save_period.count() > 0 ? std::to_string(save_period.count()) + "ms" : "only on shutdown") << std::endl;

        // Запуск воркеров
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
            });

        // После завершения ioc.run() все асинхронные операции завершены
        // Сохраняем состояние (если не сделали это при обработке сигнала)
        if (!state_file.empty()) {
            std::cout << "Saving state before exit..." << std::endl;
            state_manager.OnSave();
        }

        std::cout << "Server stopped" << std::endl;
        return EXIT_SUCCESS;

    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}