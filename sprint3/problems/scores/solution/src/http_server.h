#pragma once
#include "sdk.h"

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <memory>
#include <vector>

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

class SessionBase {
public:
    virtual ~SessionBase() = default;
};

template <typename RequestHandler>
class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
public:
    using Request = http::request<http::string_body>;
    using Response = http::response<http::string_body>;

    explicit Session(net::io_context& ioc, RequestHandler handler)
        : socket_(ioc)
        , handler_(std::move(handler)) {
    }

    void Start() {
        DoRead();
    }

    tcp::socket& socket() {
        return socket_;
    }

private:
    void DoRead() {
        auto self = this->shared_from_this();
        req_.clear();
        http::async_read(socket_, buffer_, req_,
            [this, self](beast::error_code ec, size_t bytes_transferred) {
                if (ec == http::error::end_of_stream) {
                    beast::error_code ignored_ec;
                    socket_.shutdown(tcp::socket::shutdown_send, ignored_ec);
                    return;
                }
                if (!ec) {
                    auto sender = [this](Response&& resp) {
                        res_ = std::move(resp);
                        DoWrite();
                    };
                    handler_(std::move(req_), sender);
                } else {
                    OnError(ec);
                }
            });
    }

    void DoWrite() {
        auto self = this->shared_from_this();
        http::async_write(socket_, res_,
            [this, self](beast::error_code ec, size_t bytes_transferred) {
                if (!ec) {
                    if (!res_.keep_alive()) {
                        beast::error_code ignored_ec;
                        socket_.shutdown(tcp::socket::shutdown_send, ignored_ec);
                        return;
                    }
                    DoRead();
                } else {
                    OnError(ec);
                }
            });
    }

    void OnError(beast::error_code ec) {
        if (ec != http::error::end_of_stream && ec != net::error::eof) {
            std::cerr << "Error: " << ec.message() << std::endl;
        }
        beast::error_code ignored_ec;
        socket_.close(ignored_ec);
    }

    tcp::socket socket_;
    beast::flat_buffer buffer_;
    Request req_;
    Response res_;
    RequestHandler handler_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    explicit Listener(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler handler)
        : ioc_(ioc)
        , acceptor_(ioc)
        , handler_(std::move(handler)) {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
    }

    void Start() {
        DoAccept();
    }

private:
    void DoAccept() {
        auto self = this->shared_from_this();
        auto session = std::make_shared<Session<RequestHandler>>(ioc_, handler_);
        acceptor_.async_accept(session->socket(),
            [this, self, session](beast::error_code ec) {
                if (!ec) {
                    session->Start();
                }
                DoAccept();
            });
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler handler_;
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler handler) {
    auto listener = std::make_shared<Listener<RequestHandler>>(ioc, endpoint, std::move(handler));
    listener->Start();
    static std::vector<std::shared_ptr<void>> listeners;
    listeners.push_back(listener);
}

}  // namespace http_server