#include "model.h"
#include "player.h"

#include <stdexcept>
#include <random>
#include <cmath>

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

    // --- ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ ДЛЯ ГЕНЕРАЦИИ ПОЗИЦИИ ---
    Position GenerateRandomPointOnRoad(const Map& map) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        const auto& roads = map.GetRoads();
        if (roads.empty()) {
            return { 0.0, 0.0 };
        }

        std::uniform_int_distribution<> road_dist(0, static_cast<int>(roads.size()) - 1);
        const auto& road = roads[road_dist(gen)];

        std::uniform_real_distribution<double> pos_dist(0.0, 1.0);
        double t = pos_dist(gen);

        double x, y;
        if (road.IsHorizontal()) {
            double x0 = road.GetStart().x;
            double x1 = road.GetEnd().x;
            x = x0 + t * (x1 - x0);
            y = road.GetStart().y;
        }
        else {
            double y0 = road.GetStart().y;
            double y1 = road.GetEnd().y;
            x = road.GetStart().x;
            y = y0 + t * (y1 - y0);
        }
        return { x, y };
    }

    Player& PlayerManager::AddPlayer(const std::string& name, Map* map) {
        Token token = GenerateToken();
        PlayerId id = PlayerId{ players_.size() };
        players_.emplace_back(id, name, std::move(token), map);
        Player& player = players_.back();

        // Инициализация случайной позиции
        Position pos = GenerateRandomPointOnRoad(*map);
        player.SetPosition(pos);

        token_to_player_.emplace(*player.GetToken(), &player);
        return player;
    }

}  // namespace model