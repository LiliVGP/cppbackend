#pragma once
#include "boost/beast/http/message.hpp"
#include "boost/beast/http/status.hpp"
#include "boost/beast/http/string_body.hpp"
#include "boost/beast/http/verb.hpp"
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/time_formatters.hpp>
#include <boost/json.hpp>
#include "boost/json/array.hpp"
#include "boost/json/object.hpp"
#include "boost/json/serialize.hpp"
#include "http_server.h"
#include "model.h"
#include <string>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, const std::filesystem::path& base_path)
        : game_{game}, base_path_{base_path} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    json::array OfficesSerialize(const model::Map* map_ptr){
        json::array j_offices;
        model::Map::Offices offices = map_ptr->GetOffices();

        for(const model::Office office : offices){
            auto position = office.GetPosition();
            auto offset = office.GetOffset();
            json::object j_office{
                {"id", *office.GetId()},
                {"x", position.x},
                {"y", position.y},
                {"offsetX", offset.dx},
                {"offsetY", offset.dy}
            };
            j_offices.push_back(j_office);
        }

        return j_offices;
    }

    json::array BuildingsSerialize(const model::Map* map_ptr){
        json::array j_buildings;
        model::Map::Buildings buildings = map_ptr->GetBuildings();

        for(const model::Building& building : buildings){
            auto rectangle = building.GetBounds();
            json::object j_rectangle;
            j_rectangle["x"] = rectangle.position.x;
            j_rectangle["y"] = rectangle.position.y;
            j_rectangle["w"] = rectangle.size.width;
            j_rectangle["h"] = rectangle.size.height;
            j_buildings.push_back(j_rectangle);
        }

        return j_buildings;
    }

    json::array RoadsSerialize(const model::Map* map_ptr){
        json::array j_roads;
        for (const auto& road : map_ptr->GetRoads()) {
            json::object j_road{{"x0", road.GetStart().x}, {"y0", road.GetStart().y}};
            if (road.IsHorizontal()) j_road["x1"] = road.GetEnd().x;
            else j_road["y1"] = road.GetEnd().y;
            j_roads.push_back(std::move(j_road));
        }
        return j_roads;
    }

    json::object MapSerialize(const model::Map* map_ptr){
        json::object j_map;
        j_map["id"] = *map_ptr->GetId();
        j_map["name"] = map_ptr->GetName();
        json::array j_roads = RoadsSerialize(map_ptr);
        j_map["roads"] = std::move(j_roads);
        json::array j_buildings = BuildingsSerialize(map_ptr);
        j_map["buildings"] = std::move(j_buildings);
        json::array j_offices = OfficesSerialize(map_ptr);
        j_map["offices"] = std::move(j_offices);
        return j_map;
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        using namespace std::literals;

        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            send([&]{
                http::response<http::string_body> res{http::status::method_not_allowed, req.version()};
                res.set(http::field::content_type, "application/json");
                res.set(http::field::allow, "GET, HEAD");
                res.body() = "{\"code\": \"invalidMethod\", \"message\": \"Only GET and HEAD methods are allowed\"}";
                res.prepare_payload();
                res.keep_alive(req.keep_alive());
                return res;
            }());
            return;
        }

        std::string target = {req.target().begin(), req.target().end()};

        auto text_response = [&](http::status status, std::string_view body, std::string_view type = "application/json") {
            http::response<http::string_body> res{status, req.version()};
            res.set(http::field::content_type, type);
            res.body() = std::string(body);
            res.prepare_payload();
            res.keep_alive(req.keep_alive());
            return res;
        };

        if (target == maps_path_) {
            json::array j_maps;
            for (const auto& map : game_.GetMaps()) {
                j_maps.push_back({{"id", *map.GetId()}, {"name", map.GetName()}});
            }
            send(text_response(http::status::ok, json::serialize(j_maps)));

        } else if (target.starts_with(maps_path_ + "/")) {
            int start = static_cast<int>(maps_path_.size()) + 1;
            model::Map::Id id{target.substr(start)};
            const model::Map* map_ptr = game_.FindMap(id);

            if (map_ptr) {
                json::object j_map = MapSerialize(map_ptr);
                send(text_response(http::status::ok, json::serialize(j_map)));
            } else {
                send(text_response(http::status::not_found,
                    "{\"code\": \"mapNotFound\", \"message\": \"Map not found\"}"));
            }

        } else if (target.starts_with("/api/")) {
            send(text_response(http::status::bad_request,
                "{\"code\": \"badRequest\", \"message\": \"Bad request\"}"));

        } else {
            // Обработка статических файлов
            std::string decoded_url = UrlDecoded(target);
            
            // Проверяем, не содержит ли путь ".." для обхода директорий
            if (decoded_url.find("..") != std::string::npos) {
                send(text_response(http::status::bad_request, "Access denied"sv, "text/plain"sv));
                return;
            }

            // Убираем ведущий слеш, если он есть, чтобы сделать путь относительным
            if (!decoded_url.empty() && decoded_url[0] == '/') {
                decoded_url = decoded_url.substr(1);
            }
            if (decoded_url.empty()) {
                decoded_url = "index.html";
            }

            std::filesystem::path full_path = std::filesystem::weakly_canonical(base_path_ / decoded_url);
            
            // Проверяем, что полный путь находится внутри base_path_ (дополнительная проверка)
            auto relative_path = std::filesystem::relative(full_path, base_path_);
            if (relative_path.empty() || relative_path.string().rfind("..", 0) == 0) {
                send(text_response(http::status::bad_request, "Access denied"sv, "text/plain"sv));
                return;
            }

            std::string content_type = ContentType(full_path);
            http::file_body::value_type file;
            boost::system::error_code ec;
            file.open(full_path.string().c_str(), beast::file_mode::read, ec);

            if (ec) {
                send(text_response(http::status::not_found, "File not found"sv, "text/plain"sv));
                return;
            }

            http::response<http::file_body> response;
            response.version(req.version());
            response.result(http::status::ok);
            response.insert(http::field::content_type, content_type);
            response.body() = std::move(file);
            response.prepare_payload();
            response.keep_alive(req.keep_alive());
            send(std::move(response));
        }
    }

private:
    std::string UrlDecoded(const std::string& url_encoded) {
        std::string res;
        res.reserve(url_encoded.size());
        for (size_t i = 0; i < url_encoded.size(); ++i) {
            if (url_encoded[i] == '%' && i + 2 < url_encoded.size()) {
                std::string hex_str = url_encoded.substr(i + 1, 2);
                res.push_back(static_cast<char>(std::strtol(hex_str.c_str(), nullptr, 16)));
                i += 2;
            } else if (url_encoded[i] == '+') {
                res.push_back(' ');
            } else {
                res.push_back(url_encoded[i]);
            }
        }
        return res;
    }

    const std::string ContentType(const std::filesystem::path& path) {
        std::string type = path.extension().string();
        std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c){ return tolower(c); });

        if (type == ".htm" || type == ".html") return "text/html";
        if (type == ".css")                    return "text/css";
        if (type == ".txt")                    return "text/plain";
        if (type == ".js")                     return "text/javascript";
        if (type == ".json")                   return "application/json";
        if (type == ".xml")                    return "application/xml";
        if (type == ".png")                    return "image/png";
        if (type == ".jpg" || type == ".jpe" || type == ".jpeg") return "image/jpeg";
        if (type == ".gif")                    return "image/gif";
        if (type == ".bmp")                    return "image/bmp";
        if (type == ".ico")                    return "image/vnd.microsoft.icon";
        if (type == ".tiff" || type == ".tif") return "image/tiff";
        if (type == ".svg" || type == ".svgz") return "image/svg+xml";
        if (type == ".mp3")                    return "audio/mpeg";
        return "application/octet-stream";
    }

    model::Game& game_;
    std::filesystem::path base_path_;
    const std::string maps_path_ = "/api/v1/maps";
};

// Декоратор для логирования ответов
template <typename RequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(RequestHandler& handler) : handler_(handler) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        using namespace std::literals;

        // Замеряем время начала обработки
        auto start_time = boost::posix_time::microsec_clock::universal_time();

        // Передаем запрос оригинальному обработчику, оборачивая функцию отправки
        handler_(std::forward<decltype(req)>(req), [this, start_time, send = std::forward<Send>(send)](auto&& response) {
            // Вычисляем время ответа
            auto end_time = boost::posix_time::microsec_clock::universal_time();
            auto duration_ms = (end_time - start_time).total_milliseconds();

            json::object resp_obj;
            resp_obj["response_time"] = static_cast<std::size_t>(duration_ms);
            resp_obj["code"] = response.result_int();
            // Получаем content_type из заголовка ответа, если есть
            auto content_type_it = response.find(http::field::content_type);
            if (content_type_it != response.end()) {
                resp_obj["content_type"] = std::string(content_type_it->value());
            } else {
                resp_obj["content_type"] = nullptr;
            }
            
            BOOST_LOG_TRIVIAL(info)
            << logging::add_value("data", resp_obj)
            << logging::add_value("msg", "response sent"s);

            // Отправляем ответ оригинальному отправителю
            send(std::forward<decltype(response)>(response));
        });
    }

private:
    RequestHandler& handler_;
};

}  // namespace http_handler
