#include "model.h"
#include <cmath>
#include <iostream>
#include <vector>

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

    // Создаём список всех допустимых точек на всех дорогах
    std::vector<Point> valid_points;
    const int points_per_road = 100;
    
    for (const auto& road : roads) {
        // Генерируем points_per_road точек на каждой дороге
        for (int i = 0; i < points_per_road; ++i) {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            double t = dist(*rng_);
            
            Point p;
            const double eps = 1e-9;
            
            if (std::abs(road.x0 - road.x1) < eps) {
                // Вертикальная дорога: x фиксирован
                p.x = road.x0;
                p.y = road.y0 + (road.y1 - road.y0) * t;
            } else if (std::abs(road.y0 - road.y1) < eps) {
                // Горизонтальная дорога: y фиксирован
                p.x = road.x0 + (road.x1 - road.x0) * t;
                p.y = road.y0;
            } else {
                // Диагональная дорога (не должна возникать)
                p.x = road.x0 + (road.x1 - road.x0) * t;
                p.y = road.y0 + (road.y1 - road.y0) * t;
            }
            
            valid_points.push_back(p);
        }
    }

    if (valid_points.empty()) {
        return;
    }

    // Выбираем случайные точки из списка допустимых
    std::uniform_int_distribution<size_t> point_dist(0, valid_points.size() - 1);
    
    for (unsigned i = 0; i < generated; ++i) {
        // Выбираем случайный тип трофея
        std::uniform_int_distribution<size_t> type_dist(0, current_map_->GetLootTypesCount() - 1);
        LootTypeId type{ static_cast<LootTypeId::IdType>(type_dist(*rng_)) };

        // Выбираем случайную допустимую точку
        Point position = valid_points[point_dist(*rng_)];

        // Добавляем трофей
        AddLoot(next_loot_id_++, type, position);
    }
}