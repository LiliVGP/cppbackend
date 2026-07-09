#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <sstream>

namespace http_handler {
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace json = boost::json;

    class RequestHandler {
    public:
        explicit RequestHandler(model::Game& game)
            : game_{ game } {
        }

        RequestHandler(const RequestHandler&) = delete;
        RequestHandler& operator=(const RequestHandler&) = delete;

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {

            std::string target(req.target());

            if (req.method() == http::verb::get && target == "/api/v1/maps") {
                return send(MakeMapsResponse(req.version(), req.keep_alive()));
            }

            if (req.method() == http::verb::get && target.find("/api/v1/maps/") == 0) {
                std::string map_id = target.substr(std::string("/api/v1/maps/").size());
                return send(MakeMapResponse(map_id, req.version(), req.keep_alive()));
            }

            if (target.find("/api/") == 0) {
                return send(MakeErrorResponse(http::status::bad_request, "badRequest",
                    "Bad request", req.version(), req.keep_alive()));
            }

            send(MakeErrorResponse(http::status::not_found, "notFound",
                "Not found", req.version(), req.keep_alive()));
        }

    private:
        http::response<http::string_body> MakeMapsResponse(unsigned version, bool keep_alive) {
            json::array maps_array;

            for (const auto& map : game_.GetMaps()) {
                json::object map_obj;
                map_obj["id"] = std::string(*map.GetId());
                map_obj["name"] = map.GetName();
                maps_array.push_back(map_obj);
            }

            std::string body = json::serialize(maps_array);

            http::response<http::string_body> response(http::status::ok, version);
            response.set(http::field::content_type, "application/json");
            response.body() = body;
            response.content_length(body.size());
            response.keep_alive(keep_alive);
            return response;
        }

        http::response<http::string_body> MakeMapResponse(const std::string& map_id,
            unsigned version, bool keep_alive) {
            const auto* map = game_.FindMap(model::Map::Id(map_id));

            if (!map) {
                return MakeErrorResponse(http::status::not_found, "mapNotFound",
                    "Map not found", version, keep_alive);
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
                }
                else {
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

            std::string body = json::serialize(map_obj);

            http::response<http::string_body> response(http::status::ok, version);
            response.set(http::field::content_type, "application/json");
            response.body() = body;
            response.content_length(body.size());
            response.keep_alive(keep_alive);
            return response;
        }

        http::response<http::string_body> MakeErrorResponse(http::status status,
            std::string_view code,
            std::string_view message,
            unsigned version, bool keep_alive) {
            json::object error_obj;
            error_obj["code"] = std::string(code);
            error_obj["message"] = std::string(message);

            std::string body = json::serialize(error_obj);

            http::response<http::string_body> response(status, version);
            response.set(http::field::content_type, "application/json");
            response.body() = body;
            response.content_length(body.size());
            response.keep_alive(keep_alive);
            return response;
        }

        model::Game& game_;
    };

}  // namespace http_handler
