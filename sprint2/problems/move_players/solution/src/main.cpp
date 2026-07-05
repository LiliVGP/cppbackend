#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>
#include <filesystem>

#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"

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
}

int main(int argc, const char* argv[]) {
    try {
        std::string config_path;
        std::string static_root_str;

        // Приоритет: аргумент командной строки > переменная окружения CONFIG_PATH
        if (argc >= 2) {
            config_path = argv[1];
        } else {
            const char* env_config = std::getenv("CONFIG_PATH");
            if (env_config) {
                config_path = env_config;
            } else {
                std::cerr << "Error: config path not provided. Set CONFIG_PATH environment variable."sv << std::endl;
                return EXIT_FAILURE;
            }
        }

        if (argc >= 3) {
            static_root_str = argv[2];
        } else {
            const char* env_static = std::getenv("STATIC_ROOT");
            if (env_static) {
                static_root_str = env_static;
            } else {
                std::cerr << "Error: static root not provided. Set STATIC_ROOT environment variable."sv << std::endl;
                return EXIT_FAILURE;
            }
        }

        model::Game game;
        json_loader::LoadGame(config_path, game);

        // Отладочный вывод — увидим, сколько карт загружено
        std::cout << "Loaded " << game.GetMaps().size() << " maps from " << config_path << std::endl;
        for (const auto& map : game.GetMaps()) {
            std::cout << " - " << *map.GetId() << ": " << map.GetName() << std::endl;
        }

        std::filesystem::path static_root = static_root_str;

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        http_handler::RequestHandler handler{ game, static_root };

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, { address, port }, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        std::cout << "Server has started..."sv << std::endl;

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    }
    catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
