#include "http_server.h"
#include "boost/asio/dispatch.hpp"
#include "boost/beast/core/bind_handler.hpp"
#include "boost/beast/core/error.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/http/error.hpp"
#include "request_handler.h"

#include <boost/json/string.hpp>
#include <boost/log/trivial.hpp>
#include <cstdlib>

namespace http_server {
    void SessionBase::Run(){
        net::dispatch(stream_.get_executor(),
            beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
    }
    void SessionBase::Read(){
        using namespace std::literals;
        request_ = {};
        stream_.expires_after(30s);
        http::async_read(stream_, buffer_, request_,
            beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
    }
    void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t read_bytes){
        using namespace std::literals;
        if(ec == http::error::end_of_stream){
            Close();
            return;
        }
        if(ec){
            return ReportError(ec, "read"s);
        }
        json::object stop_obj;
        stop_obj["ip"] = stream_.socket().remote_endpoint().address().to_string();
        std::string url(request_.target().begin(), request_.target().end());
        stop_obj["URI"] = url;
        std::string mtd = std::string(request_.method_string());
        stop_obj["method"] = mtd;
        BOOST_LOG_TRIVIAL(info)
        << logging::add_value("data", stop_obj)
        << logging::add_value("msg", "request received"s);
        HandleRequest(std::move(request_));
    }
    void SessionBase::Close(){
        using namespace std::literals;
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        if(ec){
            return ReportError(ec, "close"s);
        }
    }
    void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t write_bytes){
        using namespace std::literals;
        if (ec) {
            return ReportError(ec, "write"s);
        }

        if (close) {
            // Семантика ответа требует закрыть соединение
            return Close();
        }
        Read();
    }
}  // namespace http_server
