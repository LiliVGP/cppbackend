#include "model.h"

#include <stdexcept>
#include <random>
#include <algorithm>
#include <cctype>
#include <cmath>
#include "collision_detector.h" // Подключаем детектор коллизий

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

    void Game::Tick(int time_delta_ms) {
        double time_seconds = time_delta_ms / 1000.0;

        // 1. Сохраняем старые позиции игроков перед перемещением
        struct PlayerSnapshot {
            int id;
            PlayerPosition old_pos;
            PlayerPosition new_pos;
            std::string token;
            Map::Id map_id;
        };
        std::vector<PlayerSnapshot> snapshots;

        for (auto& [player_id, player] : all_players_) {
            std::string token;
            for (const auto& [t, info] : token_to_player_) {
                if (info.second == player_id) {
                    token = t;
                    break;
                }
            }
            if (token.empty()) continue;

            const auto* map_id = GetMapIdByToken(token);
            if (!map_id) continue;

            snapshots.push_back({ player_id, player.GetPosition(), player.GetPosition(), token, *map_id });
        }

        // 2. Двигаем игроков (существующий код)
        for (auto& [player_id, player] : all_players_) {
            const auto& speed = player.GetSpeed();
            if (speed.dx == 0.0 && speed.dy == 0.0) continue;

            std::string token;
            for (const auto& [t, info] : token_to_player_) {
                if (info.second == player_id) {
                    token = t;
                    break;
                }
            }
            if (token.empty()) continue;

            const auto* map_id = GetMapIdByToken(token);
            if (!map_id) continue;

            const auto* map = FindMap(*map_id);
            if (!map) continue;

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

            if (start_roads.empty()) continue;

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
            if (player_speed.dx > 0) player.SetDirection(PlayerDirection::EAST);
            else if (player_speed.dx < 0) player.SetDirection(PlayerDirection::WEST);
            else if (player_speed.dy > 0) player.SetDirection(PlayerDirection::SOUTH);
            else if (player_speed.dy < 0) player.SetDirection(PlayerDirection::NORTH);

            if (stopped) {
                player.SetSpeed(0.0, 0.0);
            }

            // Обновляем новую позицию в снимке
            for (auto& snap : snapshots) {
                if (snap.id == player_id) {
                    snap.new_pos = player.GetPosition();
                    break;
                }
            }
        }

        // 3. Обработка коллизий с предметами
        // 3.1. Собираем всех игроков как Gatherer'ы
        struct PlayerGathererInfo {
            int player_id;
            collision_detector::Gatherer gatherer;
        };
        std::vector<PlayerGathererInfo> gatherers;

        for (const auto& snap : snapshots) {
            collision_detector::Gatherer g;
            g.start_pos = { snap.old_pos.x, snap.old_pos.y };
            g.end_pos = { snap.new_pos.x, snap.new_pos.y };
            g.width = 0.3; // Половина ширины игрока
            gatherers.push_back({ snap.id, g });
        }

        // 3.2. Собираем все предметы как Item'ы
        struct ItemInfo {
            int id;
            collision_detector::Item item;
            int type;
            int value;
        };
        std::vector<ItemInfo> items;

        for (const auto& [map_id, loot_vec] : map_loot_) {
            for (const auto& obj : loot_vec) {
                collision_detector::Item item;
                item.position = { obj.pos.x, obj.pos.y };
                item.width = 0.0; // Предметы имеют ширину 0
                items.push_back({ obj.id, item, obj.type, obj.value });
            }
        }

        // 3.3. Провайдер для детектора коллизий
        class CollisionProvider : public collision_detector::ItemGathererProvider {
        public:
            CollisionProvider(const std::vector<PlayerGathererInfo>& g, const std::vector<ItemInfo>& i)
                : gatherers_(g), items_(i) {
            }

            size_t ItemsCount() const override { return items_.size(); }
            collision_detector::Item GetItem(size_t idx) const override { return items_[idx].item; }
            size_t GatherersCount() const override { return gatherers_.size(); }
            collision_detector::Gatherer GetGatherer(size_t idx) const override { return gatherers_[idx].gatherer; }

            int GetItemId(size_t idx) const { return items_[idx].id; }
            int GetItemType(size_t idx) const { return items_[idx].type; }
            int GetItemValue(size_t idx) const { return items_[idx].value; }
            int GetGathererId(size_t idx) const { return gatherers_[idx].player_id; }

        private:
            std::vector<PlayerGathererInfo> gatherers_;
            std::vector<ItemInfo> items_;
        };

        CollisionProvider provider(gatherers, items);
        auto events = collision_detector::FindGatherEvents(provider);

        // 3.4. Обрабатываем события в хронологическом порядке
        for (const auto& event : events) {
            int player_id = provider.GetGathererId(event.gatherer_id);
            int item_idx = event.item_id;
            int item_id = provider.GetItemId(item_idx);

            Player* player = const_cast<Player*>(GetPlayer(player_id));
            if (!player) continue;

            // Находим карту игрока
            std::string token;
            for (const auto& [t, info] : token_to_player_) {
                if (info.second == player_id) {
                    token = t;
                    break;
                }
            }
            if (token.empty()) continue;
            const auto* map_id = GetMapIdByToken(token);
            if (!map_id) continue;

            // Проверяем вместимость рюкзака
            const auto* map = FindMap(*map_id);
            int bag_capacity = map ? map->GetBagCapacity() : GetDefaultBagCapacity();

            if (player->GetBagSize() < static_cast<size_t>(bag_capacity)) {
                // Берём предмет
                auto& loot_vec = map_loot_[*map_id];
                auto it = std::find_if(loot_vec.begin(), loot_vec.end(),
                    [item_id](const LostObject& obj) { return obj.id == item_id; });

                if (it != loot_vec.end()) {
                    player->AddToBag(*it);
                    loot_vec.erase(it);
                }
            }
            // Если рюкзак полон, предмет игнорируется
        }

        // 4. Обработка возврата предметов на базу
        for (const auto& snap : snapshots) {
            // ИСПРАВЛЕНИЕ: GetPlayer возвращает неконстантный указатель, так как all_players_ - неконстантное хранилище
            Player* player = GetPlayer(snap.id);
            if (!player) continue;

            const auto* map = FindMap(snap.map_id);
            if (!map) continue;

            // Проверяем расстояние до всех офисов на карте
            for (const auto& office : map->GetOffices()) {
                double dx = player->GetPosition().x - static_cast<double>(office.GetPosition().x);
                double dy = player->GetPosition().y - static_cast<double>(office.GetPosition().y);
                double dist_sq = dx * dx + dy * dy;

                // Радиус сбора: 0.25 (половина базы) + 0.3 (половина игрока) = 0.55
                if (dist_sq <= 0.55 * 0.55) {
                    // Сдаём предметы и начисляем очки
                    for (const auto& obj : player->GetBag()) {
                        player->AddScore(obj.value);
                    }
                    player->ClearBag();
                    break; // Достаточно одного офиса
                }
            }
        }

        // 5. Генерация новых предметов (существующий код)
        if (loot_generator_) {
            for (auto& [map_id, player_ids] : map_players_) {
                if (player_ids.empty()) continue;

                const auto* map = FindMap(map_id);
                if (!map || map->GetLootTypeCount() == 0) continue;

                auto& loot = map_loot_[map_id];
                unsigned loot_count = static_cast<unsigned>(loot.size());
                unsigned looter_count = static_cast<unsigned>(player_ids.size());

                unsigned generated = loot_generator_->Generate(
                    std::chrono::milliseconds(time_delta_ms), loot_count, looter_count);

                int& next_id = next_loot_id_[map_id];
                const auto& roads = map->GetRoads();
                if (roads.empty()) continue;

                for (unsigned i = 0; i < generated; ++i) {
                    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
                    const Road& road = roads[road_dist(rng_)];
                    PlayerPosition pos = road.RandomPointOnRoad(rng_);

                    std::uniform_int_distribution<int> type_dist(0, static_cast<int>(map->GetLootTypeCount()) - 1);

                    LostObject obj;
                    obj.id = next_id++;
                    obj.type = type_dist(rng_);
                    obj.pos = pos;
                    obj.value = 0; // Стоимость будет загружена из JSON
                    loot.push_back(std::move(obj));
                }
            }
        }
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