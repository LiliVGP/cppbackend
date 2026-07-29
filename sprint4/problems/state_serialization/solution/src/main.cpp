#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <iostream>
#include <thread>
#include <string>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <chrono>

#include "json_loader.h"
#include "request_handler.h"
#include "extra_data.h"

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
    int save_state_period_ms = 0;
};

Args ParseArgs(int argc, const char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--config-file"sv && i + 1 < argc) {
            args.config_file = argv[++i];
        } else if (arg == "--www-root"sv && i + 1 < argc) {
            args.www_root = argv[++i];
        } else if (arg == "--state-file"sv && i + 1 < argc) {
            args.state_file = argv[++i];
        } else if (arg == "--save-state-period"sv && i + 1 < argc) {
            args.save_state_period_ms = std::stoi(argv[++i]);
        }
    }
    if (args.config_file.empty()) {
        throw std::invalid_argument("Usage: game_server --config-file <config> --www-root <static-dir>"s);
    }
    if (args.www_root.empty()) {
        args.www_root = ".";
    }
    return args;
}

// Save game state to file using atomic write (temp file + rename)
void SaveGameState(const model::Game& game, const std::filesystem::path& state_path) {
    std::filesystem::path temp_path = state_path;
    temp_path += ".tmp";

    std::ofstream ofs(temp_path);
    if (!ofs) {
        throw std::runtime_error("Failed to open state file for writing: " + state_path.string());
    }

    boost::archive::text_oarchive archive(ofs);

    // Save default dog speed
    archive << game.GetDefaultDogSpeed();

    // Save default bag capacity
    archive << game.GetDefaultBagCapacity();

    // Save next player id
    archive << game.GetNextPlayerId();

    // Save loot generator config
    archive << game.GetLootPeriodMs();
    archive << game.GetLootProbability();

    // Save tokens with player mapping
    const auto& token_map = game.GetTokenToPlayer();
    archive << token_map.size();
    for (const auto& [token, info] : token_map) {
        archive << token;
        archive << *info.first;  // map_id string
        archive << info.second;  // player_id
    }

    // Save all players (full state)
    const auto& all_players = game.GetAllPlayers();
    archive << all_players.size();
    for (const auto& [pid, player] : all_players) {
        archive << pid;
        archive << player.GetName();
        auto pos = player.GetPosition();
        archive << pos.x << pos.y;
        auto speed = player.GetSpeed();
        archive << speed.dx << speed.dy;
        archive << static_cast<int>(player.GetDirection());
        const auto& bag = player.GetBag();
        archive << bag.size();
        for (const auto& item : bag) {
            archive << item.id << item.type;
        }
        archive << player.GetScore();
    }

    // Save lost objects per map
    const auto& all_loot = game.GetAllLostObjects();
    archive << all_loot.size();
    for (const auto& [map_id, lost_objects] : all_loot) {
        archive << *map_id;
        archive << lost_objects.size();
        for (const auto& obj : lost_objects) {
            archive << obj.id << obj.type << obj.pos.x << obj.pos.y;
        }
    }

    // Atomic rename
    std::error_code ec;
    std::filesystem::rename(temp_path, state_path, ec);
    if (ec) {
        throw std::runtime_error("Failed to rename temp state file: " + ec.message());
    }
}

// Load game state from file
void LoadGameState(model::Game& game, const std::filesystem::path& state_path) {
    std::ifstream ifs(state_path);
    if (!ifs) {
        throw std::runtime_error("Failed to open state file for reading: " + state_path.string());
    }

    boost::archive::text_iarchive archive(ifs);

    // Load default dog speed
    double default_dog_speed = 0.0;
    archive >> default_dog_speed;
    game.SetDefaultDogSpeed(default_dog_speed);

    // Load default bag capacity
    size_t default_bag_capacity = 0;
    archive >> default_bag_capacity;
    game.SetDefaultBagCapacity(default_bag_capacity);

    // Load next player id
    int next_player_id = 0;
    archive >> next_player_id;
    game.SetNextPlayerId(next_player_id);

    // Load loot generator config
    int loot_period = 0;
    double loot_prob = 0.0;
    archive >> loot_period >> loot_prob;
    game.SetLootGeneratorConfig(loot_period / 1000.0, loot_prob);

    // Load tokens with player mapping
    size_t token_count = 0;
    archive >> token_count;
    for (size_t i = 0; i < token_count; ++i) {
        std::string token;
        std::string map_id_str;
        int player_id = 0;
        archive >> token >> map_id_str >> player_id;
        game.RestoreToken(token, model::Map::Id{map_id_str}, player_id);
    }

    // Load all players (full state)
    size_t player_count = 0;
    archive >> player_count;
    for (size_t i = 0; i < player_count; ++i) {
        int pid = 0;
        std::string name;
        double px, py, sx, sy;
        int dir_int;
        archive >> pid >> name >> px >> py >> sx >> sy >> dir_int;

        size_t bag_size = 0;
        archive >> bag_size;
        std::vector<model::BagItem> bag;
        bag.reserve(bag_size);
        for (size_t j = 0; j < bag_size; ++j) {
            int item_id, item_type;
            archive >> item_id >> item_type;
            bag.push_back(model::BagItem{item_id, item_type});
        }

        int score = 0;
        archive >> score;

        // Find which map this player belongs to via token mapping
        model::Map::Id map_id{""};
        const auto& token_map = game.GetTokenToPlayer();
        for (const auto& [token, info] : token_map) {
            if (info.second == pid) {
                map_id = info.first;
                break;
            }
        }

        game.RestorePlayer(pid, name, map_id,
                           model::PlayerPosition{px, py},
                           model::PlayerSpeed{sx, sy},
                           static_cast<model::PlayerDirection>(dir_int),
                           bag, score);
    }

    // Load lost objects per map
    size_t loot_map_count = 0;
    archive >> loot_map_count;
    for (size_t i = 0; i < loot_map_count; ++i) {
        std::string map_id_str;
        size_t obj_count = 0;
        archive >> map_id_str >> obj_count;
        std::vector<model::LostObject> objects;
        objects.reserve(obj_count);
        for (size_t j = 0; j < obj_count; ++j) {
            int id, type;
            double ox, oy;
            archive >> id >> type >> ox >> oy;
            objects.push_back(model::LostObject{id, type, model::PlayerPosition{ox, oy}});
        }
        game.RestoreLostObjects(model::Map::Id{map_id_str}, objects);
    }
}

}  // namespace

// Global save callback for auto-save on ticks
static std::function<void()> g_save_callback;
static int g_save_state_period_ms = 0;
static int g_last_save_game_time_ms = 0;

void SetSaveCallback(std::function<void()> save_cb, int save_period_ms) {
    g_save_callback = std::move(save_cb);
    g_save_state_period_ms = save_period_ms;
}

void OnTick(int game_time_ms) {
    if (!g_save_callback || g_save_state_period_ms <= 0) {
        return;
    }
    
    if (game_time_ms - g_last_save_game_time_ms >= g_save_state_period_ms) {
        try {
            g_save_callback();
            g_last_save_game_time_ms = game_time_ms;
        } catch (const std::exception& ex) {
            std::cerr << "Auto-save failed: " << ex.what() << std::endl;
        }
    }
}

int main(int argc, const char* argv[]) {
    try {
        Args args = ParseArgs(argc, argv);

        extra_data::GameExtraData extra_data;
        model::Game game = json_loader::LoadGame(args.config_file, extra_data);

        if (!args.state_file.empty()) {
            std::filesystem::path state_path(args.state_file);
            if (std::filesystem::exists(state_path)) {
                try {
                    LoadGameState(game, state_path);
                } catch (const std::exception& ex) {
                    std::cerr << "Failed to load state file: " << ex.what() << std::endl;
                    return EXIT_FAILURE;
                }
            }
        }

        // Set up save callback
        if (!args.state_file.empty()) {
            std::filesystem::path state_path(args.state_file);
            SetSaveCallback([&game, state_path]() {
                SaveGameState(game, state_path);
            }, args.save_state_period_ms);
        }

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        http_handler::RequestHandler handler{game, extra_data, args.www_root, OnTick};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        std::cout << "server started"sv << std::endl;

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        if (!args.state_file.empty()) {
            try {
                SaveGameState(game, args.state_file);
                std::cout << "State saved to " << args.state_file << std::endl;
            } catch (const std::exception& ex) {
                std::cerr << "Failed to save state: " << ex.what() << std::endl;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
