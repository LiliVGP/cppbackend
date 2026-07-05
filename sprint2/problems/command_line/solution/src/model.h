#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <optional>
#include <cmath>
#include <algorithm>

#include "tagged.h"
#include "player.h"

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

        Road(HorizontalTag, Point start, Coord end_x) noexcept
            : start_{ start }
            , end_{ end_x, start.y } {
        }

        Road(VerticalTag, Point start, Coord end_y) noexcept
            : start_{ start }
            , end_{ start.x, end_y } {
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

    private:
        Point start_;
        Point end_;
    };

    class Building {
    public:
        explicit Building(Rectangle bounds) noexcept
            : bounds_{ bounds } {
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
            : id_{ std::move(id) }
            , position_{ position }
            , offset_{ offset } {
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
            : id_(std::move(id)), name_(std::move(name)), dogSpeed_(std::nullopt) {
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

        void SetDogSpeed(double speed) { dogSpeed_ = speed; }
        double GetDogSpeed(double default_speed) const noexcept {
            return dogSpeed_.value_or(default_speed);
        }

        bool IsOnRoad(double x, double y) const {
            const double half_width = 0.4;
            const double eps = 1e-9;
            
            for (const auto& road : roads_) {
                if (road.IsHorizontal()) {
                    double road_y = road.GetStart().y;
                    if (std::abs(y - road_y) <= half_width + eps) {
                        double x1 = road.GetStart().x;
                        double x2 = road.GetEnd().x;
                        double min_x = std::min(x1, x2);
                        double max_x = std::max(x1, x2);
                        if (x >= min_x - half_width - eps && x <= max_x + half_width + eps) {
                            return true;
                        }
                    }
                } else {
                    double road_x = road.GetStart().x;
                    if (std::abs(x - road_x) <= half_width + eps) {
                        double y1 = road.GetStart().y;
                        double y2 = road.GetEnd().y;
                        double min_y = std::min(y1, y2);
                        double max_y = std::max(y1, y2);
                        if (y >= min_y - half_width - eps && y <= max_y + half_width + eps) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

    private:
        using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

        Id id_;
        std::string name_;
        Roads roads_;
        Buildings buildings_;

        OfficeIdToIndex warehouse_id_to_index_;
        Offices offices_;

        std::optional<double> dogSpeed_;
    };

    class Game {
    public:
        using Maps = std::vector<Map>;

        struct JoinGameResult {
            PlayerId player_id;
            Token auth_token;
        };

        void AddMap(Map map);

        const Maps& GetMaps() const noexcept {
            return maps_;
        }

        Map* FindMap(const Map::Id& id) noexcept {
            if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
                return &maps_.at(it->second);
            }
            return nullptr;
        }

        PlayerManager& GetPlayerManager() { return player_manager_; }

        std::variant<JoinGameResult, std::string> JoinGame(const std::string& user_name, const std::string& map_id);

        void SetDefaultDogSpeed(double speed) { defaultDogSpeed_ = speed; }
        double GetDefaultDogSpeed() const noexcept { return defaultDogSpeed_; }

    private:
        using MapIdHasher = util::TaggedHasher<Map::Id>;
        using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

        std::vector<Map> maps_;
        MapIdToIndex map_id_to_index_;

        PlayerManager player_manager_;

        double defaultDogSpeed_ = 1.0;
    };

}  // namespace model