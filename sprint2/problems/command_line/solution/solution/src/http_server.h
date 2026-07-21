#pragma once
#include "sdk.h"

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <memory>

namespace http_server {

    namespace net = boost::asio;
    using tcp = net::ip::tcp;
    namespace beast = boost::beast;
    namespace http = beast::http;

    class SessionBase {
    public:
        SessionBase(const SessionBase&) = delete;
        SessionBase& operator=(const SessionBase&) = delete;

        void Run();

    protected:
        explicit SessionBase(tcp::socket&& socket);
        virtual ~SessionBase() = default;

        virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;
        virtual void HandleRequest(http::request<http::string_body>&& request) = 0;

        template <typename Body, typename Fields>
        void Write(http::response<Body, Fields>&& response);

    private:
        using HttpRequest = http::request<http::string_body>;

        void Read();
        void OnRead(beast::error_code ec, std::size_t bytes_read);
        void Close();
        void ReportError(beast::error_code ec, std::string_view what);

        void OnWrite(bool close, beast::error_code ec, std::size_t bytes_written);

        beast::tcp_stream stream_;
        beast::flat_buffer buffer_;
        HttpRequest request_;
    };

    template <typename RequestHandler>
    class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
    public:
        template <typename Handler>
        Session(tcp::socket&& socket, Handler&& request_handler)
            : SessionBase(std::move(socket))
            , request_handler_(std::forward<Handler>(request_handler)) {
        }

    private:
        std::shared_ptr<SessionBase> GetSharedThis() override {
            return this->shared_from_this();
        }

        void HandleRequest(http::request<http::string_body>&& request) override {
            request_handler_(std::move(request), [self = this->shared_from_this()](auto&& response) {
                self->Write(std::move(response));
                });
        }

        RequestHandler request_handler_;
    };

    template <typename RequestHandler>
    class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
    public:
        template <typename Handler>
        Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& handler)
            : ioc_(ioc)
            , acceptor_(net::make_strand(ioc))
            , request_handler_(std::forward<Handler>(handler)) {
            beast::error_code ec;
            acceptor_.open(endpoint.protocol(), ec);
            if (ec) {
                throw std::system_error(ec);
            }
            acceptor_.set_option(net::socket_base::reuse_address(true), ec);
            if (ec) {
                throw std::system_error(ec);
            }
            acceptor_.bind(endpoint, ec);
            if (ec) {
                throw std::system_error(ec);
            }
            acceptor_.listen(net::socket_base::max_listen_connections, ec);
            if (ec) {
                throw std::system_error(ec);
            }
        }

        void Run() {
            DoAccept();
        }

    private:
        void DoAccept() {
            acceptor_.async_accept(net::make_strand(ioc_),
                [self = this->shared_from_this()](beast::error_code ec, tcp::socket socket) {
                    if (!ec) {
                        self->AsyncRunSession(std::move(socket));
                    }
                    self->DoAccept();
                });
        }

        void AsyncRunSession(tcp::socket&& socket) {
            std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
        }

        net::io_context& ioc_;
        tcp::acceptor acceptor_;
        RequestHandler request_handler_;
    };

    template <typename RequestHandler>
    void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
        using MyListener = Listener<std::decay_t<RequestHandler>>;
        std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
    }

}  // namespace http_server