#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include "game_server.h"

int main(int argc, char* argv[]) {
    using namespace std::literals;

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
        // Проверка существования конфигурационного файла
        if (!std::filesystem::exists(config_file)) {
            throw std::runtime_error("Config file not found: " + config_file);
        }

        GameServer server(config_file);
        server.Run();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
}