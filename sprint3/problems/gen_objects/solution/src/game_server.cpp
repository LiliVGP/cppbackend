#include "game_server.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>
#include <signal.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

std::atomic<bool> stop_server{ false };

void signal_handler(int) {
    stop_server = true;
}

GameServer::GameServer(const std::string& config_file) {
    try {
        std::cout << "Starting server with config: " << config_file << std::endl;
        config_ = std::make_unique<ConfigLoader>(ConfigLoader::LoadFromFile(config_file));
        std::cout << "Config loaded successfully" << std::endl;

        InitializeMaps();
        InitializeGameState();
        std::cout << "Game server initialized successfully!" << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "FATAL ERROR: " << e.what() << std::endl;
        throw std::runtime_error("Failed to initialize game server: " + std::string(e.what()));
    }
}

void GameServer::InitializeMaps() {
    const auto& map_infos = config_->GetMaps();
    if (map_infos.empty()) {
        std::cout << "Warning: No maps loaded from config!" << std::endl;
    }

    for (const auto& map_info : map_infos) {
        auto map = std::make_unique<Map>(
            map_info.id,
            map_info.name,
            map_info.loot_types.size()
        );
        for (const auto& road : map_info.roads) map->AddRoad(road);
        for (const auto& building : map_info.buildings) map->AddBuilding(building);
        for (const auto& office : map_info.offices) map->AddOffice(office);

        maps_[map_info.id] = std::move(map);
        map_infos_[map_info.id] = map_info;
    }
    std::cout << "Loaded " << maps_.size() << " maps" << std::endl;
}

void GameServer::InitializeGameState() {
    auto& gen_config = config_->GetLootGeneratorConfig();
    std::cout << "Loot generator config: period=" << gen_config.period.count()
        << "ms, probability=" << gen_config.probability << std::endl;

    auto loot_gen = std::make_shared<loot_gen::LootGenerator>(
        gen_config.period,
        gen_config.probability
    );
    auto rng = std::make_unique<std::mt19937>(std::random_device{}());
    game_state_ = std::make_unique<GameState>(loot_gen, std::move(rng));
    if (!maps_.empty()) {
        game_state_->SetCurrentMap(maps_.begin()->second.get());
        std::cout << "Current map set to: " << maps_.begin()->first << std::endl;
    }
}

const Map* GameServer::GetMap(const std::string& id) const {
    auto it = maps_.find(id);
    if (it == maps_.end()) return nullptr;
    return it->second.get();
}

void GameServer::AddPlayer(PlayerId id, const GameState::Player& player) {
    game_state_->AddPlayer(id, player);
}

void GameServer::ProcessTick(std::chrono::milliseconds delta) {
    time_ += delta;
    game_state_->GenerateLoot(delta);
}

boost::json::object GameServer::GetMapInfo(const std::string& id) const {
    auto it = map_infos_.find(id);
    if (it == map_infos_.end()) return {};
    const auto& info = it->second;

    boost::json::object obj;
    obj["id"] = info.id;
    obj["name"] = info.name;

    auto to_json_number = [](double val) -> boost::json::value {
        if (val == static_cast<int>(val)) {
            return boost::json::value(static_cast<int>(val));
        }
        return boost::json::value(val);
    };

    boost::json::array roads;
    for (const auto& r : info.roads) {
        boost::json::object road_obj;
        road_obj["x0"] = to_json_number(r.x0);
        road_obj["y0"] = to_json_number(r.y0);
        if (r.x0 != r.x1) {
            road_obj["x1"] = to_json_number(r.x1);
        } else {
            road_obj["y1"] = to_json_number(r.y1);
        }
        roads.push_back(road_obj);
    }
    obj["roads"] = roads;

    boost::json::array buildings;
    for (const auto& b : info.buildings) {
        boost::json::object building_obj;
        building_obj["x"] = to_json_number(b.position.x);
        building_obj["y"] = to_json_number(b.position.y);
        building_obj["w"] = to_json_number(b.width);
        building_obj["h"] = to_json_number(b.height);
        buildings.push_back(building_obj);
    }
    obj["buildings"] = buildings;

    boost::json::array offices;
    for (const auto& o : info.offices) {
        boost::json::object office_obj;
        office_obj["id"] = o.id;
        office_obj["x"] = to_json_number(o.position.x);
        office_obj["y"] = to_json_number(o.position.y);
        office_obj["offsetX"] = to_json_number(o.offset_x);
        office_obj["offsetY"] = to_json_number(o.offset_y);
        offices.push_back(office_obj);
    }
    obj["offices"] = offices;

    obj["lootTypes"] = info.loot_types;

    return obj;
}

boost::json::object GameServer::GetGameState() const {
    boost::json::object response;

    boost::json::object players_obj;
    for (const auto& [id, player] : game_state_->GetPlayers()) {
        boost::json::object p;
        p["pos"] = boost::json::array{ player.position.x, player.position.y };
        p["speed"] = boost::json::array{ player.speed.x, player.speed.y };
        std::string dir;
        switch (player.direction) {
        case GameState::Direction::U: dir = "U"; break;
        case GameState::Direction::D: dir = "D"; break;
        case GameState::Direction::L: dir = "L"; break;
        case GameState::Direction::R: dir = "R"; break;
        }
        p["dir"] = dir;
        players_obj[std::to_string(id.GetId())] = p;
    }
    response["players"] = players_obj;

    boost::json::object loot_obj;
    for (const auto& [id, loot] : game_state_->GetLoot()) {
        boost::json::object l;
        l["type"] = static_cast<int>(loot.type.GetId());
        l["pos"] = boost::json::array{ loot.position.x, loot.position.y };
        loot_obj[std::to_string(id.GetId())] = l;
    }
    response["lostObjects"] = loot_obj;

    return response;
}

// ========= СИНХРОННЫЙ HTTP-СЕРВЕР =========

void HandleRequest(tcp::socket& socket, std::shared_ptr<GameServer> server) {
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    
    try {
        // Читаем запрос (синхронно)
        beast::error_code ec;
        http::read(socket, buffer, req, ec);
        if (ec) {
            std::cerr << "Read error: " << ec.message() << std::endl;
            return;
        }

        http::response<http::string_body> res;
        res.version(req.version());
        res.set(http::field::cache_control, "no-cache");
        res.set(http::field::content_type, "application/json");
        res.result(http::status::ok);

        std::string target = req.target().to_string();
        std::cout << "Request: " << req.method_string() << " " << target << std::endl;

        // GET и HEAD запросы
        if (req.method() == http::verb::get || req.method() == http::verb::head) {
            if (target == "/api/v1/game/state" || target == "api/v1/game/state") {
                auto json_obj = server->GetGameState();
                if (req.method() == http::verb::head) {
                    res.set(http::field::content_length, std::to_string(boost::json::serialize(json_obj).size()));
                } else {
                    res.body() = boost::json::serialize(json_obj);
                    res.set(http::field::content_length, std::to_string(res.body().size()));
                }
            }
            else if (target.rfind("/api/v1/maps/", 0) == 0 || target.rfind("api/v1/maps/", 0) == 0) {
                std::string map_id = target.substr(target.rfind('/') + 1);
                auto json_obj = server->GetMapInfo(map_id);
                if (json_obj.empty()) {
                    res.result(http::status::not_found);
                    boost::json::object err;
                    err["code"] = "mapNotFound";
                    err["message"] = "Map not found";
                    if (req.method() == http::verb::head) {
                        res.set(http::field::content_length, std::to_string(boost::json::serialize(err).size()));
                    } else {
                        res.body() = boost::json::serialize(err);
                        res.set(http::field::content_length, std::to_string(res.body().size()));
                    }
                } else {
                    if (req.method() == http::verb::head) {
                        res.set(http::field::content_length, std::to_string(boost::json::serialize(json_obj).size()));
                    } else {
                        res.body() = boost::json::serialize(json_obj);
                        res.set(http::field::content_length, std::to_string(res.body().size()));
                    }
                }
            }
            else {
                res.result(http::status::not_found);
                res.set(http::field::content_length, "0");
            }
        }
        // POST запросы
        else if (req.method() == http::verb::post) {
            if (target == "/api/v1/game/tick" || target == "api/v1/game/tick") {
                try {
                    auto body = boost::json::parse(req.body()).as_object();
                    if (!body.contains("timeDelta")) {
                        throw std::runtime_error("Missing timeDelta field");
                    }
                    int delta = body.at("timeDelta").as_int64();
                    
                    server->ProcessTick(std::chrono::milliseconds(delta));
                    
                    // Возвращаем пустой объект JSON
                    res.result(http::status::ok);
                    res.body() = "{}";
                    res.set(http::field::content_length, "2");
                    
                    std::cout << "Tick processed: delta=" << delta << "ms" << std::endl;
                }
                catch (const std::exception& e) {
                    std::cerr << "Tick error: " << e.what() << std::endl;
                    res.result(http::status::bad_request);
                    boost::json::object err;
                    err["code"] = "invalidArgument";
                    err["message"] = e.what();
                    res.body() = boost::json::serialize(err);
                    res.set(http::field::content_length, std::to_string(res.body().size()));
                }
            }
            else if (target == "/api/v1/game/join" || target == "api/v1/game/join") {
                try {
                    auto body = boost::json::parse(req.body()).as_object();
                    if (!body.contains("userName") || !body.contains("mapId")) {
                        throw std::runtime_error("Missing required fields: userName or mapId");
                    }
                    std::string user_name = body.at("userName").as_string().c_str();
                    std::string map_id = body.at("mapId").as_string().c_str();

                    const Map* map = server->GetMap(map_id);
                    if (!map) {
                        res.result(http::status::bad_request);
                        boost::json::object err;
                        err["code"] = "mapNotFound";
                        err["message"] = "Map not found";
                        res.body() = boost::json::serialize(err);
                        res.set(http::field::content_length, std::to_string(res.body().size()));
                    }
                    else {
                        static PlayerId::IdType next_player_id = 0;
                        static std::mt19937 rng(std::random_device{}());
                        PlayerId player_id{ next_player_id++ };
                        std::string auth_token = std::to_string(std::uniform_int_distribution<unsigned long long>(0, ULLONG_MAX)(rng));

                        GameState::Player player;
                        player.position = { 0.0, 0.0 };
                        player.speed = { 0.0, 0.0 };
                        player.direction = GameState::Direction::U;
                        server->AddPlayer(player_id, player);

                        boost::json::object resp;
                        resp["authToken"] = auth_token;
                        resp["playerId"] = static_cast<int>(player_id.GetId());
                        res.result(http::status::ok);
                        res.body() = boost::json::serialize(resp);
                        res.set(http::field::content_length, std::to_string(res.body().size()));
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "Join error: " << e.what() << std::endl;
                    res.result(http::status::bad_request);
                    boost::json::object err;
                    err["code"] = "invalidArgument";
                    err["message"] = e.what();
                    res.body() = boost::json::serialize(err);
                    res.set(http::field::content_length, std::to_string(res.body().size()));
                }
            }
            else {
                res.result(http::status::method_not_allowed);
                res.set(http::field::allow, "GET, HEAD");
                boost::json::object err;
                err["code"] = "invalidMethod";
                err["message"] = "Method not allowed";
                res.body() = boost::json::serialize(err);
                res.set(http::field::content_length, std::to_string(res.body().size()));
            }
        }
        else {
            res.result(http::status::method_not_allowed);
            res.set(http::field::allow, "GET, HEAD");
            boost::json::object err;
            err["code"] = "invalidMethod";
            err["message"] = "Method not allowed";
            res.body() = boost::json::serialize(err);
            res.set(http::field::content_length, std::to_string(res.body().size()));
        }

        // Убеждаемся, что Content-Length всегда установлен
        if (res.find(http::field::content_length) == res.end()) {
            if (res.body().empty()) {
                res.set(http::field::content_length, "0");
            } else {
                res.set(http::field::content_length, std::to_string(res.body().size()));
            }
        }

        std::cout << "Sending response: " << res.result_int() << " " << res.result() 
                  << " Content-Length: " << res.at(http::field::content_length) << std::endl;

        // Отправляем ответ (синхронно)
        http::write(socket, res, ec);
        if (ec) {
            std::cerr << "Write error: " << ec.message() << std::endl;
        } else {
            std::cout << "Response sent successfully" << std::endl;
        }

        // Закрываем соединение
        socket.shutdown(tcp::socket::shutdown_send, ec);
        if (ec && ec != beast::errc::not_connected) {
            std::cerr << "Shutdown error: " << ec.message() << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "HandleRequest exception: " << e.what() << std::endl;
    }
}

void GameServer::Run() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    const auto tick_duration = std::chrono::milliseconds(100);
    std::cout << "Tick duration: " << tick_duration.count() << "ms" << std::endl;

    std::thread tick_thread([this, tick_duration]() {
        try {
            std::cout << "Tick thread started" << std::endl;
            while (!stop_server) {
                auto start = std::chrono::steady_clock::now();
                ProcessTick(tick_duration);
                auto end = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                if (elapsed < tick_duration) {
                    std::this_thread::sleep_for(tick_duration - elapsed);
                }
            }
            std::cout << "Tick thread stopping" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Tick thread exception: " << e.what() << std::endl;
        }
    });

    auto server_ptr = std::shared_ptr<GameServer>(this, [](GameServer* p) {});

    std::cout << "Starting HTTP server on port 8080..." << std::endl;

    try {
        net::io_context ioc;
        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 8080));
        std::cout << "HTTP server started on port 8080" << std::endl;

        while (!stop_server) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            std::cout << "New connection accepted" << std::endl;
            
            // Обрабатываем запрос синхронно в отдельном потоке
            std::thread([socket = std::move(socket), server_ptr]() mutable {
                HandleRequest(socket, server_ptr);
            }).detach();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "HTTP server exception: " << e.what() << std::endl;
        stop_server = true;
        if (tick_thread.joinable()) {
            tick_thread.join();
        }
        throw;
    }

    stop_server = true;
    if (tick_thread.joinable()) {
        tick_thread.join();
    }
    std::cout << "Server stopped." << std::endl;
}
