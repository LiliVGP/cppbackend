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
    player_to_token_.emplace(player_id, token);
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

std::vector<RetiredPlayer> Game::Tick(int time_delta_ms) {
    start_positions_global_.clear();

    MovePlayers(time_delta_ms);
    ProcessCollisions();
    GenerateLoot(time_delta_ms);

    return RetireInactivePlayers();
}

void Game::MovePlayers(int time_delta_ms) {
    double time_seconds = time_delta_ms / 1000.0;

    for (auto& [player_id, player] : all_players_) {
        start_positions_global_[player_id] = player.GetPosition();

        const auto& speed = player.GetSpeed();
        player.IncrementPlayTime(time_delta_ms);

        if (speed.dx == 0.0 && speed.dy == 0.0) {
            player.IncrementInactiveTime(time_delta_ms);
            continue;
        }

        player.ResetInactiveTime();

        // Look up map via reverse token index
        auto token_it = player_to_token_.find(player_id);
        if (token_it == player_to_token_.end()) continue;

        const auto* map_id = GetMapIdByToken(token_it->second);
        if (!map_id) continue;

        const auto* map = FindMap(*map_id);
        if (!map) continue;

        PlayerPosition current_pos = player.GetPosition();
        PlayerPosition new_pos{
            current_pos.x + speed.dx * time_seconds,
            current_pos.y + speed.dy * time_seconds
        };

        // Find all roads that the player is currently on
        std::vector<const Road*> start_roads;
        for (const auto& road : map->GetRoads()) {
            if (road.IsPointOnRoad(current_pos)) {
                start_roads.push_back(&road);
            }
        }
        if (start_roads.empty()) continue;

        // Pick the road that allows the furthest movement
        PlayerPosition bounded_pos = start_roads[0]->BoundToRoad(new_pos);
        double max_dist = std::sqrt(
            std::pow(bounded_pos.x - current_pos.x, 2) +
            std::pow(bounded_pos.y - current_pos.y, 2));

        for (size_t i = 1; i < start_roads.size(); ++i) {
            PlayerPosition candidate = start_roads[i]->BoundToRoad(new_pos);
            double dist = std::sqrt(
                std::pow(candidate.x - current_pos.x, 2) +
                std::pow(candidate.y - current_pos.y, 2));
            if (dist > max_dist) {
                max_dist = dist;
                bounded_pos = candidate;
            }
        }

        bool stopped = (bounded_pos.x != new_pos.x || bounded_pos.y != new_pos.y);
        player.SetPosition(bounded_pos.x, bounded_pos.y);

        // Update direction based on speed
        if (speed.dx > 0) {
            player.SetDirection(PlayerDirection::EAST);
        } else if (speed.dx < 0) {
            player.SetDirection(PlayerDirection::WEST);
        } else if (speed.dy > 0) {
            player.SetDirection(PlayerDirection::SOUTH);
        } else if (speed.dy < 0) {
            player.SetDirection(PlayerDirection::NORTH);
        }

        if (stopped) {
            player.SetSpeed(0.0, 0.0);
        }
    }
}

void Game::ProcessCollisions() {
    for (auto& [map_id, player_ids] : map_players_) {
        if (player_ids.empty()) continue;

        const auto* map = FindMap(map_id);
        if (!map) continue;

        auto& loot = map_loot_[map_id];
        size_t bag_capacity = map->GetBagCapacity(GetDefaultBagCapacity());

        CollisionProvider provider;

        // Add loot as items
        for (const auto& obj : loot) {
            provider.AddItem({
                geom::Point2D(obj.pos.x, obj.pos.y),
                ITEM_WIDTH / 2.0
            });
        }

        // Add offices as items
        size_t office_count = map->GetOffices().size();
        for (const auto& office : map->GetOffices()) {
            provider.AddItem({
                geom::Point2D(
                    static_cast<double>(office.GetPosition().x) + office.GetOffset().dx,
                    static_cast<double>(office.GetPosition().y) + office.GetOffset().dy),
                OFFICE_WIDTH / 2.0
            });
        }

        // Add players as gatherers
        for (int pid : player_ids) {
            auto* player = GetMutablePlayer(pid);
            auto sp = start_positions_global_[pid];            geom::Point2D start(sp.x, sp.y);
            geom::Point2D end = player ? geom::Point2D(player->GetPosition().x, player->GetPosition().y) : start;
            provider.AddGatherer({start, end, DOG_WIDTH / 2.0});
        }

        auto events = collision_detector::FindGatherEvents(provider);
        std::set<int> collected_loot_indices;

        for (const auto& evt : events) {
            if (evt.gatherer_id >= player_ids.size()) continue;

            int pid = player_ids[evt.gatherer_id];
            auto* player = GetMutablePlayer(pid);
            if (!player) continue;

            if (evt.item_id < loot.size()) {
                int loot_idx = static_cast<int>(evt.item_id);
                if (collected_loot_indices.count(loot_idx)) continue;

                if (!player->IsBagFull(bag_capacity)) {
                    player->AddToBag({loot[loot_idx].id, loot[loot_idx].type});
                    collected_loot_indices.insert(loot_idx);
                }
            } else {
                size_t office_idx = evt.item_id - loot.size();
                if (office_idx < office_count) {
                    for (const auto& bag_item : player->GetBag()) {
                        player->AddScore(map->GetLootTypeValue(bag_item.type));
                    }
                    player->ClearBag();
                }
            }
        }

        // Remove collected loot
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
}

void Game::GenerateLoot(int time_delta_ms) {
    if (!loot_generator_) return;

    for (auto& [map_id, player_ids] : map_players_) {
        if (player_ids.empty()) continue;

        const auto* map = FindMap(map_id);
        if (!map || map->GetLootTypeCount() == 0) continue;

        auto& loot = map_loot_[map_id];
        unsigned generated = loot_generator_->Generate(
            std::chrono::milliseconds(time_delta_ms),
            static_cast<unsigned>(loot.size()),
            static_cast<unsigned>(player_ids.size()));

        int& next_id = next_loot_id_[map_id];
        const auto& roads = map->GetRoads();
        if (roads.empty()) continue;

        for (unsigned i = 0; i < generated; ++i) {
            std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
            const Road& road = roads[road_dist(rng_)];
            std::uniform_int_distribution<int> type_dist(0, static_cast<int>(map->GetLootTypeCount()) - 1);

            loot.push_back({next_id++, type_dist(rng_), road.RandomPointOnRoad(rng_)});
        }
    }
}

std::vector<RetiredPlayer> Game::RetireInactivePlayers() {
    std::vector<RetiredPlayer> retired_players;
    std::vector<int> retired_ids;

    for (auto& [player_id, player] : all_players_) {
        if (player.GetInactiveTimeMs() >= dog_retirement_time_ms_) {
            retired_players.push_back({
                player.GetName(),
                player.GetScore(),
                player.GetPlayTimeMs() / 1000.0
            });
            retired_ids.push_back(player_id);
        }
    }

    for (int pid : retired_ids) {
        // Remove token mapping
        auto token_it = player_to_token_.find(pid);
        if (token_it != player_to_token_.end()) {
            const auto& token = token_it->second;
            auto map_it = token_to_player_.find(token);
            if (map_it != token_to_player_.end()) {
                Map::Id map_id = map_it->second.first;
                token_to_player_.erase(map_it);

                // Remove from map_players_
                if (auto mp_it = map_players_.find(map_id); mp_it != map_players_.end()) {
                    auto& ids = mp_it->second;
                    ids.erase(std::remove(ids.begin(), ids.end(), pid), ids.end());
                }
            }
            player_to_token_.erase(token_it);
        }

        players_.erase(pid);
        all_players_.erase(pid);
    }

    return retired_players;
}

const std::vector<LostObject>& Game::GetLostObjects(const Map::Id& map_id) const {
    static const std::vector<LostObject> empty;
    auto it = map_loot_.find(map_id);
    return it != map_loot_.end() ? it->second : empty;
}

}  // namespace model
