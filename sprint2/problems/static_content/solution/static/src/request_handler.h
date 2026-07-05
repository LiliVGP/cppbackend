#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <boost/beast/http/file_body.hpp> // Добавили для file_body
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
        // Конструктор теперь принимает путь к статическим файлам
        explicit RequestHandler(model::Game& game, fs::path static_root)
            : game_{ game }, static_root_{ std::move(static_root) } {
        }

        RequestHandler(const RequestHandler&) = delete;
        RequestHandler& operator=(const RequestHandler&) = delete;

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            // Извлекаем декодированный путь к ресурсу
            std::string target = req.target();

            // Если запрос к API, обрабатываем как раньше
            if (target.find("/api/") == 0) {
                return HandleApiRequest(std::forward<decltype(req)>(req), std::forward<Send>(send));
            }

            // Обрабатываем запрос к статическому файлу
            HandleStaticRequest(std::forward<decltype(req)>(req), std::forward<Send>(send));
        }

    private:
        // --- API Обработчики (старые) ---
        template <typename Body, typename Allocator, typename Send>
        void HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            std::string target(req.target());

            if (req.method() == http::verb::get && target == "/api/v1/maps") {
                return send(MakeMapsResponse(req.version(), req.keep_alive()));
            }

            if (req.method() == http::verb::get && target.find("/api/v1/maps/") == 0) {
                std::string map_id = target.substr(std::string("/api/v1/maps/").size());
                return send(MakeMapResponse(map_id, req.version(), req.keep_alive()));
            }

            // Если запрос /api/... не подошел ни под один обработчик, возвращаем Bad Request
            return send(MakeErrorResponse(http::status::bad_request, "badRequest",
                "Bad request", req.version(), req.keep_alive()));
        }

        // --- Статический Обработчик (новый) ---
        template <typename Body, typename Allocator, typename Send>
        void HandleStaticRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            // 1. Поддерживаем только GET и HEAD
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                return send(MakeErrorResponse(http::status::method_not_allowed, "methodNotAllowed",
                    "Method not allowed", req.version(), req.keep_alive()));
            }

            // 2. Декодируем URL
            std::string decoded_target = UrlDecode(req.target());

            // 3. Формируем безопасный путь
            fs::path full_path = static_root_;
            // Заменяем / на разделитель платформы, убираем лишние /, чтобы корректно склеить
            // fs::path поддерживает и / и \, поэтому просто берем target как есть после декодирования
            // и убираем начальный /, если он есть.
            if (!decoded_target.empty() && decoded_target[0] == '/') {
                decoded_target.erase(0, 1);
            }
            full_path /= decoded_target;

            // Если путь пустой или ведет к директории, добавляем index.html
            if (fs::is_directory(full_path)) {
                full_path /= "index.html";
            }

            // 4. Проверяем, что путь находится внутри корневого каталога
            fs::path canonical_path = fs::weakly_canonical(full_path);
            if (!IsSubPath(canonical_path, static_root_)) {
                return send(MakePlainErrorResponse(http::status::bad_request, "Bad request"));
            }

            // 5. Пытаемся открыть файл
            http::response<http::file_body> response;
            response.version(req.version());
            response.result(http::status::ok);
            response.set(http::field::content_type, GetMimeType(full_path.filename().string()));

            beast::error_code ec;
            response.body().open(canonical_path.c_str(), beast::file_mode::read, ec);

            if (ec) {
                // Если файл не найден или ошибка доступа
                return send(MakePlainErrorResponse(http::status::not_found, "Not found"));
            }

            response.prepare_payload();
            response.keep_alive(req.keep_alive());

            // Если это HEAD-запрос, нужно отправить ответ без тела. 
            // file_body для HEAD обрабатывается prepare_payload автоматически (тело не отправится).
            send(std::move(response));
        }

        // --- Вспомогательные функции ---

        // Декодирование URL
        static std::string UrlDecode(std::string_view str) {
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
                        else goto normal_char; // невалидный hex, трактуем как обычный символ
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

        // Проверка, находится ли путь в корневом каталоге
        static bool IsSubPath(const fs::path& path, const fs::path& base) {
            auto it_base = base.begin();
            auto it_path = path.begin();
            while (it_base != base.end() && it_path != path.end()) {
                if (*it_base != *it_path) return false;
                ++it_base;
                ++it_path;
            }
            return it_base == base.end();
        }

        // Определение MIME-типа
        static std::string_view GetMimeType(const fs::path& path) {
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
            // делаем lowercase, так как расширение регистронезависимо
            for (char& c : ext) c = tolower(c);

            auto it = mime_map.find(ext);
            if (it != mime_map.end()) {
                return it->second;
            }
            return "application/octet-stream";
        }

        // --- Генераторы ответов ---

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

        http::response<http::string_body> MakeMapResponse(const std::string& map_id, unsigned version, bool keep_alive) {
            const auto* map = game_.FindMap(model::Map::Id(map_id));
            if (!map) {
                return MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found", version, keep_alive);
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

        // JSON ошибка для API
        http::response<http::string_body> MakeErrorResponse(http::status status, std::string_view code, std::string_view message, unsigned version, bool keep_alive) {
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

        // Обычная текстовая ошибка для статических файлов
        http::response<http::string_body> MakePlainErrorResponse(http::status status, std::string_view message, unsigned version, bool keep_alive) {
            http::response<http::string_body> response(status, version);
            response.set(http::field::content_type, "text/plain");
            response.body() = message;
            response.content_length(message.size());
            response.keep_alive(keep_alive);
            return response;
        }

        model::Game& game_;
        fs::path static_root_; // Корневой путь к статическим файлам
    };

}  // namespace http_handler