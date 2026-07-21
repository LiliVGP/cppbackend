#pragma once
#include "http_server.h"
#include "model.h"
#include "extra_data.h"

#include <boost/json.hpp>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <random>
#include <sstream>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using namespace std::literals;

namespace detail {

inline json::array SerializeRoads(const model::Map& map) {
    json::array roads;
    for (const auto& road : map.GetRoads()) {
        json::object road_obj;
        road_obj["x0"] = road.GetStart().x;
        road_obj["y0"] = road.GetStart().y;
        if (road.IsHorizontal()) {
            road_obj["x1"] = road.GetEnd().x;
        } else {
            road_obj["y1"] = road.GetEnd().y;
        }
        roads.push_back(std::move(road_obj));
    }
    return roads;
}

inline json::array SerializeBuildings(const model::Map& map) {
    json::array buildings;
    for (const auto& building : map.GetBuildings()) {
        json::object building_obj;
        building_obj["x"] = building.GetBounds().position.x;
        building_obj["y"] = building.GetBounds().position.y;
        building_obj["w"] = building.GetBounds().size.width;
        building_obj["h"] = building.GetBounds().size.height;
        buildings.push_back(std::move(building_obj));
    }
    return buildings;
}

inline json::array SerializeOffices(const model::Map& map) {
    json::array offices;
    for (const auto& office : map.GetOffices()) {
        json::object office_obj;
        office_obj["id"] = *office.GetId();
        office_obj["x"] = office.GetPosition().x;
        office_obj["y"] = office.GetPosition().y;
        office_obj["offsetX"] = office.GetOffset().dx;
        office_obj["offsetY"] = office.GetOffset().dy;
        offices.push_back(std::move(office_obj));
    }
    return offices;
}

inline json::object SerializeMap(const model::Map& map, const extra_data::GameExtraData& extra_data) {
    json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();
    map_obj["roads"] = SerializeRoads(map);
    map_obj["buildings"] = SerializeBuildings(map);
    map_obj["offices"] = SerializeOffices(map);

    const auto* map_extra = extra_data.GetMapExtraData(map.GetId());
    if (map_extra) {
        map_obj["lootTypes"] = map_extra->GetLootTypes();
    }

    return map_obj;
}

}  // namespace detail

class RequestHandler {
public:
    RequestHandler(model::Game& game, extra_data::GameExtraData& extra_data, const std::string& static_dir)
        : game_{game}
        , extra_data_{extra_data}
        , static_dir_{static_dir} {
        while (static_dir_.size() > 1 && static_dir_.back() == '/') {
            static_dir_.pop_back();
        }
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string_view target = req.target();

        if (target.starts_with("/api/v1/maps"sv)) {
            HandleMaps(req, send);
            return;
        }

        if (target == "/api/v1/game/join"sv) {
            HandleJoin(req, send);
            return;
        }

        if (target == "/api/v1/game/state"sv) {
            HandleState(req, send);
            return;
        }

        if (target == "/api/v1/game/player/action"sv) {
            HandleAction(req, send);
            return;
        }

        if (target == "/api/v1/game/tick"sv) {
            HandleTick(req, send);
            return;
        }

        if (target == "/api/v1/game/players"sv) {
            HandlePlayers(req, send);
            return;
        }

        if (target.starts_with("/api/"sv)) {
            json::object err_obj;
            err_obj["code"] = "badRequest";
            err_obj["message"] = "invalid request";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        if (req.method() == http::verb::get || req.method() == http::verb::head) {
            if (ServeStaticFile(req, send)) {
                return;
            }
        }

        json::object err_obj;
        err_obj["code"] = "badRequest";
        err_obj["message"] = "invalid request";
        send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
    }

private:
    template <typename Body, typename Allocator, typename Send>
    void HandleMaps(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        std::string_view target = req.target();
        std::string_view api_prefix = "/api/v1/maps"sv;

        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            json::object err_obj;
            err_obj["code"] = "invalidMethod";
            err_obj["message"] = "Invalid method";
            send(MakeMethodNotAllowedResponse(json::serialize(err_obj), req, "GET, HEAD"));
            return;
        }

        std::string_view suffix = target.substr(api_prefix.size());

        if (suffix.empty() || suffix == "/"sv) {
            json::array maps_arr;
            for (const auto& map : game_.GetMaps()) {
                json::object map_obj;
                map_obj["id"] = *map.GetId();
                map_obj["name"] = map.GetName();
                maps_arr.push_back(std::move(map_obj));
            }
            std::string body = json::serialize(maps_arr);
            send(MakeJsonResponse(http::status::ok, body, req, req.keep_alive()));
            return;
        }

        if (suffix.starts_with("/"sv)) {
            std::string_view map_id_str = suffix.substr(1);
            if (map_id_str.find('/') == std::string_view::npos) {
                model::Map::Id map_id{std::string{map_id_str}};
                if (const auto* map = game_.FindMap(map_id)) {
                    json::object map_obj = detail::SerializeMap(*map, extra_data_);
                    std::string body = json::serialize(map_obj);
                    send(MakeJsonResponse(http::status::ok, body, req, req.keep_alive()));
                    return;
                } else {
                    json::object err_obj;
                    err_obj["code"] = "mapNotFound";
                    err_obj["message"] = "Map not found";
                    send(MakeJsonResponse(http::status::not_found, json::serialize(err_obj), req, false));
                    return;
                }
            }
        }

        json::object err_obj;
        err_obj["code"] = "badRequest";
        err_obj["message"] = "invalid request";
        send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleJoin(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            json::object err_obj;
            err_obj["code"] = "invalidMethod";
            err_obj["message"] = "Only POST method is expected";
            send(MakeMethodNotAllowedResponse(json::serialize(err_obj), req, "POST"));
            return;
        }

        json::value parsed;
        try {
            parsed = json::parse(req.body());
        } catch (...) {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Join game request parse error";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        if (!parsed.is_object()) {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Join game request parse error";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        const auto& obj = parsed.as_object();

        auto user_name_it = obj.find("userName");
        if (user_name_it == obj.end() || !user_name_it->value().is_string()) {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Invalid name";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        std::string user_name = user_name_it->value().as_string().data();
        if (user_name.empty()) {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Invalid name";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        auto map_id_it = obj.find("mapId");
        if (map_id_it == obj.end() || !map_id_it->value().is_string()) {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Invalid map id";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        std::string map_id_str = map_id_it->value().as_string().data();

        model::Map::Id map_id{map_id_str};
        const auto* map = game_.FindMap(map_id);
        if (!map) {
            json::object err_obj;
            err_obj["code"] = "mapNotFound";
            err_obj["message"] = "Map not found";
            send(MakeJsonResponse(http::status::not_found, json::serialize(err_obj), req, false));
            return;
        }

        const int player_id = game_.AddPlayer(user_name, map_id);
        const std::string token = GenerateToken();

        game_.RegisterPlayer(token, map_id, player_id);

        json::object resp_obj;
        resp_obj["authToken"] = token;
        resp_obj["playerId"] = player_id;

        send(MakeJsonResponse(http::status::ok, json::serialize(resp_obj), req, true));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleState(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            json::object err_obj;
            err_obj["code"] = "invalidMethod";
            err_obj["message"] = "Invalid method";
            send(MakeMethodNotAllowedResponse(json::serialize(err_obj), req, "GET, HEAD"));
            return;
        }

        auto auth_it = req.find("Authorization");
        if (auth_it == req.end() || std::string(auth_it->value()).substr(0, 7) != "Bearer ") {
            json::object err_obj;
            err_obj["code"] = "invalidToken";
            err_obj["message"] = "Authorization header is required";
            send(MakeJsonResponse(http::status::unauthorized, json::serialize(err_obj), req, false));
            return;
        }

        std::string token = std::string(auth_it->value()).substr(7);

        if (!model::Game::IsValidTokenFormat(token)) {
            json::object err_obj;
            err_obj["code"] = "invalidToken";
            err_obj["message"] = "Invalid token format";
            send(MakeJsonResponse(http::status::unauthorized, json::serialize(err_obj), req, false));
            return;
        }

        if (!game_.ValidateToken(token)) {
            json::object err_obj;
            err_obj["code"] = "unknownToken";
            err_obj["message"] = "Player token has not been found";
            send(MakeJsonResponse(http::status::unauthorized, json::serialize(err_obj), req, false));
            return;
        }

        const auto& players = game_.GetPlayersByToken(token);

        json::object resp_obj;
        json::object players_obj;

        for (const auto& [id, info] : players) {
            const auto* player = game_.GetPlayer(id);
            if (!player) {
                continue;
            }

            json::object player_obj;

            json::array pos_arr;
            pos_arr.push_back(player->GetPosition().x);
            pos_arr.push_back(player->GetPosition().y);
            player_obj["pos"] = json::value(std::move(pos_arr));

            json::array speed_arr;
            speed_arr.push_back(player->GetSpeed().dx);
            speed_arr.push_back(player->GetSpeed().dy);
            player_obj["speed"] = json::value(std::move(speed_arr));

            player_obj["dir"] = model::DirectionToString(player->GetDirection());

            players_obj[std::to_string(id)] = std::move(player_obj);
        }

        resp_obj["players"] = std::move(players_obj);

        const auto* map_id = game_.GetMapIdByToken(token);
        if (map_id) {
            const auto& lost_objects = game_.GetLostObjects(*map_id);
            json::object lost_obj;
            for (const auto& obj : lost_objects) {
                json::object loot_obj;
                loot_obj["type"] = obj.type;
                json::array pos_arr;
                pos_arr.push_back(obj.pos.x);
                pos_arr.push_back(obj.pos.y);
                loot_obj["pos"] = json::value(std::move(pos_arr));
                lost_obj[std::to_string(obj.id)] = std::move(loot_obj);
            }
            resp_obj["lostObjects"] = std::move(lost_obj);
        } else {
            resp_obj["lostObjects"] = json::object();
        }

        send(MakeJsonResponse(http::status::ok, json::serialize(resp_obj), req, true));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleAction(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            json::object err_obj;
            err_obj["code"] = "invalidMethod";
            err_obj["message"] = "Invalid method";
            send(MakeMethodNotAllowedResponse(json::serialize(err_obj), req, "POST"));
            return;
        }

        auto auth_it = req.find("Authorization");
        if (auth_it == req.end() || std::string(auth_it->value()).substr(0, 7) != "Bearer ") {
            json::object err_obj;
            err_obj["code"] = "invalidToken";
            err_obj["message"] = "Authorization header is required";
            send(MakeJsonResponse(http::status::unauthorized, json::serialize(err_obj), req, false));
            return;
        }

        std::string token = std::string(auth_it->value()).substr(7);

        if (!model::Game::IsValidTokenFormat(token)) {
            json::object err_obj;
            err_obj["code"] = "invalidToken";
            err_obj["message"] = "Invalid token format";
            send(MakeJsonResponse(http::status::unauthorized, json::serialize(err_obj), req, false));
            return;
        }

        if (!game_.ValidateToken(token)) {
            json::object err_obj;
            err_obj["code"] = "unknownToken";
            err_obj["message"] = "Player token has not been found";
            send(MakeJsonResponse(http::status::unauthorized, json::serialize(err_obj), req, false));
            return;
        }

        int player_id = game_.GetPlayerIdByToken(token);
        const auto* player = game_.GetPlayer(player_id);
        if (!player) {
            json::object err_obj;
            err_obj["code"] = "unknownToken";
            err_obj["message"] = "Player token has not been found";
            send(MakeJsonResponse(http::status::unauthorized, json::serialize(err_obj), req, false));
            return;
        }

        json::value parsed;
        try {
            parsed = json::parse(req.body());
        } catch (...) {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Failed to parse action";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        if (!parsed.is_object()) {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Failed to parse action";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        const auto& obj = parsed.as_object();

        auto move_it = obj.find("move");
        if (move_it == obj.end() || !move_it->value().is_string()) {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Failed to parse action";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        std::string move = move_it->value().as_string().data();

        if (move != "L" && move != "R" && move != "U" && move != "D" && move != "") {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Failed to parse action";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        double dx = 0.0, dy = 0.0;

        const auto* map_id = game_.GetMapIdByToken(token);
        double speed = 1.0;
        if (map_id) {
            speed = game_.GetDogSpeed(*map_id);
        }

        if (move == "L") {
            dx = -speed;
        } else if (move == "R") {
            dx = speed;
        } else if (move == "U") {
            dy = -speed;
        } else if (move == "D") {
            dy = speed;
        }

        auto* mutable_player = const_cast<model::Player*>(player);
        mutable_player->SetSpeed(dx, dy);

        if (dx > 0) {
            mutable_player->SetDirection(model::PlayerDirection::EAST);
        } else if (dx < 0) {
            mutable_player->SetDirection(model::PlayerDirection::WEST);
        } else if (dy > 0) {
            mutable_player->SetDirection(model::PlayerDirection::SOUTH);
        } else if (dy < 0) {
            mutable_player->SetDirection(model::PlayerDirection::NORTH);
        }

        send(MakeJsonResponse(http::status::ok, "{}", req, true));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleTick(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            json::object err_obj;
            err_obj["code"] = "invalidMethod";
            err_obj["message"] = "Invalid method";
            send(MakeMethodNotAllowedResponse(json::serialize(err_obj), req, "POST"));
            return;
        }

        json::value parsed;
        try {
            parsed = json::parse(req.body());
        } catch (...) {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Failed to parse tick request JSON";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        if (!parsed.is_object()) {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Failed to parse tick request JSON";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        const auto& obj = parsed.as_object();

        auto delta_it = obj.find("timeDelta");
        if (delta_it == obj.end() || !delta_it->value().is_int64()) {
            json::object err_obj;
            err_obj["code"] = "invalidArgument";
            err_obj["message"] = "Failed to parse tick request JSON";
            send(MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), req, false));
            return;
        }

        int64_t time_delta = delta_it->value().as_int64();

        game_.Tick(static_cast<int>(time_delta));

        send(MakeJsonResponse(http::status::ok, "{}", req, true));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandlePlayers(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            json::object err_obj;
            err_obj["code"] = "invalidMethod";
            err_obj["message"] = "Invalid method";
            send(MakeMethodNotAllowedResponse(json::serialize(err_obj), req, "GET, HEAD"));
            return;
        }

        auto auth_it = req.find("Authorization");
        if (auth_it == req.end() || std::string(auth_it->value()).substr(0, 7) != "Bearer ") {
            json::object err_obj;
            err_obj["code"] = "invalidToken";
            err_obj["message"] = "Authorization header is required";
            send(MakeJsonResponse(http::status::unauthorized, json::serialize(err_obj), req, false));
            return;
        }

        std::string token = std::string(auth_it->value()).substr(7);

        if (!model::Game::IsValidTokenFormat(token)) {
            json::object err_obj;
            err_obj["code"] = "invalidToken";
            err_obj["message"] = "Invalid token format";
            send(MakeJsonResponse(http::status::unauthorized, json::serialize(err_obj), req, false));
            return;
        }

        if (!game_.ValidateToken(token)) {
            json::object err_obj;
            err_obj["code"] = "unknownToken";
            err_obj["message"] = "Player token has not been found";
            send(MakeJsonResponse(http::status::unauthorized, json::serialize(err_obj), req, false));
            return;
        }

        const auto& players = game_.GetPlayersByToken(token);

        json::object resp_obj;
        for (const auto& [id, info] : players) {
            json::object player_obj;
            player_obj["name"] = info.name;
            resp_obj[std::to_string(id)] = std::move(player_obj);
        }

        send(MakeJsonResponse(http::status::ok, json::serialize(resp_obj), req, true));
    }

    std::string GenerateToken() {
        static const char chars[] = "0123456789abcdef";
        static thread_local std::mt19937 rng(std::random_device{}());
        static thread_local std::uniform_int_distribution<> dist(0, sizeof(chars) - 2);

        std::string token;
        token.reserve(32);
        for (int i = 0; i < 32; ++i) {
            token += chars[dist(rng)];
        }
        return token;
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeJsonResponse(
            http::status status, std::string_view body,
            const http::request<Body, http::basic_fields<Allocator>>& req,
            bool keep_alive) {
        http::response<http::string_body> response(status, req.version());

        if (req.method() == http::verb::head) {
            response.set(http::field::content_type, "application/json"sv);
            response.set("Cache-Control", "no-cache"sv);
            response.set(http::field::content_length, "0"sv);
            response.keep_alive(keep_alive);
            response.body() = "";
        } else {
            response.set(http::field::content_type, "application/json"sv);
            response.set("Cache-Control", "no-cache"sv);
            response.body() = std::string{body};
            response.keep_alive(keep_alive);
            response.prepare_payload();
        }

        return response;
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeMethodNotAllowedResponse(
            std::string_view body,
            const http::request<Body, http::basic_fields<Allocator>>& req,
            std::string_view allow) {
        http::response<http::string_body> response(http::status::method_not_allowed, req.version());
        response.set(http::field::content_type, "application/json"sv);
        response.set("Cache-Control", "no-cache"sv);
        response.set(http::field::allow, allow);
        response.body() = std::string{body};
        response.prepare_payload();
        return response;
    }

    template <typename Body, typename Allocator, typename Send>
    bool ServeStaticFile(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        std::string target(static_cast<std::string>(req.target()));

        std::string decoded_target;
        UrlDecode(target, decoded_target);

        while (!decoded_target.empty() && decoded_target.front() == '/') {
            decoded_target.erase(0, 1);
        }

        if (decoded_target.empty()) {
            decoded_target = "index.html";
        }

        std::filesystem::path file_path = std::filesystem::path(static_dir_) / decoded_target;

        std::error_code ec;
        if (!std::filesystem::exists(file_path, ec) || ec) {
            return false;
        }

        if (std::filesystem::is_directory(file_path)) {
            file_path /= "index.html";
        }

        if (!std::filesystem::exists(file_path, ec) || ec) {
            return false;
        }

        std::uintmax_t file_size = std::filesystem::file_size(file_path, ec);
        if (ec) {
            return false;
        }

        std::string content_type = GetMimeType(file_path.extension().string());

        http::response<http::string_body> response(
            req.method() == http::verb::head ? http::status::no_content : http::status::ok,
            req.version());
        response.set(http::field::content_type, content_type);
        response.set(http::field::content_length, std::to_string(file_size));
        response.keep_alive(req.keep_alive());

        if (req.method() == http::verb::get) {
            std::ifstream file(file_path, std::ios::binary);
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                response.body() = content;
            }
        }

        response.prepare_payload();
        send(std::move(response));
        return true;
    }

    void UrlDecode(const std::string& src, std::string& dst) {
        dst.clear();
        for (size_t i = 0; i < src.size(); ++i) {
            if (src[i] == '%' && i + 2 < src.size()) {
                std::string hex = src.substr(i + 1, 2);
                char c = static_cast<char>(std::stoi(hex, nullptr, 16));
                dst += c;
                i += 2;
            } else if (src[i] == '+') {
                dst += ' ';
            } else {
                dst += src[i];
            }
        }
    }

    std::string GetMimeType(const std::string& extension) {
        std::string ext = extension;
        for (auto& c : ext) {
            c = std::tolower(c);
        }

        if (ext == ".htm" || ext == ".html") return "text/html";
        if (ext == ".css") return "text/css";
        if (ext == ".txt") return "text/plain";
        if (ext == ".js") return "text/javascript";
        if (ext == ".json") return "application/json";
        if (ext == ".xml") return "application/xml";
        if (ext == ".png") return "image/png";
        if (ext == ".jpg" || ext == ".jpe" || ext == ".jpeg") return "image/jpeg";
        if (ext == ".gif") return "image/gif";
        if (ext == ".bmp") return "image/bmp";
        if (ext == ".ico") return "image/vnd.microsoft.icon";
        if (ext == ".tiff" || ext == ".tif") return "image/tiff";
        if (ext == ".svg" || ext == ".svgz") return "image/svg+xml";
        if (ext == ".mp3") return "audio/mpeg";

        return "application/octet-stream";
    }

    model::Game& game_;
    extra_data::GameExtraData& extra_data_;
    std::string static_dir_;
};

}  // namespace http_handler
