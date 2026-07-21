#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <chrono>
#include <random>

#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "ticker.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace po = boost::program_options;

struct Args {
    int tick_period = 0;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    po::options_description desc{"Usage: game_server [options]"};

    Args args;
    desc.add_options()
        ("help,h", "Show this help message")
        ("tick-period,t", po::value(&args.tick_period)->value_name("ms"), "Set tick period in milliseconds")
        ("config-file,c", po::value(&args.config_file)->required()->value_name("file"), "Set config file path")
        ("www-root,w", po::value(&args.www_root)->required()->value_name("dir"), "Set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "Spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.contains("help")) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    po::notify(vm);
    return args;
}

int main(int argc, const char* argv[]) {
    try {
        auto args = ParseCommandLine(argc, argv);
        if (!args) {
            return EXIT_SUCCESS;
        }

        model::Game game;
        json_loader::LoadGame(args->config_file, game);

        std::cout << "Loaded " << game.GetMaps().size() << " maps from " << args->config_file << std::endl;
        for (const auto& map : game.GetMaps()) {
            std::cout << " - " << *map.GetId() << ": " << map.GetName() << std::endl;
        }

        if (args->randomize_spawn_points) {
            std::cout << "Random spawn points enabled" << std::endl;
        }

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);
        auto api_strand = net::make_strand(ioc);

        std::filesystem::path static_root = args->www_root;
        bool tick_enabled = (args->tick_period > 0);
        http_handler::RequestHandler handler{ game, static_root, tick_enabled };

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, { address, port }, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        std::shared_ptr<Ticker> ticker;
        if (tick_enabled) {
            auto period = std::chrono::milliseconds(args->tick_period);
            ticker = std::make_shared<Ticker>(api_strand, period,
                [&game](std::chrono::milliseconds delta) {
                    auto& players = game.GetPlayerManager().GetPlayers();
                    double delta_seconds = delta.count() / 1000.0;
                    for (auto& player : players) {
                        model::Map* map = player.GetMap();
                        if (map) {
                            player.UpdatePosition(delta_seconds, *map);
                        }
                    }
                });
            ticker->Start();
            std::cout << "Auto-tick enabled: period = " << args->tick_period << " ms" << std::endl;
        } else {
            std::cout << "Auto-tick disabled, time controlled by /api/v1/game/tick" << std::endl;
        }

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        std::cout << "Server has started on port " << port << "..."sv << std::endl;

        auto worker = [&ioc]() { ioc.run(); };
        std::vector<std::jthread> workers;
        workers.reserve(num_threads - 1);
        for (unsigned i = 0; i < num_threads - 1; ++i) {
            workers.emplace_back(worker);
        }
        worker();

        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
