#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <algorithm>
#include <random>
#include <chrono>
#include <memory>

#include "tagged.h"
#include "loot_generator.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct PlayerPosition {
    double x, y;
};

struct PlayerSpeed {
    double dx, dy;
};

enum class PlayerDirection {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

std::string DirectionToString(PlayerDirection dir);

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    static constexpr double ROAD_HALF_WIDTH = 0.4;

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

    struct RoadBounds {
        double min_x, min_y;
        double max_x, max_y;
    };

    RoadBounds GetBounds() const noexcept {
        RoadBounds bounds;
        bounds.min_x = std::min(start_.x, end_.x) - ROAD_HALF_WIDTH;
        bounds.min_y = std::min(start_.y, end_.y) - ROAD_HALF_WIDTH;
        bounds.max_x = std::max(start_.x, end_.x) + ROAD_HALF_WIDTH;
        bounds.max_y = std::max(start_.y, end_.y) + ROAD_HALF_WIDTH;
        return bounds;
    }

    bool IsPointOnRoad(const PlayerPosition& point) const noexcept {
        auto bounds = GetBounds();
        return point.x >= bounds.min_x && point.x <= bounds.max_x &&
               point.y >= bounds.min_y && point.y <= bounds.max_y;
    }

    PlayerPosition BoundToRoad(const PlayerPosition& point) const noexcept {
        auto bounds = GetBounds();
        PlayerPosition bounded;
        bounded.x = std::max(bounds.min_x, std::min(bounds.max_x, point.x));
        bounded.y = std::max(bounds.min_y, std::min(bounds.max_y, point.y));
        return bounded;
    }

    // Generate a random point on this road
    PlayerPosition RandomPointOnRoad(std::mt19937& rng) const noexcept {
        PlayerPosition pos;
        if (IsHorizontal()) {
            double min_x = std::min(start_.x, end_.x);
            double max_x = std::max(start_.x, end_.x);
            std::uniform_real_distribution<double> dist_x(min_x, max_x);
            pos.x = dist_x(rng);
            pos.y = static_cast<double>(start_.y);
        } else {
            double min_y = std::min(start_.y, end_.y);
            double max_y = std::max(start_.y, end_.y);
            std::uniform_real_distribution<double> dist_y(min_y, max_y);
            pos.x = static_cast<double>(start_.x);
            pos.y = dist_y(rng);
        }
        return pos;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

    double GetDogSpeed(double default_speed) const noexcept {
        return dog_speed_.value_or(default_speed);
    }

    void SetDogSpeed(double speed) noexcept {
        dog_speed_ = speed;
    }

    size_t GetLootTypeCount() const noexcept {
        return loot_type_count_;
    }

    void SetLootTypeCount(size_t count) noexcept {
        loot_type_count_ = count;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    std::optional<double> dog_speed_;
    size_t loot_type_count_ = 0;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Player {
public:
    Player(int id, const std::string& name, PlayerPosition pos, PlayerSpeed speed, PlayerDirection dir)
        : id_(id)
        , name_(name)
        , pos_(pos)
        , speed_(speed)
        , dir_(dir) {
    }

    int GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const PlayerPosition& GetPosition() const noexcept {
        return pos_;
    }

    const PlayerSpeed& GetSpeed() const noexcept {
        return speed_;
    }

    PlayerDirection GetDirection() const noexcept {
        return dir_;
    }

    void SetSpeed(double dx, double dy) noexcept {
        speed_.dx = dx;
        speed_.dy = dy;
    }

    void SetPosition(double x, double y) noexcept {
        pos_.x = x;
        pos_.y = y;
    }

    void SetDirection(PlayerDirection dir) noexcept {
        dir_ = dir;
    }

private:
    int id_;
    std::string name_;
    PlayerPosition pos_;
    PlayerSpeed speed_;
    PlayerDirection dir_;
};

struct LostObject {
    int id;
    int type;
    PlayerPosition pos;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    struct PlayerInfo {
        std::string name;
    };

    using Players = std::unordered_map<int, PlayerInfo>;

    int AddPlayer(const std::string& user_name, const Map::Id& map_id);
    void RegisterPlayer(const std::string& token, const Map::Id& map_id, int player_id);
    const Players& GetPlayersByToken(const std::string& token) const;
    const Player* GetPlayer(int player_id) const;
    double GetDogSpeed(const Map::Id& map_id) const;

    void Tick(int time_delta_ms);

    bool ValidateToken(const std::string& token) const noexcept;
    int GetPlayerIdByToken(const std::string& token) const noexcept;

    static bool IsValidTokenFormat(const std::string& token);

    void SetDefaultDogSpeed(double speed) noexcept {
        default_dog_speed_ = speed;
    }

    double GetDefaultDogSpeed() const noexcept {
        return default_dog_speed_.value_or(1.0);
    }

    const Map::Id* GetMapIdByToken(const std::string& token) const noexcept;

    // Loot generator config
    void SetLootGeneratorConfig(double period, double probability) {
        loot_period_ms_ = static_cast<int>(period * 1000);
        loot_probability_ = probability;
        loot_generator_ = std::make_unique<loot_gen::LootGenerator>(
            std::chrono::milliseconds(loot_period_ms_), probability);
    }

    // Get lost objects for a map
    const std::vector<LostObject>& GetLostObjects(const Map::Id& map_id) const;

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

    int next_player_id_ = 0;
    std::unordered_map<std::string, std::pair<Map::Id, int>> token_to_player_;
    std::unordered_map<int, PlayerInfo> players_;
    std::unordered_map<Map::Id, std::vector<int>, util::TaggedHasher<Map::Id>> map_players_;
    std::unordered_map<int, Player> all_players_;

    std::optional<double> default_dog_speed_;

    // Loot generation
    int loot_period_ms_ = 5000;
    double loot_probability_ = 0.5;
    std::unique_ptr<loot_gen::LootGenerator> loot_generator_;
    std::unordered_map<Map::Id, std::vector<LostObject>, util::TaggedHasher<Map::Id>> map_loot_;
    std::unordered_map<Map::Id, int, util::TaggedHasher<Map::Id>> next_loot_id_;
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace model
