#include "http_server.h"
#include <boost/asio/dispatch.hpp>
#include <iostream>

namespace http_server {

    SessionBase::SessionBase(tcp::socket&& socket)
        : stream_(std::move(socket)) {
    }

    void SessionBase::Run() {
        net::dispatch(stream_.get_executor(),
            beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
    }

    void SessionBase::Read() {
        request_ = {};
        stream_.expires_after(std::chrono::seconds(30));

        http::async_read(stream_, buffer_, request_,
            beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
    }

    void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
        if (ec == http::error::end_of_stream) {
            return Close();
        }
        if (ec) {
            return ReportError(ec, "read");
        }
        HandleRequest(std::move(request_));
    }

    void SessionBase::Close() {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        if (ec) {
            std::cerr << "Error during shutdown: " << ec.message() << std::endl;
        }
    }

    void SessionBase::ReportError(beast::error_code ec, std::string_view what) {
        std::cerr << "Error in " << what << ": " << ec.message() << std::endl;
    }

    template <typename Body, typename Fields>
    void SessionBase::Write(http::response<Body, Fields>&& response) {
        auto safe_response = std::make_shared<http::response<Body, Fields>>(std::move(response));

        auto self = GetSharedThis();
        http::async_write(stream_, *safe_response,
            [safe_response, self](beast::error_code ec, std::size_t bytes_written) {
                self->OnWrite(safe_response->need_eof(), ec, bytes_written);
            });
    }

    void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
        if (ec) {
            return ReportError(ec, "write");
        }

        if (close) {
            return Close();
        }

        Read();
    }

    // Явные инстанциации шаблонов Write
    template void SessionBase::Write(http::response<http::string_body>&& response);
    template void SessionBase::Write(http::response<http::empty_body>&& response);

    // ДОБАВЬ ЭТУ СТРОКУ:
    template void SessionBase::Write(http::response<http::file_body>&& response);

}  // namespace http_server
