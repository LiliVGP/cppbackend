#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <string_view>

namespace http_handler {
    namespace json = boost::json;
    namespace http = boost::beast::http;

    class ApiHandler {
    public:
        explicit ApiHandler(model::Game& game) : game_(game) {}

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            std::string_view target(req.target().data(), req.target().size());

            // Обработка GET /api/v1/game/state
            if (target == "/api/v1/game/state") {
                return HandleGameState(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            // Обработка GET /api/v1/maps (для тестов)
            else if (target == "/api/v1/maps") {
                return HandleMaps(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            // Обработка GET /api/v1/maps/{map_id}
            else if (target.find("/api/v1/maps/") == 0) {
                std::string map_id = std::string(target.substr(std::string("/api/v1/maps/").size()));
                return HandleMap(map_id, std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            // Обработка POST /api/v1/game/join
            else if (target == "/api/v1/game/join") {
                return HandleJoinGame(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            // Обработка GET /api/v1/game/players
            else if (target == "/api/v1/game/players") {
                return HandlePlayers(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            // Если путь не найден, возвращаем 404
            else {
                return send(MakeErrorResponse(http::status::not_found, "notFound", "API endpoint not found", req.version(), req.keep_alive()));
            }
        }

    private:
        // --- Обработчики существующих API (необходимы для тестов) ---
        template <typename Body, typename Allocator, typename Send>
        void HandleMaps(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req.version(), req.keep_alive(), "GET"));
            }
            
            json::array maps_array;
            for (const auto& map : game_.GetMaps()) {
                json::object map_obj;
                map_obj["id"] = std::string(*map.GetId());
                map_obj["name"] = map.GetName();
                maps_array.push_back(map_obj);
            }
            auto response = MakeJsonResponse(http::status::ok, maps_array, req.version(), req.keep_alive());
            send(std::move(response));
        }

        template <typename Body, typename Allocator, typename Send>
        void HandleMap(std::string map_id, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req.version(), req.keep_alive(), "GET"));
            }

            const auto* map = game_.FindMap(model::Map::Id(map_id));
            if (!map) {
                return send(MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found", req.version(), req.keep_alive()));
            }

            json::object map_obj;
            map_obj["id"] = std::string(*map->GetId());
            map_obj["name"] = map->GetName();

            json::array roads_array;
            for (const auto& road : map->GetRoads()) {
                json::object road_obj;
                if (road.IsHorizontal()) {
                    road_obj["x0"] = road.GetStart().x;
                    road_obj["y0"] = road.GetStart().y;
                    road_obj["x1"] = road.GetEnd().x;
                } else {
                    road_obj["x0"] = road.GetStart().x;
                    road_obj["y0"] = road.GetStart().y;
                    road_obj["y1"] = road.GetEnd().y;
                }
                roads_array.push_back(road_obj);
            }
            map_obj["roads"] = roads_array;

            json::array buildings_array;
            for (const auto& building : map->GetBuildings()) {
                json::object building_obj;
                building_obj["x"] = building.GetBounds().position.x;
                building_obj["y"] = building.GetBounds().position.y;
                building_obj["w"] = building.GetBounds().size.width;
                building_obj["h"] = building.GetBounds().size.height;
                buildings_array.push_back(building_obj);
            }
            map_obj["buildings"] = buildings_array;

            json::array offices_array;
            for (const auto& office : map->GetOffices()) {
                json::object office_obj;
                office_obj["id"] = std::string(*office.GetId());
                office_obj["x"] = office.GetPosition().x;
                office_obj["y"] = office.GetPosition().y;
                office_obj["offsetX"] = office.GetOffset().dx;
                office_obj["offsetY"] = office.GetOffset().dy;
                offices_array.push_back(office_obj);
            }
            map_obj["offices"] = offices_array;

            auto response = MakeJsonResponse(http::status::ok, map_obj, req.version(), req.keep_alive());
            send(std::move(response));
        }

        template <typename Body, typename Allocator, typename Send>
        void HandleJoinGame(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            if (req.method() != http::verb::post) {
                return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", req.version(), req.keep_alive(), "POST"));
            }

            try {
                auto json_body = json::parse(req.body()).as_object();
                if (!json_body.contains("mapId") || !json_body.at("mapId").is_string()) {
                    throw std::runtime_error("Missing or invalid mapId");
                }
                std::string map_id = std::string(json_body.at("mapId").as_string());

                if (!json_body.contains("userName") || !json_body.at("userName").is_string()) {
                    throw std::runtime_error("Missing or invalid userName");
                }
                std::string user_name = std::string(json_body.at("userName").as_string());

                auto result = game_.JoinGame(user_name, map_id);

                if (auto* error = std::get_if<std::string>(&result)) {
                    if (*error == "Map not found") {
                        return send(MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found", req.version(), req.keep_alive()));
                    }
                    else if (*error == "Invalid name") {
                        return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid name", req.version(), req.keep_alive()));
                    }
                    return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", *error, req.version(), req.keep_alive()));
                }

                auto join_result = std::get<model::Game::JoinGameResult>(result);
                json::object response_obj;
                response_obj["authToken"] = *join_result.auth_token;
                response_obj["playerId"] = *join_result.player_id;

                auto response = MakeJsonResponse(http::status::ok, response_obj, req.version(), req.keep_alive());
                send(std::move(response));

            } catch (const std::exception&) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req.version(), req.keep_alive()));
            }
        }

        template <typename Body, typename Allocator, typename Send>
        void HandlePlayers(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req.version(), req.keep_alive(), "GET, HEAD"));
            }

            auto auth_header = req.find(http::field::authorization);
            if (auth_header == req.end()) {
                return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is missing", req.version(), req.keep_alive()));
            }

            std::string_view auth_value = auth_header->value();
            const std::string_view bearer_prefix = "Bearer ";
            if (auth_value.size() <= bearer_prefix.size() ||
                auth_value.substr(0, bearer_prefix.size()) != bearer_prefix) {
                return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is invalid", req.version(), req.keep_alive()));
            }

            std::string token_str(auth_value.substr(bearer_prefix.size()));
            if (token_str.length() != 32) {
                return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is invalid", req.version(), req.keep_alive()));
            }

            for (char c : token_str) {
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                    return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is invalid", req.version(), req.keep_alive()));
                }
            }

            model::Token token(token_str);
            auto* player = game_.GetPlayerManager().FindPlayerByToken(token);
            if (!player) {
                return send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", req.version(), req.keep_alive()));
            }

            json::object players_obj;
            for (const auto& p : game_.GetPlayerManager().GetPlayers()) {
                if (p.GetMap() == player->GetMap()) {
                    json::object player_obj;
                    player_obj["name"] = p.GetName();
                    players_obj[std::to_string(*p.GetId())] = player_obj;
                }
            }

            auto response = MakeJsonResponse(http::status::ok, players_obj, req.version(), req.keep_alive());
            send(std::move(response));
        }

        // --- Обработчик game/state ---
        template <typename Body, typename Allocator, typename Send>
        void HandleGameState(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req.version(), req.keep_alive(), "GET, HEAD"));
            }

            auto auth_header = req.find(http::field::authorization);
            if (auth_header == req.end()) {
                return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is missing", req.version(), req.keep_alive()));
            }

            std::string_view auth_value = auth_header->value();
            const std::string_view bearer_prefix = "Bearer ";
            if (auth_value.size() <= bearer_prefix.size() ||
                auth_value.substr(0, bearer_prefix.size()) != bearer_prefix) {
                return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is invalid", req.version(), req.keep_alive()));
            }

            std::string token_str(auth_value.substr(bearer_prefix.size()));
            if (token_str.length() != 32) {
                return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is invalid", req.version(), req.keep_alive()));
            }

            for (char c : token_str) {
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                    return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is invalid", req.version(), req.keep_alive()));
                }
            }

            model::Token token(token_str);
            auto* player = game_.GetPlayerManager().FindPlayerByToken(token);
            if (!player) {
                return send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", req.version(), req.keep_alive()));
            }

            json::object players_obj;
            for (const auto& p : game_.GetPlayerManager().GetPlayers()) {
                if (p.GetMap() == player->GetMap()) {
                    json::object player_info;
                    auto pos = p.GetPosition();
                    auto vel = p.GetVelocity();
                    
                    player_info["pos"] = json::array{json::value(pos.x), json::value(pos.y)};
                    player_info["speed"] = json::array{json::value(vel.vx), json::value(vel.vy)};

                    switch (p.GetDirection()) {
                        case model::Direction::NORTH: player_info["dir"] = "U"; break;
                        case model::Direction::SOUTH: player_info["dir"] = "D"; break;
                        case model::Direction::WEST:  player_info["dir"] = "L"; break;
                        case model::Direction::EAST:  player_info["dir"] = "R"; break;
                    }

                    players_obj[std::to_string(*p.GetId())] = player_info;
                }
            }

            json::object response_obj;
            response_obj["players"] = players_obj;

            auto response = MakeJsonResponse(http::status::ok, response_obj, req.version(), req.keep_alive());
            
            // Для HEAD-запроса не нужно тело, но заголовки должны быть корректны
            if (req.method() == http::verb::head) {
                response.body() = "";
            }
            
            send(std::move(response));
        }

        http::response<http::string_body> MakeJsonResponse(http::status status,
            const json::value& body_value, unsigned version, bool keep_alive) {
            std::string body = json::serialize(body_value);
            http::response<http::string_body> response(status, version);
            response.set(http::field::content_type, "application/json");
            response.set(http::field::cache_control, "no-cache");
            response.body() = body;
            response.content_length(body.size());
            response.keep_alive(keep_alive);
            return response;
        }

        http::response<http::string_body> MakeErrorResponse(http::status status,
            std::string_view code, std::string_view message,
            unsigned version, bool keep_alive,
            std::string_view allow = "") {
            json::object error_obj;
            error_obj["code"] = std::string(code);
            error_obj["message"] = std::string(message);
            auto response = MakeJsonResponse(status, error_obj, version, keep_alive);
            if (!allow.empty()) {
                response.set(http::field::allow, allow);
            }
            return response;
        }

        model::Game& game_;
    };
} // namespace http_handler
