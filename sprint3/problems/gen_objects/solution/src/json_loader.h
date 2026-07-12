#pragma once
#include <string>
#include <vector>
#include <memory>

#include "model.h"
#include "loot_generator.h"

class ConfigLoader {
public:
    struct LootTypeInfo {
        std::string name;
        std::string file;
        std::string type;
        double rotation = 0;
        std::string color;
        double scale = 1.0;
    };

    struct LootGeneratorConfig {
        std::chrono::milliseconds period;
        double probability;
    };

    struct MapInfo {
        std::string id;
        std::string name;
        std::vector<Road> roads;
        std::vector<Map::Building> buildings;
        std::vector<Map::Office> offices;
        std::vector<LootTypeInfo> loot_types;
    };

    static ConfigLoader LoadFromFile(const std::string& path);

    const std::vector<MapInfo>& GetMaps() const { return maps_; }
    const LootGeneratorConfig& GetLootGeneratorConfig() const { return loot_gen_config_; }

private:
    std::vector<MapInfo> maps_;
    LootGeneratorConfig loot_gen_config_;
};