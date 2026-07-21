#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <optional>
#include <string_view>
#include <algorithm>

namespace http_handler {
    namespace http = boost::beast::http;

    inline std::optional<model::Token> TryExtractToken(const http::request<http::string_body>& req) {
        auto auth_header = req.find(http::field::authorization);
        if (auth_header == req.end()) {
            return std::nullopt;
        }

        std::string_view auth_value = auth_header->value();
        const std::string_view bearer_prefix = "Bearer ";

        if (auth_value.size() <= bearer_prefix.size() ||
            auth_value.substr(0, bearer_prefix.size()) != bearer_prefix) {
            return std::nullopt;
        }

        auto is_hex_digit = [](char c) -> bool {
            return (c >= '0' && c <= '9') ||
                   (c >= 'a' && c <= 'f') ||
                   (c >= 'A' && c <= 'F');
        };

        if (!std::all_of(token_str.begin(), token_str.end(), is_hex_digit)) {
            return std::nullopt;
        }

        return model::Token(std::move(token_str));
    }

} // namespace http_handler
