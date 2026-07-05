#include "boost/asio/ip/address.hpp"
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/time_formatters.hpp>
#include <boost/json/object.hpp>
#include <boost/log/expressions/formatter.hpp>
#include <boost/log/trivial.hpp>
#include <boost/date_time.hpp>
#include "boost/asio/ip/basic_endpoint.hpp"
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/asio/signal_set.hpp>
#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <thread>
#include <signal.h>

#include "json_loader.h"
#include "request_handler.h"
#include "log.h"

using namespace std::literals;
namespace json = boost::json;
namespace net = boost::asio;
namespace http = boost::beast::http;
namespace logging = boost::log;
namespace keywords = boost::log::keywords;

namespace {

// Инициализация логгера с нашим кастомным форматером
void InitLog(){
    logging::add_console_log(
        std::clog,
        keywords::format = &MyFormatter
    );
}

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    //const auto address = net::ip::make_address("127.0.0.1");
    constexpr net::ip::port_type port = 8080;
    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }
    try {
        //Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(argv[1]);
        //загружаем базовый путь статических файлов
        std::filesystem::path base_path(argv[2]);

        //Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        InitLog();

        //Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                json::object stop_obj;
                stop_obj["code"] = 0;
                BOOST_LOG_TRIVIAL(info)
                << logging::add_value("data", stop_obj)
                << logging::add_value("msg", "server exited"s);
                ioc.stop();
            }
        });
        //Создаём обработчик HTTP-запросов и связываем его с моделью игры
        http_handler::RequestHandler handler{game, base_path};

        // Создаем декорированный обработчик запросов для логирования
        http_handler::LoggingRequestHandler logging_handler{handler};

        //Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        net::ip::tcp::endpoint ep = {tcp::v4(), port};

        http_server::ServeHttp(ioc, ep, [&logging_handler](auto&& req, auto&& send) {
            logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        // Логируем запуск сервера
        json::object endp;
        endp["port"] = port;
        endp["address"] = ep.address().to_string();

        BOOST_LOG_TRIVIAL(info)
        << logging::add_value("data", endp)
        << logging::add_value("msg", "server started"s);

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        json::object stop_obj;
        stop_obj["code"] = EXIT_FAILURE;
        stop_obj["exception"] = ex.what();
        BOOST_LOG_TRIVIAL(fatal)
        << logging::add_value("data", stop_obj)
        << logging::add_value("msg", "server exited"s);
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
