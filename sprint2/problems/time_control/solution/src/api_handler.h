#pragma once
#include "http_server.h"
#include "model.h"
#include "authorization.h"
#include <boost/json.hpp>
#include <string_view>
#include <iostream>

namespace http_handler {
    namespace json = boost::json;
    namespace http = boost::beast::http;

    class ApiHandler {
    public:
        explicit ApiHandler(model::Game& game) : game_(game) {}

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            std::string_view target(req.target().data(), req.target().size());

            if (target.find("/api/v1/") != 0) {
                if (target.find("/api/") == 0) {
                    return send(MakeErrorResponse(http::status::bad_request, "badRequest", "Invalid API path", req.version(), req.keep_alive()));
                }
                return send(MakeErrorResponse(http::status::not_found, "notFound", "API endpoint not found", req.version(), req.keep_alive()));
            }

            if (target == "/api/v1/maps") {
                return HandleMaps(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            else if (target.find("/api/v1/maps/") == 0) {
                std::string map_id = std::string(target.substr(std::string("/api/v1/maps/").size()));
                return HandleMap(map_id, std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            else if (target == "/api/v1/game/join") {
                return HandleJoinGame(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            else if (target == "/api/v1/game/players") {
                return HandlePlayers(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            else if (target == "/api/v1/game/state") {
                return HandleGameState(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            else if (target == "/api/v1/game/player/action") {
                return HandlePlayerAction(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            else if (target == "/api/v1/game/tick") {
                return HandleGameTick(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }
            else {
                return send(MakeErrorResponse(http::status::not_found, "notFound", "API endpoint not found", req.version(), req.keep_alive()));
            }
        }

    private:
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

            http::request<http::string_body> string_req;
            string_req.method(req.method());
            string_req.target(req.target());
            string_req.version(req.version());
            string_req.body() = req.body();
            for (auto const& field : req) {
                string_req.set(field.name_string(), field.value());
            }

            ExecuteAuthorized(string_req, std::forward<Send>(send), [&](model::Player* player) {
                json::object players_obj;
                for (const auto& pair : game_.GetPlayerManager().GetPlayers()) {
                    const auto& p = pair.second;
                    if (p.GetMap() == player->GetMap()) {
                        json::object player_obj;
                        player_obj["name"] = p.GetName();
                        players_obj[std::to_string(*p.GetId())] = player_obj;
                    }
                }

                auto response = MakeJsonResponse(http::status::ok, players_obj, req.version(), req.keep_alive());
                send(std::move(response));
            });
        }

        template <typename Body, typename Allocator, typename Send>
        void HandleGameState(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req.version(), req.keep_alive(), "GET, HEAD"));
            }

            http::request<http::string_body> string_req;
            string_req.method(req.method());
            string_req.target(req.target());
            string_req.version(req.version());
            string_req.body() = req.body();
            for (auto const& field : req) {
                string_req.set(field.name_string(), field.value());
            }

            ExecuteAuthorized(string_req, std::forward<Send>(send), [&](model::Player* player) {
                // ОТЛАДКА
                std::cout << "DEBUG: HandleGameState called" << std::endl;
                std::cout << "DEBUG: Total players: " << game_.GetPlayerManager().GetPlayers().size() << std::endl;
                
                json::object players_obj;
                for (const auto& pair : game_.GetPlayerManager().GetPlayers()) {
                    const auto& p = pair.second;
                    std::cout << "DEBUG: Found player: " << *p.GetId() << ", name: " << p.GetName() << std::endl;
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
                send(std::move(response));
            });
        }

        template <typename Body, typename Allocator, typename Send>
        void HandlePlayerAction(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            if (req.method() != http::verb::post) {
                return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req.version(), req.keep_alive(), "POST"));
            }

            auto content_type = req.find(http::field::content_type);
            if (content_type == req.end() || content_type->value() != "application/json") {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid content type", req.version(), req.keep_alive()));
            }

            http::request<http::string_body> string_req;
            string_req.method(req.method());
            string_req.target(req.target());
            string_req.version(req.version());
            string_req.body() = req.body();
            for (auto const& field : req) {
                string_req.set(field.name_string(), field.value());
            }

            ExecuteAuthorized(string_req, std::forward<Send>(send), [&](model::Player* player) {
                try {
                    auto json_body = json::parse(req.body()).as_object();
                    
                    if (!json_body.contains("move") || !json_body.at("move").is_string()) {
                        throw std::runtime_error("Missing or invalid move");
                    }
                    
                    std::string move = std::string(json_body.at("move").as_string());
                    
                    double speed = player->GetMap()->GetDogSpeed(game_.GetDefaultDogSpeed());
                    model::Velocity new_vel{0.0, 0.0};
                    model::Direction new_dir = player->GetDirection();

                    if (move == "L") {
                        new_vel = {-speed, 0.0};
                        new_dir = model::Direction::WEST;
                    } else if (move == "R") {
                        new_vel = {speed, 0.0};
                        new_dir = model::Direction::EAST;
                    } else if (move == "U") {
                        new_vel = {0.0, -speed};
                        new_dir = model::Direction::NORTH;
                    } else if (move == "D") {
                        new_vel = {0.0, speed};
                        new_dir = model::Direction::SOUTH;
                    } else if (move == "") {
                        new_vel = {0.0, 0.0};
                    } else {
                        throw std::runtime_error("Invalid move value");
                    }

                    player->SetVelocity(new_vel);
                    player->SetDirection(new_dir);

                    auto response = MakeJsonResponse(http::status::ok, json::object(), req.version(), req.keep_alive());
                    send(std::move(response));

                } catch (const std::exception&) {
                    return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", req.version(), req.keep_alive()));
                }
            });
        }

        template <typename Body, typename Allocator, typename Send>
        void HandleGameTick(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            if (req.method() != http::verb::post) {
                return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", req.version(), req.keep_alive(), "POST"));
            }

            auto content_type = req.find(http::field::content_type);
            if (content_type == req.end() || content_type->value() != "application/json") {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid content type", req.version(), req.keep_alive()));
            }

            try {
                auto json_body = json::parse(req.body()).as_object();
                
                if (!json_body.contains("timeDelta") || !json_body.at("timeDelta").is_number()) {
                    throw std::runtime_error("Missing or invalid timeDelta");
                }
                
                int64_t time_delta_ms = json_body.at("timeDelta").as_int64();
                if (time_delta_ms <= 0) {
                    throw std::runtime_error("timeDelta must be positive");
                }
                
                double delta_time = time_delta_ms / 1000.0; // конвертируем в секунды
                
                auto& players = game_.GetPlayerManager().GetPlayers();
                for (auto& pair : players) {
                    auto& player = pair.second;
                    model::Map* map = player.GetMap();
                    if (map) {
                        player.UpdatePosition(delta_time, *map);
                    }
                }
                
                auto response = MakeJsonResponse(http::status::ok, json::object(), req.version(), req.keep_alive());
                send(std::move(response));
                
            } catch (const std::exception&) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", req.version(), req.keep_alive()));
            }
        }

        template <typename Fn, typename Send>
        void ExecuteAuthorized(http::request<http::string_body>& req, Send&& send, Fn&& action) {
            auto token_opt = TryExtractToken(req);
            if (!token_opt) {
                return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", req.version(), req.keep_alive()));
            }

            auto* player = game_.GetPlayerManager().FindPlayerByToken(*token_opt);
            if (!player) {
                return send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", req.version(), req.keep_alive()));
            }

            action(player);
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

    private:
        model::Game& game_;
    };

} // namespace http_handler