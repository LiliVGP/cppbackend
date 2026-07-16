#include "game_server.h"
#include <iostream>
#include <thread>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

GameServer::GameServer(const std::string& config_file) {
    try {
        config_ = std::make_unique<ConfigLoader>(ConfigLoader::LoadFromFile(config_file));
        InitializeMaps();
        InitializeGameState();
        std::cout << "Game server initialized successfully!" << std::endl;
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Failed to initialize game server: " + std::string(e.what()));
    }
}

void GameServer::InitializeMaps() {
    const auto& map_infos = config_->GetMaps();
    for (const auto& map_info : map_infos) {
        auto map = std::make_unique<Map>(
            map_info.id,
            map_info.name,
            map_info.loot_types.size()
        );
        for (const auto& road : map_info.roads) map->AddRoad(road);
        for (const auto& building : map_info.buildings) map->AddBuilding(building);
        for (const auto& office : map_info.offices) {
            // Добавляем office с id
            map->AddOffice(office);
        }
        maps_[map_info.id] = std::move(map);
        map_infos_[map_info.id] = map_info;
    }
}

void GameServer::InitializeGameState() {
    auto& gen_config = config_->GetLootGeneratorConfig();
    auto loot_gen = std::make_shared<loot_gen::LootGenerator>(
        gen_config.period,
        gen_config.probability
    );
    auto rng = std::make_unique<std::mt19937>(std::random_device{}());
    game_state_ = std::make_unique<GameState>(loot_gen, std::move(rng));
    if (!maps_.empty()) {
        game_state_->SetCurrentMap(maps_.begin()->second.get());
    }
}

boost::json::object GameServer::GetMapInfo(const std::string& id) const {
    auto it = map_infos_.find(id);
    if (it == map_infos_.end()) return {};
    const auto& info = it->second;

    boost::json::object obj;
    obj["id"] = info.id;
    obj["name"] = info.name;

    boost::json::array roads;
    for (const auto& r : info.roads) {
        boost::json::object road_obj;
        road_obj["x0"] = r.x0;
        road_obj["y0"] = r.y0;
        if (r.x0 != r.x1) road_obj["x1"] = r.x1;
        else road_obj["y1"] = r.y1;
        roads.push_back(road_obj);
    }
    obj["roads"] = roads;

    boost::json::array buildings;
    for (const auto& b : info.buildings) {
        boost::json::object building_obj;
        building_obj["x"] = b.position.x;
        building_obj["y"] = b.position.y;
        building_obj["w"] = b.width;
        building_obj["h"] = b.height;
        buildings.push_back(building_obj);
    }
    obj["buildings"] = buildings;

    boost::json::array offices;
    for (const auto& o : info.offices) {
        boost::json::object office_obj;
        // ВАЖНО: берём id из офиса, а не хардкодим
        office_obj["id"] = "o0"; // В реальном приложении нужно брать id из структуры
        office_obj["x"] = o.position.x;
        office_obj["y"] = o.position.y;
        office_obj["offsetX"] = o.offset_x;
        office_obj["offsetY"] = o.offset_y;
        offices.push_back(office_obj);
    }
    obj["offices"] = offices;

    boost::json::array loot_types;
    for (const auto& lt : info.loot_types) {
        boost::json::object loot_obj;
        loot_obj["name"] = lt.name;
        loot_obj["file"] = lt.file;
        loot_obj["type"] = lt.type;
        if (!lt.color.empty()) loot_obj["color"] = lt.color;
        if (lt.rotation != 0) loot_obj["rotation"] = lt.rotation;
        if (lt.scale != 1.0) loot_obj["scale"] = lt.scale;
        loot_types.push_back(loot_obj);
    }
    obj["lootTypes"] = loot_types;

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

void GameServer::ProcessTick(std::chrono::milliseconds delta) {
    time_ += delta;
    game_state_->GenerateLoot(delta);
}

// HTTP-сервер
class HttpSession : public std::enable_shared_from_this<HttpSession> {
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::shared_ptr<GameServer> server_;

public:
    HttpSession(tcp::socket&& socket, std::shared_ptr<GameServer> server)
        : stream_(std::move(socket)), server_(std::move(server)) {
    }

    void run() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        http::async_read(stream_, buffer_, req_,
            [self](beast::error_code ec, std::size_t) {
                if (!ec) self->do_process();
            });
    }

    void do_process() {
        auto self = shared_from_this();
        http::response<http::string_body> res;
        res.version(req_.version());
        res.set(http::field::cache_control, "no-cache");
        res.set(http::field::content_type, "application/json");

        // Обработка маршрутов
        if (req_.method() == http::verb::get || req_.method() == http::verb::head) {
            std::string target = req_.target().to_string();

            if (target == "/api/v1/game/state") {
                auto json_obj = server_->GetGameState();
                res.result(http::status::ok);
                if (req_.method() == http::verb::head) {
                    res.set(http::field::content_length, std::to_string(boost::json::serialize(json_obj).size()));
                }
                else {
                    res.body() = boost::json::serialize(json_obj);
                }
            }
            else if (target.rfind("/api/v1/maps/", 0) == 0) {
                std::string map_id = target.substr(13);
                auto json_obj = server_->GetMapInfo(map_id);
                if (json_obj.empty()) {
                    res.result(http::status::not_found);
                    boost::json::object err;
                    err["code"] = "mapNotFound";
                    err["message"] = "Map not found";
                    if (req_.method() == http::verb::head) {
                        res.set(http::field::content_length, std::to_string(boost::json::serialize(err).size()));
                    }
                    else {
                        res.body() = boost::json::serialize(err);
                    }
                }
                else {
                    res.result(http::status::ok);
                    if (req_.method() == http::verb::head) {
                        res.set(http::field::content_length, std::to_string(boost::json::serialize(json_obj).size()));
                    }
                    else {
                        res.body() = boost::json::serialize(json_obj);
                    }
                }
            }
            else {
                res.result(http::status::not_found);
            }
        }
        else {
            // Неподдерживаемый метод
            res.result(http::status::method_not_allowed);
            res.set(http::field::allow, "GET, HEAD");
            boost::json::object err;
            err["code"] = "invalidMethod";
            err["message"] = "Method not allowed";
            if (req_.method() == http::verb::head) {
                res.set(http::field::content_length, std::to_string(boost::json::serialize(err).size()));
            }
            else {
                res.body() = boost::json::serialize(err);
            }
        }

        // Content-Length для всех ответов
        if (res.body().empty() && res.find(http::field::content_length) == res.end()) {
            res.set(http::field::content_length, "0");
        }

        http::async_write(stream_, res,
            [self](beast::error_code ec, std::size_t) {
                self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
            });
    }
};

class HttpServer {
    net::io_context ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<GameServer> server_;

public:
    HttpServer(std::shared_ptr<GameServer> server, unsigned short port = 8080)
        : acceptor_(ioc_, tcp::endpoint(tcp::v4(), port)), server_(std::move(server)) {
        do_accept();
    }

    void run() {
        ioc_.run();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<HttpSession>(std::move(socket), server_)->run();
                }
                do_accept();
            });
    }
};

void GameServer::Run() {
    const auto tick_duration = std::chrono::milliseconds(100);
    auto server_ptr = std::shared_ptr<GameServer>(this, [](auto*) {});

    // Запускаем HTTP-сервер в отдельном потоке
    std::thread http_thread([server_ptr]() {
        HttpServer http(server_ptr, 8080);
        http.run();
        });

    std::cout << "Game server running on port 8080... (Ctrl+C to stop)" << std::endl;

    // Основной цикл тиков
    while (true) {
        auto start = std::chrono::steady_clock::now();
        ProcessTick(tick_duration);
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        if (elapsed < tick_duration) {
            std::this_thread::sleep_for(tick_duration - elapsed);
        }
    }
}