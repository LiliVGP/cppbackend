#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <boost/json.hpp>

#include "model.h"
#include "json_loader.h"

class GameServer {
public:
    explicit GameServer(const std::string& config_file);
    void Run();

    boost::json::object GetMapInfo(const std::string& id) const;
    boost::json::object GetGameState() const;

    const Map* GetMap(const std::string& id) const;
    void AddPlayer(PlayerId id, const GameState::Player& player);

    void ProcessTick(std::chrono::milliseconds delta);

private:
    std::unique_ptr<ConfigLoader> config_;
    std::unordered_map<std::string, std::unique_ptr<Map>> maps_;
    std::unordered_map<std::string, ConfigLoader::MapInfo> map_infos_;
    std::unique_ptr<GameState> game_state_;
    std::chrono::milliseconds time_{ 0 };

    void InitializeMaps();
    void InitializeGameState();
};