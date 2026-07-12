#include "model.h"

void GameState::GenerateLoot(std::chrono::milliseconds delta) {
    if (!current_map_ || !loot_generator_) {
        return;
    }

    unsigned loot_count = static_cast<unsigned>(loot_.size());
    unsigned looter_count = static_cast<unsigned>(players_.size());

    unsigned generated = loot_generator_->Generate(delta, loot_count, looter_count);

    const auto& roads = current_map_->GetRoads();
    if (roads.empty()) {
        return;
    }

    for (unsigned i = 0; i < generated; ++i) {
        // Выбираем случайный тип трофея
        std::uniform_int_distribution<size_t> type_dist(0, current_map_->GetLootTypesCount() - 1);
        LootTypeId type{ static_cast<LootTypeId::IdType>(type_dist(*rng_)) };

        // Выбираем случайную дорогу
        std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
        const auto& road = roads[road_dist(*rng_)];

        // Генерируем случайную точку на дороге
        Point position = road.random_point(*rng_);

        // Добавляем трофей
        AddLoot(next_loot_id_++, type, position);
    }
}