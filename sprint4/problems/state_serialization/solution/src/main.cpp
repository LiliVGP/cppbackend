#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <string>
#include <string_view>
#include <filesystem>
#include <optional>

#include "model.h"
#include "state_serializer.h"  // Добавляем сериализатор

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {

    template <typename Fn>
    void RunWorkers(unsigned n, const Fn& fn) {
        n = std::max(1u, n);
        std::vector<std::jthread> workers;
        workers.reserve(n - 1);
        while (--n) {
            workers.emplace_back(fn);
        }
        fn();
    }

    struct Args {
        std::string config_file;
        std::string www_root;
        std::string state_file;
        std::optional<int> save_state_period_ms;
    };

    Args ParseArgs(int argc, const char* argv[]) {
        Args args;
        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--config-file"sv && i + 1 < argc) {
                args.config_file = argv[++i];
            }
            else if (arg == "--www-root"sv && i + 1 < argc) {
                args.www_root = argv[++i];
            }
            else if (arg == "--state-file"sv && i + 1 < argc) {
                args.state_file = argv[++i];
            }
            else if (arg == "--save-state-period"sv && i + 1 < argc) {
                args.save_state_period_ms = std::stoi(argv[++i]);
            }
        }
        if (args.config_file.empty()) {
            throw std::invalid_argument("Usage: game_server --config-file <config> --www-root <static-dir> [--state-file <file>] [--save-state-period <ms>]"s);
        }
        if (args.www_root.empty()) {
            args.www_root = ".";
        }
        return args;
    }

}  // namespace

int main(int argc, const char* argv[]) {
    try {
        Args args = ParseArgs(argc, argv);

        extra_data::GameExtraData extra_data;

        model::Game game = json_loader::LoadGame(args.config_file, extra_data);

        // Создаём сериализатор, если указан state-file
        std::unique_ptr<infra::StateSerializer> serializer;
        if (!args.state_file.empty()) {
            std::optional<app::Milliseconds> save_period;
            if (args.save_state_period_ms.has_value() && args.save_state_period_ms.value() > 0) {
                save_period = app::Milliseconds(args.save_state_period_ms.value());
            }

            // Создаём сериализатор
            serializer = std::make_unique<infra::StateSerializer>(
                game,                  // игра
                std::filesystem::path(args.state_file),
                save_period
            );
            // Запускаем автосохранение
            serializer->Start();
            std::cout << "State serialization enabled, file: " << args.state_file << std::endl;
            if (save_period.has_value()) {
                std::cout << "Save period: " << save_period->count() << " ms" << std::endl;
            }
        }

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, &serializer](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                std::cout << "Signal received, stopping..." << std::endl;
                // Сохраняем состояние перед остановкой
                if (serializer) {
                    std::cout << "Saving state before exit..." << std::endl;
                    serializer->SaveNow();
                }
                ioc.stop();
            }
            });

        http_handler::RequestHandler handler{ game, extra_data, args.www_root };

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, { address, port }, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
            });

        std::cout << "Server has started..."sv << std::endl;

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
            });

        // После остановки io_context сохраняем состояние (если не было сохранено в обработчике сигналов)
        if (serializer) {
            // Если обработчик сигналов не вызван, всё равно сохраняем
            std::cout << "Saving final state..." << std::endl;
            serializer->SaveNow();
        }

        std::cout << "Server stopped" << std::endl;

    }
    catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}