#include "model.h"

#include <stdexcept>
#include <random>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace model {
using namespace std::literals;

namespace {
constexpr size_t TOKEN_SIZE = 32;
}

std::string DirectionToString(PlayerDirection dir) {
    switch (dir) {
        case PlayerDirection::NORTH: return "U";
        case PlayerDirection::SOUTH: return "D";
        case PlayerDirection::WEST: return "L";
        case PlayerDirection::EAST: return "R";
        default: return "U";
    }
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

int Game::AddPlayer(const std::string& user_name, const Map::Id& map_id) {
    const int player_id = next_player_id_++;

    const auto* map = FindMap(map_id);
    if (!map) {
        throw std::runtime_error("Map not found");
    }

    const auto& roads = map->GetRoads();
    if (roads.empty()) {
        throw std::runtime_error("No roads on map");
    }

    const Road& first_road = roads.front();

    PlayerPosition pos;
    PlayerSpeed speed{0.0, 0.0};

    pos.x = static_cast<double>(first_road.GetStart().x);
    pos.y = static_cast<double>(first_road.GetStart().y);

    Player player(player_id, user_name, pos, speed, PlayerDirection::NORTH);
    all_players_.emplace(player_id, std::move(player));

    players_.emplace(player_id, PlayerInfo{user_name});
    return player_id;
}

void Game::RegisterPlayer(const std::string& token, const Map::Id& map_id, int player_id) {
    token_to_player_.emplace(token, std::make_pair(map_id, player_id));
    map_players_[map_id].emplace_back(player_id);
}

const model::Game::Players& Game::GetPlayersByToken(const std::string& token) const {
    static Players empty_players;

    auto it = token_to_player_.find(token);
    if (it == token_to_player_.end()) {
        return empty_players;
    }

    const auto& [map_id, player_id] = it->second;
    const auto* map = FindMap(map_id);
    if (!map) {
        return empty_players;
    }

    static thread_local Players result;
    result.clear();

    auto map_players_it = map_players_.find(map_id);
    if (map_players_it != map_players_.end()) {
        for (int pid : map_players_it->second) {
            auto player_it = players_.find(pid);
            if (player_it != players_.end()) {
                result[pid] = player_it->second;
            }
        }
    }

    return result;
}

const Player* Game::GetPlayer(int player_id) const {
    auto it = all_players_.find(player_id);
    return it != all_players_.end() ? &it->second : nullptr;
}

Player* Game::GetMutablePlayer(int player_id) {
    auto it = all_players_.find(player_id);
    return it != all_players_.end() ? &it->second : nullptr;
}

double Game::GetDogSpeed(const Map::Id& map_id) const {
    const auto* map = FindMap(map_id);
    return map ? map->GetDogSpeed(GetDefaultDogSpeed()) : GetDefaultDogSpeed();
}

bool Game::ValidateToken(const std::string& token) const noexcept {
    return token_to_player_.find(token) != token_to_player_.end();
}

int Game::GetPlayerIdByToken(const std::string& token) const noexcept {
    auto it = token_to_player_.find(token);
    return it != token_to_player_.end() ? it->second.second : -1;
}

bool Game::IsValidTokenFormat(const std::string& token) {
    if (token.size() != TOKEN_SIZE) {
        return false;
    }

    for (char c : token) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

const model::Map::Id* Game::GetMapIdByToken(const std::string& token) const noexcept {
    auto it = token_to_player_.find(token);
    return it != token_to_player_.end() ? &it->second.first : nullptr;
}

namespace {
// Width constants for collision detection (full widths, collision_detector uses half)
constexpr double ITEM_WIDTH = 0.0;
constexpr double DOG_WIDTH = 0.6;
constexpr double OFFICE_WIDTH = 0.5;

class CollisionProvider : public collision_detector::ItemGathererProvider {
public:
    void AddItem(collision_detector::Item item) {
        items_.push_back(std::move(item));
    }

    void AddGatherer(collision_detector::Gatherer gatherer) {
        gatherers_.push_back(std::move(gatherer));
    }

    size_t ItemsCount() const override {
        return items_.size();
    }

    collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_[idx];
    }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};
} // namespace

void Game::Tick(int time_delta_ms) {
    double time_seconds = time_delta_ms / 1000.0;

    // Save start positions before movement for collision detection
    std::unordered_map<int, PlayerPosition> start_positions;

    // Move players and compute new positions
    for (auto& [player_id, player] : all_players_) {
        start_positions[player_id] = player.GetPosition();

        const auto& speed = player.GetSpeed();

        if (speed.dx == 0.0 && speed.dy == 0.0) {
            continue;
        }

        std::string player_token;
        for (const auto& [token, info] : token_to_player_) {
            if (info.second == player_id) {
                player_token = token;
                break;
            }
        }

        if (player_token.empty()) {
            continue;
        }

        const auto* map_id = GetMapIdByToken(player_token);
        if (!map_id) {
            continue;
        }

        const auto* map = FindMap(*map_id);
        if (!map) {
            continue;
        }

        PlayerPosition current_pos = player.GetPosition();
        PlayerPosition new_pos;
        new_pos.x = current_pos.x + speed.dx * time_seconds;
        new_pos.y = current_pos.y + speed.dy * time_seconds;

        const auto& roads = map->GetRoads();
        std::vector<const Road*> start_roads;
        for (const auto& road : roads) {
            if (road.IsPointOnRoad(current_pos)) {
                start_roads.push_back(&road);
            }
        }

        if (start_roads.empty()) {
            continue;
        }

        PlayerPosition bounded_pos = start_roads[0]->BoundToRoad(new_pos);
        double max_dist = 0.0;

        double dx = bounded_pos.x - current_pos.x;
        double dy = bounded_pos.y - current_pos.y;
        max_dist = std::sqrt(dx * dx + dy * dy);

        for (size_t i = 1; i < start_roads.size(); ++i) {
            PlayerPosition pretender_pos = start_roads[i]->BoundToRoad(new_pos);
            double pdx = pretender_pos.x - current_pos.x;
            double pdy = pretender_pos.y - current_pos.y;
            double dist = std::sqrt(pdx * pdx + pdy * pdy);

            if (dist > max_dist) {
                max_dist = dist;
                bounded_pos = pretender_pos;
            }
        }

        bool stopped = (bounded_pos.x != new_pos.x || bounded_pos.y != new_pos.y);

        player.SetPosition(bounded_pos.x, bounded_pos.y);

        auto player_speed = player.GetSpeed();
        if (player_speed.dx > 0) {
            player.SetDirection(PlayerDirection::EAST);
        } else if (player_speed.dx < 0) {
            player.SetDirection(PlayerDirection::WEST);
        } else if (player_speed.dy > 0) {
            player.SetDirection(PlayerDirection::SOUTH);
        } else if (player_speed.dy < 0) {
            player.SetDirection(PlayerDirection::NORTH);
        }

        if (stopped) {
            player.SetSpeed(0.0, 0.0);
        }
    }

    // Process item gathering and office return per map
    for (auto& [map_id, player_ids] : map_players_) {
        if (player_ids.empty()) {
            continue;
        }

        const auto* map = FindMap(map_id);
        if (!map) {
            continue;
        }

        auto& loot = map_loot_[map_id];
        size_t bag_capacity = map->GetBagCapacity(GetDefaultBagCapacity());

        // Build collision provider: items = loot + offices, gatherers = players
        CollisionProvider provider;

        // Add loot as items (width = ITEM_WIDTH / 2 = 0)
        for (size_t i = 0; i < loot.size(); ++i) {
            collision_detector::Item item;
            item.position = geom::Point2D(loot[i].pos.x, loot[i].pos.y);
            item.width = ITEM_WIDTH / 2.0;
            provider.AddItem(item);
        }

        // Add offices as items (width = OFFICE_WIDTH / 2)
        size_t office_count = map->GetOffices().size();
        for (const auto& office : map->GetOffices()) {
            collision_detector::Item item;
            item.position = geom::Point2D(
                static_cast<double>(office.GetPosition().x) + office.GetOffset().dx,
                static_cast<double>(office.GetPosition().y) + office.GetOffset().dy);
            item.width = OFFICE_WIDTH / 2.0;
            provider.AddItem(item);
        }

        // Add players as gatherers (width = DOG_WIDTH / 2)
        for (size_t g = 0; g < player_ids.size(); ++g) {
            int pid = player_ids[g];
            auto* player = GetMutablePlayer(pid);
            if (!player) {
                collision_detector::Gatherer gatherer;
                auto sp = start_positions[pid];
                gatherer.start_pos = geom::Point2D(sp.x, sp.y);
                gatherer.end_pos = gatherer.start_pos;
                gatherer.width = DOG_WIDTH / 2.0;
                provider.AddGatherer(gatherer);
                continue;
            }

            const auto& pos = player->GetPosition();
            auto start = start_positions[pid];

            collision_detector::Gatherer gatherer;
            gatherer.start_pos = geom::Point2D(start.x, start.y);
            gatherer.end_pos = geom::Point2D(pos.x, pos.y);
            gatherer.width = DOG_WIDTH / 2.0;
            provider.AddGatherer(gatherer);
        }

        // Find all collision events
        auto events = collision_detector::FindGatherEvents(provider);

        // Process events in chronological order
        std::set<int> collected_loot_indices;

        for (const auto& evt : events) {
            size_t gatherer_idx = evt.gatherer_id;
            if (gatherer_idx >= player_ids.size()) {
                continue;
            }

            int pid = player_ids[gatherer_idx];
            auto* player = GetMutablePlayer(pid);
            if (!player) {
                continue;
            }

            size_t item_idx = evt.item_id;
            if (item_idx < loot.size()) {
                int loot_idx = static_cast<int>(item_idx);
                if (collected_loot_indices.count(loot_idx)) {
                    continue;
                }

                if (!player->IsBagFull(bag_capacity)) {
                    player->AddToBag(BagItem{loot[loot_idx].id, loot[loot_idx].type});
                    collected_loot_indices.insert(loot_idx);
                }
            } else {
                size_t office_idx = item_idx - loot.size();
                if (office_idx < office_count) {
                    for (const auto& bag_item : player->GetBag()) {
                        player->AddScore(map->GetLootTypeValue(bag_item.type));
                    }
                    player->ClearBag();
                }
            }
        }

        // Remove collected loot from the map
        if (!collected_loot_indices.empty()) {
            std::vector<LostObject> remaining;
            remaining.reserve(loot.size() - collected_loot_indices.size());
            for (size_t i = 0; i < loot.size(); ++i) {
                if (!collected_loot_indices.count(static_cast<int>(i))) {
                    remaining.push_back(std::move(loot[i]));
                }
            }
            loot = std::move(remaining);
        }
    }

    // Generate loot for each map that has players
    if (loot_generator_) {
        for (auto& [map_id, player_ids] : map_players_) {
            if (player_ids.empty()) {
                continue;
            }

            const auto* map = FindMap(map_id);
            if (!map || map->GetLootTypeCount() == 0) {
                continue;
            }

            auto& loot = map_loot_[map_id];
            unsigned loot_count = static_cast<unsigned>(loot.size());
            unsigned looter_count = static_cast<unsigned>(player_ids.size());

            unsigned generated = loot_generator_->Generate(
                std::chrono::milliseconds(time_delta_ms), loot_count, looter_count);

            int& next_id = next_loot_id_[map_id];
            const auto& roads = map->GetRoads();
            if (roads.empty()) {
                continue;
            }

            for (unsigned i = 0; i < generated; ++i) {
                std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
                const Road& road = roads[road_dist(rng_)];
                PlayerPosition pos = road.RandomPointOnRoad(rng_);

                std::uniform_int_distribution<int> type_dist(0, static_cast<int>(map->GetLootTypeCount()) - 1);

                LostObject obj;
                obj.id = next_id++;
                obj.type = type_dist(rng_);
                obj.pos = pos;
                loot.push_back(std::move(obj));
            }
        }
    }
}

const std::vector<LostObject>& Game::GetLostObjects(const Map::Id& map_id) const {
    static const std::vector<LostObject> empty;
    auto it = map_loot_.find(map_id);
    return it != map_loot_.end() ? it->second : empty;
}

}  // namespace model
