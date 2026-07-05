#pragma once
#include "http_server.h"
#include "model.h"
#include "player.h" // Добавили
#include <boost/json.hpp>
#include <boost/beast/http/file_body.hpp>
#include <filesystem>
#include <unordered_map>
#include <sstream>

namespace http_handler {
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace json = boost::json;
    namespace fs = std::filesystem;

    class RequestHandler {
    public:
        // Конструктор теперь принимает и модель игры, и контейнер игроков
        explicit RequestHandler(model::Game& game, fs::path static_root, model::Players& players)
            : game_{ game }, static_root_{ fs::weakly_canonical(static_root) }, players_{ players } {
        }

        RequestHandler(const RequestHandler&) = delete;
        RequestHandler& operator=(const RequestHandler&) = delete;

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            std::string target(req.target().data(), req.target().size());

            if (target.find("/api/") == 0) {
                return HandleApiRequest(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }

            HandleStaticRequest(std::forward<decltype(req)>(req), std::forward<Send>(send));
        }

    private:
        // --- API Обработчики ---
        template <typename Body, typename Allocator, typename Send>
        void HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            std::string target(req.target().data(), req.target().size());

            // Обработка /api/v1/game/join
            if (target == "/api/v1/game/join") {
                if (req.method() == http::verb::post) {
                    return HandleJoinRequest(std::forward<decltype(req)>(req), std::forward<Send>(send));
                } else {
                    return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod",
                        "Only POST method is expected", req.version(), req.keep_alive(),
                        {{"Allow", "POST"}}));
                }
            }

            // Обработка /api/v1/game/players
            if (target == "/api/v1/game/players") {
                if (req.method() == http::verb::get || req.method() == http::verb::head) {
                    return HandlePlayersRequest(std::forward<decltype(req)>(req), std::forward<Send>(send));
                } else {
                    return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod",
                        "Invalid method", req.version(), req.keep_alive(),
                        {{"Allow", "GET, HEAD"}}));
                }
            }

            // --- Обработка старых API запросов (из предыдущего задания) ---
            if (req.method() == http::verb::get && target == "/api/v1/maps") {
                return send(MakeMapsResponse(req.version(), req.keep_alive()));
            }

            if (req.method() == http::verb::get && target.find("/api/v1/maps/") == 0) {
                std::string map_id = target.substr(std::string("/api/v1/maps/").size());
                return send(MakeMapResponse(map_id, req.version(), req.keep_alive()));
            }

            return send(MakeErrorResponse(http::status::bad_request, "badRequest",
                "Bad request", req.version(), req.keep_alive()));
        }

        // --- Новые обработчики API ---

        template <typename Body, typename Allocator, typename Send>
        void HandleJoinRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            // Проверяем Content-Type
            if (req[http::field::content_type] != "application/json") {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument",
                    "Invalid content type", req.version(), req.keep_alive()));
            }

            try {
                // Парсим JSON
                auto json_body = json::parse(req.body()); // Переименовали в json_body
                auto& obj = json_body.as_object();

                // Получаем поля
                if (!obj.contains("userName") || !obj.contains("mapId")) {
                    return send(MakeErrorResponse(http::status::bad_request, "invalidArgument",
                        "Missing required fields", req.version(), req.keep_alive()));
                }

                std::string user_name = json::value_to<std::string>(obj.at("userName"));
                std::string map_id_str = json::value_to<std::string>(obj.at("mapId"));

                // Проверяем имя (не пустое)
                if (user_name.empty()) {
                    return send(MakeErrorResponse(http::status::bad_request, "invalidArgument",
                        "Invalid name", req.version(), req.keep_alive()));
                }

                // Проверяем карту
                auto map_id = model::Map::Id(map_id_str);
                const auto* map = game_.FindMap(map_id);
                if (!map) {
                    return send(MakeErrorResponse(http::status::not_found, "mapNotFound",
                        "Map not found", req.version(), req.keep_alive()));
                }

                // Создаём игрока
                auto& player = players_.Add(std::move(user_name), map_id, game_);

                // Формируем ответ
                json::object resp_obj;
                resp_obj["authToken"] = player.GetToken();
                resp_obj["playerId"] = player.GetId();
                std::string body = json::serialize(resp_obj);

                http::response<http::string_body> response(http::status::ok, req.version());
                response.set(http::field::content_type, "application/json");
                response.set(http::field::cache_control, "no-cache");
                response.body() = body;
                response.content_length(body.size());
                response.keep_alive(req.keep_alive());
                send(std::move(response));

            } catch (const std::exception& e) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument",
                    "Join game request parse error", req.version(), req.keep_alive()));
            }
        }

        template <typename Body, typename Allocator, typename Send>
        void HandlePlayersRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            // Получаем токен из заголовка Authorization
            auto auth_header = req[http::field::authorization];
            if (auth_header.empty()) {
                return send(MakeErrorResponse(http::status::unauthorized, "invalidToken",
                    "Authorization header is missing", req.version(), req.keep_alive()));
            }

            // Проверяем, что заголовок начинается с "Bearer "
            const std::string bearer_prefix = "Bearer ";
            if (auth_header.size() <= bearer_prefix.size() || auth_header.compare(0, bearer_prefix.size(), bearer_prefix) != 0) {
                return send(MakeErrorResponse(http::status::unauthorized, "invalidToken",
                    "Invalid authorization header format", req.version(), req.keep_alive()));
            }

            // Преобразуем string_view в std::string
            std::string token = std::string(auth_header.substr(bearer_prefix.size()));

            // Ищем игрока по токену
            const auto* player = players_.FindByToken(token);
            if (!player) {
                return send(MakeErrorResponse(http::status::unauthorized, "unknownToken",
                    "Player token has not been found", req.version(), req.keep_alive()));
            }

            // Формируем JSON-ответ: { "playerId": {"name": "..."}, ... }
            // В этом задании возвращаем всех игроков.
            json::object players_obj;
            for (const auto& [id, p] : players_.GetPlayers()) {
                json::object player_info;
                player_info["name"] = p->GetName();
                players_obj[std::to_string(id)] = player_info;
            }

            std::string body = json::serialize(players_obj);

            http::response<http::string_body> response(http::status::ok, req.version());
            response.set(http::field::content_type, "application/json");
            response.set(http::field::cache_control, "no-cache");
            response.body() = body;
            response.content_length(body.size());
            response.keep_alive(req.keep_alive());
            send(std::move(response));
        }

        // --- Статический Обработчик (без изменений) ---
        template <typename Body, typename Allocator, typename Send>
        void HandleStaticRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            // ... (код остался таким же, как в исходном файле) ...
            std::string target(req.target().data(), req.target().size());

            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                return send(MakePlainErrorResponse(http::status::method_not_allowed,
                    "Method not allowed", req.version(), req.keep_alive()));
            }

            std::string decoded_target = UrlDecode(target);

            // Формируем путь
            fs::path relative_path = decoded_target;
            // Убираем ведущий слеш, если есть
            if (!relative_path.empty() && *relative_path.begin() == "/") {
                relative_path = relative_path.relative_path();
            }

            fs::path full_path = static_root_ / relative_path;

            // Если путь указывает на директорию, добавляем index.html
            if (fs::is_directory(full_path)) {
                full_path /= "index.html";
            }

            // Проверяем, что путь находится внутри корневого каталога
            fs::path canonical_path = fs::weakly_canonical(full_path);
            if (!IsSubPath(canonical_path, static_root_)) {
                return send(MakePlainErrorResponse(http::status::bad_request,
                    "Bad request", req.version(), req.keep_alive()));
            }

            // Проверяем, что файл существует
            if (!fs::exists(canonical_path)) {
                return send(MakePlainErrorResponse(http::status::not_found,
                    "Not found", req.version(), req.keep_alive()));
            }

            http::response<http::file_body> response;
            response.version(req.version());
            response.result(http::status::ok);
            response.set(http::field::content_type, GetMimeType(full_path.filename().string()));

            beast::error_code ec;
            response.body().open(canonical_path.c_str(), beast::file_mode::read, ec);

            if (ec) {
                return send(MakePlainErrorResponse(http::status::internal_server_error,
                    "Internal server error", req.version(), req.keep_alive()));
            }

            response.prepare_payload();
            response.keep_alive(req.keep_alive());
            send(std::move(response));
        }

        // --- Вспомогательные функции ---

        static std::string UrlDecode(std::string_view str) {
            // ... (код остался таким же) ...
            std::string result;
            result.reserve(str.size());
            for (size_t i = 0; i < str.size(); ++i) {
                if (str[i] == '%' && i + 2 < str.size()) {
                    int value = 0;
                    std::string_view hex = str.substr(i + 1, 2);
                    for (char c : hex) {
                        if (c >= '0' && c <= '9') value = (value << 4) | (c - '0');
                        else if (c >= 'a' && c <= 'f') value = (value << 4) | (c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') value = (value << 4) | (c - 'A' + 10);
                        else goto normal_char;
                    }
                    result += static_cast<char>(value);
                    i += 2;
                    continue;
                }
                if (str[i] == '+') {
                    result += ' ';
                }
                else {
                normal_char:
                    result += str[i];
                }
            }
            return result;
        }

        static bool IsSubPath(const fs::path& path, const fs::path& base) {
            // ... (код остался таким же) ...
            auto norm_path = fs::weakly_canonical(path);
            auto norm_base = fs::weakly_canonical(base);

            auto it_base = norm_base.begin();
            auto it_path = norm_path.begin();

            while (it_base != norm_base.end() && it_path != norm_path.end()) {
                if (*it_base != *it_path) return false;
                ++it_base;
                ++it_path;
            }

            return it_base == norm_base.end();
        }

        static std::string_view GetMimeType(const fs::path& path) {
            // ... (код остался таким же) ...
            static const std::unordered_map<std::string, std::string_view> mime_map = {
                {".htm", "text/html"}, {".html", "text/html"},
                {".css", "text/css"},
                {".txt", "text/plain"},
                {".js", "text/javascript"},
                {".json", "application/json"},
                {".xml", "application/xml"},
                {".png", "image/png"},
                {".jpg", "image/jpeg"}, {".jpe", "image/jpeg"}, {".jpeg", "image/jpeg"},
                {".gif", "image/gif"},
                {".bmp", "image/bmp"},
                {".ico", "image/vnd.microsoft.icon"},
                {".tiff", "image/tiff"}, {".tif", "image/tiff"},
                {".svg", "image/svg+xml"}, {".svgz", "image/svg+xml"},
                {".mp3", "audio/mpeg"}
            };

            auto ext = path.extension().string();
            for (char& c : ext) c = tolower(c);
            auto it = mime_map.find(ext);
            if (it != mime_map.end()) {
                return it->second;
            }
            return "application/octet-stream";
        }

        // --- Генераторы ответов (старые и новые) ---

        http::response<http::string_body> MakeMapsResponse(unsigned version, bool keep_alive) {
            // ... (код остался таким же) ...
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

        http::response<http::string_body> MakeMapResponse(const std::string& map_id, unsigned version, bool keep_alive) {
            // ... (код остался таким же) ...
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

        // Универсальная функция для ошибок с возможностью добавления дополнительных заголовков
        http::response<http::string_body> MakeErrorResponse(http::status status,
            std::string_view code, std::string_view message,
            unsigned version, bool keep_alive,
            std::initializer_list<std::pair<std::string, std::string>> extra_headers = {}) {
            json::object error_obj;
            error_obj["code"] = std::string(code);
            error_obj["message"] = std::string(message);
            std::string body = json::serialize(error_obj);
            http::response<http::string_body> response(status, version);
            response.set(http::field::content_type, "application/json");
            response.set(http::field::cache_control, "no-cache");
            for (const auto& [key, value] : extra_headers) {
                response.set(key, value);
            }
            response.body() = body;
            response.content_length(body.size());
            response.keep_alive(keep_alive);
            return response;
        }

        http::response<http::string_body> MakePlainErrorResponse(http::status status,
            std::string_view message, unsigned version, bool keep_alive) {
            // ... (код остался таким же) ...
            http::response<http::string_body> response(status, version);
            response.set(http::field::content_type, "text/plain");
            response.body() = message;
            response.content_length(message.size());
            response.keep_alive(keep_alive);
            return response;
        }

        model::Game& game_;
        model::Players& players_; // Добавили ссылку на Players
        fs::path static_root_;
    };

}  // namespace http_handler
