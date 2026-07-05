#include "model.h"
#include "player.h"

#include <stdexcept>
#include <random>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace model {
    using namespace std::literals;

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

    std::variant<Game::JoinGameResult, std::string> Game::JoinGame(const std::string& user_name, const std::string& map_id) {
        if (user_name.empty()) {
            return "Invalid name";
        }

        Map* map = FindMap(Map::Id(map_id));
        if (!map) {
            return "Map not found";
        }

        auto& player = player_manager_.AddPlayer(user_name, map);
        return JoinGameResult{ player.GetId(), player.GetToken() };
    }

    Position GetStartPosition(const Map& map) {
        const auto& roads = map.GetRoads();
        if (roads.empty()) {
            return { 0.0, 0.0 };
        }
        const auto& road = roads[0];
        return { static_cast<double>(road.GetStart().x) + 0.4, 
                 static_cast<double>(road.GetStart().y) + 0.4 };
    }

    void Player::UpdatePosition(double delta_time, const Map& map) {
        if (velocity_.vx == 0.0 && velocity_.vy == 0.0) {
            return;
        }

        const double eps = 1e-9;
        const double step_size = 0.05;

        double remaining = delta_time;

        while (remaining > eps) {
            double step = std::min(remaining, step_size);

            double new_x = position_.x + velocity_.vx * step;
            double new_y = position_.y + velocity_.vy * step;

            if (map.IsOnRoad(new_x, new_y)) {
                position_.x = new_x;
                position_.y = new_y;
                remaining -= step;
                continue;
            }

            // Пытаемся уменьшить шаг
            bool moved = false;
            double reduced_step = step;
            while (reduced_step > eps) {
                reduced_step /= 2.0;
                double test_x = position_.x + velocity_.vx * reduced_step;
                double test_y = position_.y + velocity_.vy * reduced_step;
                if (map.IsOnRoad(test_x, test_y)) {
                    position_.x = test_x;
                    position_.y = test_y;
                    moved = true;
                    break;
                }
            }

            if (moved) {
                remaining -= reduced_step;  // вычитаем реально использованный шаг
                continue;
            }

            // Пробуем движение только по X
            bool moved_x = false;
            if (std::abs(velocity_.vx) > eps) {
                double test_x = position_.x + velocity_.vx * step;
                if (map.IsOnRoad(test_x, position_.y)) {
                    position_.x = test_x;
                    moved_x = true;
                }
            }

            // Пробуем движение только по Y
            bool moved_y = false;
            if (std::abs(velocity_.vy) > eps) {
                double test_y = position_.y + velocity_.vy * step;
                if (map.IsOnRoad(position_.x, test_y)) {
                    position_.y = test_y;
                    moved_y = true;
                }
            }

            if (moved_x || moved_y) {
                remaining -= step;
                continue;
            }

            // Если ничего не вышло — останавливаемся
            velocity_.vx = 0.0;
            velocity_.vy = 0.0;

            // Округление до 3 знаков (убирает микро-ошибки)
            position_.x = std::round(position_.x * 1000.0) / 1000.0;
            position_.y = std::round(position_.y * 1000.0) / 1000.0;

            return;
        }
    }

    void Player::Stop() {
        velocity_.vx = 0.0;
        velocity_.vy = 0.0;
    }

}  // namespace model