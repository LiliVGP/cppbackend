#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "model.h"
#include "json_loader.h"

class GameServer {
public:
    explicit GameServer(const std::string& config_file);
    void Run();

private:
    std::unique_ptr<ConfigLoader> config_;
    std::unordered_map<std::string, std::unique_ptr<Map>> maps_;
    std::unique_ptr<GameState> game_state_;
    std::chrono::milliseconds time_{ 0 };

    void InitializeMaps();
    void InitializeGameState();
    void ProcessTick(std::chrono::milliseconds delta);
};