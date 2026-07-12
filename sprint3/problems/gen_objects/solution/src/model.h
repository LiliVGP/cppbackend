#pragma once
#include <chrono>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

#include "geom.h"
#include "loot_generator.h"
#include "tagged.h"

struct LootTypeTag {};
using LootTypeId = Tagged<LootTypeTag>;

struct PlayerTag {};
using PlayerId = Tagged<PlayerTag>;

struct LootTag {};
using LootId = Tagged<LootTag>;

class Map {
public:
    struct Building {
        Point position;
        double width, height;
    };

    struct Office {
        Point position;
        double offset_x, offset_y;
    };

    Map(std::string id, std::string name, size_t loot_types_count = 0)
        : id_(std::move(id))
        , name_(std::move(name))
        , loot_types_count_(loot_types_count) {
    }

    const std::string& GetId() const { return id_; }
    const std::string& GetName() const { return name_; }

    void AddRoad(const Road& road) { roads_.push_back(road); }
    const std::vector<Road>& GetRoads() const { return roads_; }

    void AddBuilding(const Building& building) { buildings_.push_back(building); }
    const std::vector<Building>& GetBuildings() const { return buildings_; }

    void AddOffice(const Office& office) { offices_.push_back(office); }
    const std::vector<Office>& GetOffices() const { return offices_; }

    size_t GetLootTypesCount() const { return loot_types_count_; }
    void SetLootTypesCount(size_t count) { loot_types_count_ = count; }

private:
    std::string id_;
    std::string name_;
    size_t loot_types_count_ = 0;
    std::vector<Road> roads_;
    std::vector<Building> buildings_;
    std::vector<Office> offices_;
};

class GameState {
public:
    enum class Direction { U, D, L, R };

    struct Player {
        Point position;
        Point speed;
        Direction direction;
    };

    struct Loot {
        LootTypeId type;
        Point position;
    };

    GameState(std::shared_ptr<loot_gen::LootGenerator> loot_generator,
        std::unique_ptr<std::mt19937> rng)
        : loot_generator_(std::move(loot_generator))
        , rng_(std::move(rng)) {
    }

    const std::unordered_map<PlayerId, Player>& GetPlayers() const { return players_; }
    const std::unordered_map<LootId, Loot>& GetLoot() const { return loot_; }
    const Map* GetCurrentMap() const { return current_map_; }

    void SetCurrentMap(const Map* map) { current_map_ = map; }

    void AddPlayer(PlayerId id, const Player& player) {
        players_[id] = player;
    }

    void AddLoot(LootId id, LootTypeId type, const Point& position) {
        loot_[id] = Loot{ type, position };
    }

    std::chrono::milliseconds GetTime() const { return time_; }
    void SetTime(std::chrono::milliseconds time) { time_ = time; }

    // Генерирует новые трофеи за прошедшее время
    void GenerateLoot(std::chrono::milliseconds delta);

private:
    std::unordered_map<PlayerId, Player> players_;
    std::unordered_map<LootId, Loot> loot_;
    const Map* current_map_ = nullptr;
    std::chrono::milliseconds time_{ 0 };
    std::shared_ptr<loot_gen::LootGenerator> loot_generator_;
    std::unique_ptr<std::mt19937> rng_;
    LootId next_loot_id_{ 0 };
};