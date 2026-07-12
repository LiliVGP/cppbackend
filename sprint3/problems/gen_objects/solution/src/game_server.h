#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "model.h"
#include "json_loader.h"

class GameServer {
public:
    GameServer(const std::string& config_path);

    // API методы
    std::string GetMaps() const;
    std::string GetMap(const std::string& map_id) const;
    std::string GetGameState() const;

    void Tick(std::chrono::milliseconds delta);

private:
    std::unique_ptr<ConfigLoader> config_;
    std::unordered_map<std::string, std::unique_ptr<Map>> maps_;
    std::unique_ptr<GameState> game_state_;
    std::chrono::milliseconds time_{ 0 };

    std::string SerializeLootTypes(const ConfigLoader::MapInfo& map_info) const;
    std::string SerializeLootObject(LootId id, const GameState::Loot& loot) const;
};