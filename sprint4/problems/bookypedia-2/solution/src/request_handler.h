#pragma once
#include "http_server.h"
#include "http_utils.h"
#include "model.h"
#include "extra_data.h"
#include "connection_pool.h"
#include "database.h"

#include <boost/json.hpp>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <random>
#include <sstream>
#include <functional>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using namespace std::literals;

constexpr auto kApiMaps = "/api/v1/maps"sv;
constexpr auto kApiJoin = "/api/v1/game/join"sv;
constexpr auto kApiState = "/api/v1/game/state"sv;
constexpr auto kApiAction = "/api/v1/game/player/action"sv;
constexpr auto kApiTick = "/api/v1/game/tick"sv;
constexpr auto kApiPlayers = "/api/v1/game/players"sv;
constexpr auto kApiRecords = "/api/v1/game/records"sv;
constexpr auto kApiPrefix = "/api/"sv;

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
    using TickCallback = std::function<void(int game_time_ms)>;

    RequestHandler(model::Game& game, extra_data::GameExtraData& extra_data, const std::string& static_dir,
                   ConnectionPool* db_pool = nullptr, TickCallback tick_cb = nullptr)
        : game_{game}
        , extra_data_{extra_data}
        , static_dir_{static_dir}
        , db_pool_{db_pool}
        , tick_callback_{std::move(tick_cb)} {
        while (static_dir_.size() > 1 && static_dir_.back() == '/') {
            static_dir_.pop_back();
        }
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string_view target = req.target();

        if (target.starts_with(kApiMaps))          { HandleMaps(req, send);    return; }
        if (target == kApiJoin)                    { HandleJoin(req, send);     return; }
        if (target == kApiState)                   { HandleState(req, send);    return; }
        if (target == kApiAction)                  { HandleAction(req, send);   return; }
        if (target == kApiTick)                    { HandleTick(req, send);     return; }
        if (target == kApiPlayers)                 { HandlePlayers(req, send);  return; }
        if (target.starts_with(kApiRecords))       { HandleRecords(req, send);  return; }

        if (target.starts_with(kApiPrefix)) {
            SendError(send, http::status::bad_request, "badRequest", "invalid request", req, false);
            return;
        }

        if (req.method() == http::verb::get || req.method() == http::verb::head) {
            if (ServeStaticFile(req, send)) return;
        }

        SendError(send, http::status::bad_request, "badRequest", "invalid request", req, false);
    }

private:
    // ── Response helpers ──────────────────────────────────────────────

    template <typename Send>
    void SendError(Send&& send, http::status status, std::string_view code,
                   std::string_view message, const auto& req, bool keep_alive) {
        json::object err_obj;
        err_obj["code"] = std::string{code};
        err_obj["message"] = std::string{message};
        send(MakeJsonResponse(status, json::serialize(err_obj), req, keep_alive));
    }

    template <typename Send>
    void SendMethodNotAllowed(Send&& send, const auto& req, std::string_view allow,
                              std::string_view message = "Invalid method"sv) {
        json::object err_obj;
        err_obj["code"] = "invalidMethod";
        err_obj["message"] = std::string{message};
        send(MakeMethodNotAllowedResponse(json::serialize(err_obj), req, allow));
    }

    // ── Auth helper ───────────────────────────────────────────────────

    // Extracts and validates the Bearer token. Returns the token or std::nullopt
    // (and sends an error response if invalid).
    template <typename Body, typename Allocator, typename Send>
    std::optional<std::string> ExtractToken(
            const http::request<Body, http::basic_fields<Allocator>>& req, Send& send) {
        auto auth_it = req.find("Authorization");
        if (auth_it == req.end() ||
            std::string(auth_it->value()).substr(0, 7) != "Bearer ") {
            SendError(send, http::status::unauthorized, "invalidToken",
                      "Authorization header is required", req, false);
            return std::nullopt;
        }

        std::string token = std::string(auth_it->value()).substr(7);

        if (!model::Game::IsValidTokenFormat(token)) {
            SendError(send, http::status::unauthorized, "invalidToken",
                      "Invalid token format", req, false);
            return std::nullopt;
        }

        if (!game_.ValidateToken(token)) {
            SendError(send, http::status::unauthorized, "unknownToken",
                      "Player token has not been found", req, false);
            return std::nullopt;
        }

        return token;
    }

    // ── Handlers ──────────────────────────────────────────────────────

    template <typename Body, typename Allocator, typename Send>
    void HandleMaps(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            SendMethodNotAllowed(send, req, "GET, HEAD");
            return;
        }

        std::string_view suffix = req.target().substr(std::string_view(kApiMaps).size());

        if (suffix.empty() || suffix == "/"sv) {
            json::array maps_arr;
            for (const auto& map : game_.GetMaps()) {
                json::object map_obj;
                map_obj["id"] = *map.GetId();
                map_obj["name"] = map.GetName();
                maps_arr.push_back(std::move(map_obj));
            }
            send(MakeJsonResponse(http::status::ok, json::serialize(maps_arr), req, req.keep_alive()));
            return;
        }

        if (suffix.starts_with("/"sv)) {
            std::string_view map_id_str = suffix.substr(1);
            if (map_id_str.find('/') == std::string_view::npos) {
                model::Map::Id map_id{std::string{map_id_str}};
                if (const auto* map = game_.FindMap(map_id)) {
                    json::object map_obj = detail::SerializeMap(*map, extra_data_);
                    send(MakeJsonResponse(http::status::ok, json::serialize(map_obj), req, req.keep_alive()));
                    return;
                }
                SendError(send, http::status::not_found, "mapNotFound", "Map not found", req, false);
                return;
            }
        }

        SendError(send, http::status::bad_request, "badRequest", "invalid request", req, false);
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleJoin(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            SendMethodNotAllowed(send, req, "POST", "Only POST method is expected");
            return;
        }

        json::value parsed;
        try {
            parsed = json::parse(req.body());
        } catch (...) {
            SendError(send, http::status::bad_request, "invalidArgument",
                      "Join game request parse error", req, false);
            return;
        }

        if (!parsed.is_object()) {
            SendError(send, http::status::bad_request, "invalidArgument",
                      "Join game request parse error", req, false);
            return;
        }

        const auto& obj = parsed.as_object();

        auto user_name_it = obj.find("userName");
        if (user_name_it == obj.end() || !user_name_it->value().is_string()) {
            SendError(send, http::status::bad_request, "invalidArgument", "Invalid name", req, false);
            return;
        }

        std::string user_name = user_name_it->value().as_string().data();
        if (user_name.empty()) {
            SendError(send, http::status::bad_request, "invalidArgument", "Invalid name", req, false);
            return;
        }

        auto map_id_it = obj.find("mapId");
        if (map_id_it == obj.end() || !map_id_it->value().is_string()) {
            SendError(send, http::status::bad_request, "invalidArgument", "Invalid map id", req, false);
            return;
        }

        model::Map::Id map_id{map_id_it->value().as_string().data()};
        if (!game_.FindMap(map_id)) {
            SendError(send, http::status::not_found, "mapNotFound", "Map not found", req, false);
            return;
        }

        const int player_id = game_.AddPlayer(user_name, map_id);
        const std::string token = GenerateToken();
        game_.RegisterPlayer(token, map_id, player_id);

        json::object resp_obj;
        resp_obj["authToken"] = token;
        resp_obj["playerId"] = player_id;
        send(MakeJsonResponse(http::status::ok, json::serialize(resp_obj), req, req.keep_alive()));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleState(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            SendMethodNotAllowed(send, req, "GET, HEAD");
            return;
        }

        auto token = ExtractToken(req, send);
        if (!token) return;

        const auto& players = game_.GetPlayersByToken(*token);

        json::object players_obj;
        for (const auto& [id, info] : players) {
            const auto* player = game_.GetPlayer(id);
            if (!player) continue;

            json::object player_obj;
            player_obj["pos"] = MakeArray(player->GetPosition().x, player->GetPosition().y);
            player_obj["speed"] = MakeArray(player->GetSpeed().dx, player->GetSpeed().dy);
            player_obj["dir"] = model::DirectionToString(player->GetDirection());

            json::array bag_arr;
            for (const auto& item : player->GetBag()) {
                json::object item_obj;
                item_obj["id"] = item.id;
                item_obj["type"] = item.type;
                bag_arr.push_back(std::move(item_obj));
            }
            player_obj["bag"] = std::move(bag_arr);
            player_obj["score"] = player->GetScore();

            players_obj[std::to_string(id)] = std::move(player_obj);
        }

        json::object resp_obj;
        resp_obj["players"] = std::move(players_obj);

        // Add lost objects
        const auto* map_id = game_.GetMapIdByToken(*token);
        if (map_id) {
            const auto& lost_objects = game_.GetLostObjects(*map_id);
            json::object lost_obj;
            for (const auto& obj : lost_objects) {
                json::object loot_obj;
                loot_obj["type"] = obj.type;
                loot_obj["pos"] = MakeArray(obj.pos.x, obj.pos.y);
                lost_obj[std::to_string(obj.id)] = std::move(loot_obj);
            }
            resp_obj["lostObjects"] = std::move(lost_obj);
        } else {
            resp_obj["lostObjects"] = json::object();
        }

        send(MakeJsonResponse(http::status::ok, json::serialize(resp_obj), req, req.keep_alive()));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleAction(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            SendMethodNotAllowed(send, req, "POST");
            return;
        }

        auto token = ExtractToken(req, send);
        if (!token) return;

        int player_id = game_.GetPlayerIdByToken(*token);
        auto* player = game_.GetMutablePlayer(player_id);
        if (!player) {
            SendError(send, http::status::unauthorized, "unknownToken",
                      "Player token has not been found", req, false);
            return;
        }

        json::value parsed;
        try {
            parsed = json::parse(req.body());
        } catch (...) {
            SendError(send, http::status::bad_request, "invalidArgument",
                      "Failed to parse action", req, false);
            return;
        }

        if (!parsed.is_object()) {
            SendError(send, http::status::bad_request, "invalidArgument",
                      "Failed to parse action", req, false);
            return;
        }

        const auto& obj = parsed.as_object();
        auto move_it = obj.find("move");
        if (move_it == obj.end() || !move_it->value().is_string()) {
            SendError(send, http::status::bad_request, "invalidArgument",
                      "Failed to parse action", req, false);
            return;
        }

        std::string move = move_it->value().as_string().data();
        if (move != "L" && move != "R" && move != "U" && move != "D" && move != "") {
            SendError(send, http::status::bad_request, "invalidArgument",
                      "Failed to parse action", req, false);
            return;
        }

        double speed = 1.0;
        if (const auto* map_id = game_.GetMapIdByToken(*token)) {
            speed = game_.GetDogSpeed(*map_id);
        }

        double dx = 0.0, dy = 0.0;
        if (move == "L")       dx = -speed;
        else if (move == "R")  dx = speed;
        else if (move == "U")  dy = -speed;
        else if (move == "D")  dy = speed;

        player->SetSpeed(dx, dy);

        if (dx > 0)       player->SetDirection(model::PlayerDirection::EAST);
        else if (dx < 0)  player->SetDirection(model::PlayerDirection::WEST);
        else if (dy > 0)  player->SetDirection(model::PlayerDirection::SOUTH);
        else if (dy < 0)  player->SetDirection(model::PlayerDirection::NORTH);

        send(MakeJsonResponse(http::status::ok, "{}"sv, req, req.keep_alive()));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleTick(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            SendMethodNotAllowed(send, req, "POST");
            return;
        }

        json::value parsed;
        try {
            parsed = json::parse(req.body());
        } catch (...) {
            SendError(send, http::status::bad_request, "invalidArgument",
                      "Failed to parse tick request JSON", req, req.keep_alive());
            return;
        }

        if (!parsed.is_object()) {
            SendError(send, http::status::bad_request, "invalidArgument",
                      "Failed to parse tick request JSON", req, req.keep_alive());
            return;
        }

        auto delta_it = parsed.as_object().find("timeDelta");
        if (delta_it == parsed.as_object().end() || !delta_it->value().is_int64()) {
            SendError(send, http::status::bad_request, "invalidArgument",
                      "Failed to parse tick request JSON", req, req.keep_alive());
            return;
        }

        int time_delta = static_cast<int>(delta_it->value().as_int64());
        auto retired_players = game_.Tick(time_delta);

        // Save retired players to database
        if (db_pool_ && !retired_players.empty()) {
            for (const auto& rp : retired_players) {
                try {
                    database::AddRetiredPlayer(*db_pool_, {rp.name, rp.score, rp.play_time});
                } catch (const std::exception& ex) {
                    std::cerr << "Failed to save retired player: " << ex.what() << std::endl;
                }
            }
        }

        if (tick_callback_) {
            tick_callback_(time_delta);
        }

        send(MakeJsonResponse(http::status::ok, "{}"sv, req, req.keep_alive()));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandlePlayers(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            SendMethodNotAllowed(send, req, "GET, HEAD");
            return;
        }

        auto token = ExtractToken(req, send);
        if (!token) return;

        const auto& players = game_.GetPlayersByToken(*token);

        json::object resp_obj;
        for (const auto& [id, info] : players) {
            json::object player_obj;
            player_obj["name"] = info.name;
            resp_obj[std::to_string(id)] = std::move(player_obj);
        }

        send(MakeJsonResponse(http::status::ok, json::serialize(resp_obj), req, req.keep_alive()));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleRecords(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            SendMethodNotAllowed(send, req, "GET, HEAD");
            return;
        }

        // Parse query parameters
        std::string_view target = req.target();
        std::string_view query_str;
        if (auto qpos = target.find('?'); qpos != std::string_view::npos) {
            query_str = target.substr(qpos + 1);
        }
        auto params = util::ParseQuery(query_str);

        int start = 0;
        int max_items = 100;
        bool max_items_set = false;

        if (auto it = params.find("start"); it != params.end()) {
            try {
                start = std::stoi(it->second);
                if (start < 0) start = 0;
            } catch (...) {
                SendError(send, http::status::bad_request, "badRequest",
                          "Invalid start parameter", req, false);
                return;
            }
        }

        if (auto it = params.find("maxItems"); it != params.end()) {
            try {
                max_items = std::stoi(it->second);
                max_items_set = true;
            } catch (...) {
                SendError(send, http::status::bad_request, "badRequest",
                          "Invalid maxItems parameter", req, false);
                return;
            }
        }

        if (max_items_set && max_items > 100) {
            SendError(send, http::status::bad_request, "badRequest",
                      "maxItems exceeds 100", req, false);
            return;
        }

        if (max_items < 0) max_items = 0;

        json::array records_arr;
        if (db_pool_) {
            try {
                auto records = database::GetRecords(*db_pool_, start, max_items);
                for (const auto& r : records) {
                    json::object record_obj;
                    record_obj["name"] = r.name;
                    record_obj["score"] = r.score;
                    record_obj["playTime"] = r.play_time;
                    records_arr.push_back(std::move(record_obj));
                }
            } catch (const std::exception& ex) {
                std::cerr << "Failed to get records: " << ex.what() << std::endl;
            }
        }

        send(MakeJsonResponse(http::status::ok, json::serialize(records_arr), req, req.keep_alive()));
    }

    // ── Static file serving ───────────────────────────────────────────

    template <typename Body, typename Allocator, typename Send>
    bool ServeStaticFile(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        std::string target(static_cast<std::string>(req.target()));

        std::string decoded;
        util::UrlDecode(target, decoded);
        while (!decoded.empty() && decoded.front() == '/') {
            decoded.erase(0, 1);
        }
        if (decoded.empty()) decoded = "index.html";

        std::filesystem::path file_path = std::filesystem::path(static_dir_) / decoded;

        std::error_code ec;
        if (!std::filesystem::exists(file_path, ec) || ec) return false;
        if (std::filesystem::is_directory(file_path)) file_path /= "index.html";
        if (!std::filesystem::exists(file_path, ec) || ec) return false;

        auto file_size = std::filesystem::file_size(file_path, ec);
        if (ec) return false;

        http::response<http::string_body> response(
            req.method() == http::verb::head ? http::status::no_content : http::status::ok,
            req.version());
        response.set(http::field::content_type, util::GetMimeType(file_path.extension().string()));
        response.set(http::field::content_length, std::to_string(file_size));
        response.keep_alive(req.keep_alive());

        if (req.method() == http::verb::get) {
            std::ifstream file(file_path, std::ios::binary);
            if (file) {
                response.body() = std::string(std::istreambuf_iterator<char>(file),
                                               std::istreambuf_iterator<char>());
            }
        }

        response.prepare_payload();
        send(std::move(response));
        return true;
    }

    // ── Utility ───────────────────────────────────────────────────────

    static json::array MakeArray(double x, double y) {
        json::array arr;
        arr.push_back(x);
        arr.push_back(y);
        return arr;
    }

    std::string GenerateToken() {
        static const char chars[] = "0123456789abcdef";
        static thread_local std::mt19937 rng(std::random_device{}());
        static thread_local std::uniform_int_distribution<> dist(0, sizeof(chars) - 2);

        constexpr size_t kTokenLength = 32;
        std::string token;
        token.reserve(kTokenLength);
        for (size_t i = 0; i < kTokenLength; ++i) {
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
        response.set(http::field::content_type, "application/json"sv);
        response.set("Cache-Control", "no-cache"sv);
        if (req.method() == http::verb::head) {
            response.set(http::field::content_length, "0"sv);
            response.body() = "";
        } else {
            response.body() = std::string{body};
            response.prepare_payload();
        }
        response.keep_alive(keep_alive);
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

    model::Game& game_;
    extra_data::GameExtraData& extra_data_;
    std::string static_dir_;
    ConnectionPool* db_pool_;
    TickCallback tick_callback_;
};

}  // namespace http_handler
