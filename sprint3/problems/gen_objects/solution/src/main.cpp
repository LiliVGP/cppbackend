#include <iostream>
#include <string>

#include "game_server.h"

int main(int argc, char* argv[]) {
    using namespace std::literals;

    // Парсинг аргументов командной строки
    std::string config_file = "data/config.json";
    std::string www_root = "static";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config-file" && i + 1 < argc) {
            config_file = argv[++i];
        }
        else if (arg == "--www-root" && i + 1 < argc) {
            www_root = argv[++i];
        }
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0]
                << " [--config-file <path>] [--www-root <path>]" << std::endl;
            return 0;
        }
    }

    try {
        // Создаём и запускаем сервер
        GameServer server(config_file);
        server.Run();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}