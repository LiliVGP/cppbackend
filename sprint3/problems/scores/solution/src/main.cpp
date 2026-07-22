#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <string>
#include <string_view>

#include "json_loader.h"
#include "request_handler.h"
#include "extra_data.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

struct Args {
    std::string config_file;
    std::string www_root;
};

Args ParseArgs(int argc, const char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--config-file"sv && i + 1 < argc) {
            args.config_file = argv[++i];
        } else if (arg == "--www-root"sv && i + 1 < argc) {
            args.www_root = argv[++i];
        }
    }
    if (args.config_file.empty()) {
        throw std::invalid_argument("Usage: game_server --config-file <config> --www-root <static-dir>"s);
    }
    if (args.www_root.empty()) {
        args.www_root = ".";
    }
    return args;
}

}  // namespace

int main(int argc, const char* argv[]) {
    try {
        Args args = ParseArgs(argc, argv);

        extra_data::GameExtraData extra_data;

        model::Game game = json_loader::LoadGame(args.config_file, extra_data);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        http_handler::RequestHandler handler{game, extra_data, args.www_root};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        std::cout << "Server has started..."sv << std::endl;

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
