#pragma once
#include "http_server.h"
#include "model.h"
#include "api_handler.h" // Добавлен новый заголовок
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
        explicit RequestHandler(model::Game& game, fs::path static_root)
            : game_{ game }, static_root_{ fs::weakly_canonical(static_root) }, api_handler_{ game } {
        }

        RequestHandler(const RequestHandler&) = delete;
        RequestHandler& operator=(const RequestHandler&) = delete;

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            std::string target(req.target().data(), req.target().size());

            if (target.find("/api/") == 0) {
                // Делегируем обработку API-запросов классу ApiHandler
                api_handler_(std::forward<decltype(req)>(req), std::forward<Send>(send));
                return;
            }

            HandleStaticRequest(std::forward<decltype(req)>(req), std::forward<Send>(send));
        }

    private:
        // --- API Обработчики (Удалены, перемещены в ApiHandler) ---

        // --- Вспомогательные функции для API (Удалены, перемещены в ApiHandler) ---

        // --- Статический Обработчик ---
        template <typename Body, typename Allocator, typename Send>
        void HandleStaticRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            std::string target(req.target().data(), req.target().size());

            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                return send(MakePlainErrorResponse(http::status::method_not_allowed,
                    "Method not allowed", req.version(), req.keep_alive()));
            }

            std::string decoded_target = UrlDecode(target);

            fs::path relative_path = decoded_target;
            if (!relative_path.empty() && *relative_path.begin() == "/") {
                relative_path = relative_path.relative_path();
            }

            fs::path full_path = static_root_ / relative_path;

            if (fs::is_directory(full_path)) {
                full_path /= "index.html";
            }

            fs::path canonical_path = fs::weakly_canonical(full_path);
            if (!IsSubPath(canonical_path, static_root_)) {
                return send(MakePlainErrorResponse(http::status::bad_request,
                    "Bad request", req.version(), req.keep_alive()));
            }

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

        // --- Вспомогательные функции для статики ---
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

        http::response<http::string_body> MakePlainErrorResponse(http::status status,
            std::string_view message, unsigned version, bool keep_alive) {
            http::response<http::string_body> response(status, version);
            response.set(http::field::content_type, "text/plain");
            response.body() = message;
            response.content_length(message.size());
            response.keep_alive(keep_alive);
            return response;
        }

        model::Game& game_;
        fs::path static_root_;

        // Добавлен экземпляр ApiHandler
        ApiHandler api_handler_;
    };

}  // namespace http_handler