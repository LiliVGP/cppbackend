#include "model.h"
#include <cmath>
#include <iostream>  // для std::cerr

// Вспомогательная функция для проверки, лежит ли точка на отрезке
static bool IsPointOnSegment(double x, double y, double x0, double y0, double x1, double y1) {
    const double eps = 1e-6;
    
    // Проверяем, что точка лежит на линии
    double dx = x1 - x0;
    double dy = y1 - y0;
    double len2 = dx * dx + dy * dy;
    
    if (len2 < eps * eps) {
        // Отрезок вырожден в точку
        return std::abs(x - x0) < eps && std::abs(y - y0) < eps;
    }
    
    // Проверяем, что точка лежит на прямой
    double cross = std::abs((x - x0) * dy - (y - y0) * dx);
    if (cross / std::sqrt(len2) > eps) {
        return false;
    }
    
    // Проверяем, что точка внутри отрезка
    double dot = (x - x0) * dx + (y - y0) * dy;
    if (dot < 0 || dot > len2) {
        return false;
    }
    
    return true;
}

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

        Point position;
        bool valid_position = false;
        int attempts = 0;
        const int max_attempts = 100;

        // Пытаемся сгенерировать точку на дороге
        while (!valid_position && attempts < max_attempts) {
            attempts++;
            
            // Выбираем случайную дорогу
            std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
            const auto& road = roads[road_dist(*rng_)];
            
            // Генерируем точку на дороге с учётом её ориентации
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            double t = dist(*rng_);
            
            // Определяем тип дороги и генерируем точку
            if (road.x0 == road.x1) {
                // Вертикальная дорога: x фиксирован
                position.x = road.x0;
                position.y = road.y0 + (road.y1 - road.y0) * t;
            } else if (road.y0 == road.y1) {
                // Горизонтальная дорога: y фиксирован
                position.x = road.x0 + (road.x1 - road.x0) * t;
                position.y = road.y0;
            } else {
                // Диагональная дорога
                position.x = road.x0 + (road.x1 - road.x0) * t;
                position.y = road.y0 + (road.y1 - road.y0) * t;
            }

            // Проверяем, что точка действительно лежит на дороге
            valid_position = IsPointOnSegment(position.x, position.y, 
                                             road.x0, road.y0, 
                                             road.x1, road.y1);
        }

        if (!valid_position) {
            // Если не удалось сгенерировать точку, пропускаем
            std::cerr << "Warning: Failed to generate valid loot position after " 
                      << max_attempts << " attempts" << std::endl;
            continue;
        }

        // Добавляем трофей
        AddLoot(next_loot_id_++, type, position);
    }
}
