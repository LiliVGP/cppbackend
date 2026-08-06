#pragma once

#include <boost/beast/http.hpp>
#include <string>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <map>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;

namespace util {

inline void UrlDecode(const std::string& src, std::string& dst) {
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

inline std::string GetMimeType(const std::string& extension) {
    static const std::map<std::string, std::string> types = {
        {".htm", "text/html"}, {".html", "text/html"},
        {".css", "text/css"}, {".txt", "text/plain"},
        {".js", "text/javascript"}, {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".jpe", "image/jpeg"}, {".gif", "image/gif"}, {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"}, {".tiff", "image/tiff"},
        {".tif", "image/tiff"}, {".svg", "image/svg+xml"}, {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"},
    };

    std::string ext = extension;
    for (auto& c : ext) c = std::tolower(c);

    auto it = types.find(ext);
    return it != types.end() ? it->second : "application/octet-stream";
}

inline std::map<std::string, std::string> ParseQuery(std::string_view query) {
    std::map<std::string, std::string> params;
    while (!query.empty()) {
        auto sep = query.find('&');
        std::string_view param = (sep != std::string_view::npos) ? query.substr(0, sep) : query;
        auto eq = param.find('=');
        if (eq != std::string_view::npos) {
            params[std::string(param.substr(0, eq))] = std::string(param.substr(eq + 1));
        }
        if (sep == std::string_view::npos) break;
        query = query.substr(sep + 1);
    }
    return params;
}

}  // namespace util
}  // namespace http_handler
