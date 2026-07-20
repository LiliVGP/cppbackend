#include "model.h"

#include <stdexcept>
#include <random>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>

#include "collision_detector.h"

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
        }
        catch (...) {
            offices_.pop_back();
            throw;
        }
    }

    void Game::AddMap(Map map) {
        const size_t index = maps_.size();
        if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
            throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
        }
        else {
            try {
                maps_.emplace_back(std::move(map));
            }
            catch (...) {
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
        PlayerSpeed speed{ 0.0, 0.0 };

        pos.x = static_cast<double>(first_road.GetStart().x);
        pos.y = static_cast<double>(first_road.GetStart().y);

        Player player(player_id, user_name, pos, speed, PlayerDirection::NORTH);
        all_players_.emplace(player_id, std::move(player));

        players_.emplace(player_id, PlayerInfo{ user_name });
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
        if (it == all_players_.end()) {
            return nullptr;
        }
        return &it->second;
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
        if (it == token_to_player_.end()) {
            return -1;
        }
        return it->second.second;
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
        if (it == token_to_player_.end()) {
            return nullptr;
        }
        return &it->second.first;
    }

    // Process collisions
    void Game::ProcessItemCollisions(const Map::Id& map_id, int time_delta_ms) {
        auto* map = FindMap(map_id);
        if (!map) return;

        auto& loot = map_loot_[map_id];
        if (loot.empty()) return;

        const auto& player_ids = map_players_[map_id];
        if (player_ids.empty()) return;

        // Подготавливаем данные для детектора коллизий
        std::vector<collision_detector::Gatherer> gatherers;
        std::vector<collision_detector::Collectible> items;

        // Создаем map для преобразования индексов
        std::unordered_map<size_t, int> gatherer_to_player_id;

        for (size_t i = 0; i < player_ids.size(); ++i) {
            int player_id = player_ids[i];
            const auto* player = GetPlayer(player_id);
            if (!player) continue;

            PlayerPosition start_pos = player->GetPosition();
            PlayerPosition end_pos = start_pos;
            const auto& speed = player->GetSpeed();
            double time_seconds = time_delta_ms / 1000.0;
            end_pos.x += speed.dx * time_seconds;
            end_pos.y += speed.dy * time_seconds;

            gatherers.push_back({
                {start_pos.x, start_pos.y},
                {end_pos.x, end_pos.y},
                0.6 // ширина игрока
                });
            gatherer_to_player_id[i] = player_id;
        }

        for (size_t i = 0; i < loot.size(); ++i) {
            items.push_back({
                {loot[i].pos.x, loot[i].pos.y},
                0.0 // предметы имеют нулевую ширину
                });
        }

        auto collisions = collision_detector::FindGathererItemCollisions(gatherers, items);

        // Обрабатываем коллизии в хронологическом порядке
        std::vector<bool> item_picked(loot.size(), false);

        for (const auto& collision : collisions) {
            size_t g_idx = collision.gatherer_id;
            size_t i_idx = collision.item_id;

            if (item_picked[i_idx]) continue;

            int player_id = gatherer_to_player_id[g_idx];
            auto* player = GetMutablePlayer(player_id);
            if (!player) continue;

            const auto* map_ptr = FindMap(map_id);
            int bag_capacity = map_ptr->GetBagCapacity(GetDefaultBagCapacity());

            if (!player->IsBagFull(bag_capacity)) {
                // Забираем предмет
                BagItem item;
                item.id = loot[i_idx].id;
                item.type = loot[i_idx].type;
                item.value = loot[i_idx].value;
                player->AddToBag(item);

                // Удаляем предмет с карты
                item_picked[i_idx] = true;
            }
            // Если рюкзак полон, предмет остаётся на месте
        }

        // Удаляем подобранные предметы из списка
        std::vector<LostObject> new_loot;
        for (size_t i = 0; i < loot.size(); ++i) {
            if (!item_picked[i]) {
                new_loot.push_back(loot[i]);
            }
        }
        loot.swap(new_loot);
    }

    void Game::ProcessOfficeCollisions(const Map::Id& map_id, int time_delta_ms) {
        const auto* map = FindMap(map_id);
        if (!map) return;

        const auto& offices = map->GetOffices();
        if (offices.empty()) return;

        const auto& player_ids = map_players_[map_id];
        if (player_ids.empty()) return;

        // Для каждого игрока проверяем, не оказался ли он рядом с офисом
        for (int player_id : player_ids) {
            auto* player = GetMutablePlayer(player_id);
            if (!player) continue;

            const auto& pos = player->GetPosition();
            auto bag = player->GetBag(); // копия
            if (bag.empty()) continue;

            bool delivered = false;

            for (const auto& office : offices) {
                const auto& office_pos = office.GetPosition();
                // Проверяем расстояние до офиса
                double dx = pos.x - office_pos.x;
                double dy = pos.y - office_pos.y;
                double dist = std::sqrt(dx * dx + dy * dy);

                // 0.5 (ширина офиса) / 2 + 0.6 (ширина игрока) / 2 = 0.55
                if (dist <= 0.55) {
                    delivered = true;
                    break;
                }
            }

            if (delivered) {
                // Сдаём предметы и начисляем очки
                int score_gain = 0;
                for (const auto& item : bag) {
                    score_gain += item.value;
                }
                player->AddScore(score_gain);
                player->ClearBag();
            }
        }
    }

    void Game::ProcessCollisions(int time_delta_ms) {
        for (const auto& [map_id, player_ids] : map_players_) {
            if (player_ids.empty()) continue;

            const auto* map = FindMap(map_id);
            if (!map) continue;

            // Сначала обрабатываем сбор предметов
            ProcessItemCollisions(map_id, time_delta_ms);

            // Затем сдачу на базу
            ProcessOfficeCollisions(map_id, time_delta_ms);
        }
    }

    void Game::Tick(int time_delta_ms) {
        double time_seconds = time_delta_ms / 1000.0;

        // 1. Перемещаем игроков
        for (auto& [player_id, player] : all_players_) {
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
            }
            else if (player_speed.dx < 0) {
                player.SetDirection(PlayerDirection::WEST);
            }
            else if (player_speed.dy > 0) {
                player.SetDirection(PlayerDirection::SOUTH);
            }
            else if (player_speed.dy < 0) {
                player.SetDirection(PlayerDirection::NORTH);
            }

            if (stopped) {
                player.SetSpeed(0.0, 0.0);
            }
        }

        // 2. Генерируем лут
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
                    // Pick a random road
                    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
                    const Road& road = roads[road_dist(rng_)];
                    PlayerPosition pos = road.RandomPointOnRoad(rng_);

                    // Pick a random loot type
                    std::uniform_int_distribution<int> type_dist(0, static_cast<int>(map->GetLootTypeCount()) - 1);
                    int type = type_dist(rng_);

                    // Random value for loot (1-10)
                    std::uniform_int_distribution<int> value_dist(1, 10);
                    int value = value_dist(rng_);

                    LostObject obj;
                    obj.id = next_id++;
                    obj.type = type;
                    obj.value = value;
                    obj.pos = pos;
                    loot.push_back(std::move(obj));
                }
            }
        }

        // 3. Обрабатываем коллизии
        ProcessCollisions(time_delta_ms);
    }

    const std::vector<LostObject>& Game::GetLostObjects(const Map::Id& map_id) const {
        static const std::vector<LostObject> empty;
        auto it = map_loot_.find(map_id);
        if (it == map_loot_.end()) {
            return empty;
        }
        return it->second;
    }

}  // namespace model