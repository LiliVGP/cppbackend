#include "application.h"
#include "model_serialization.h"
#include <stdexcept>
#include <algorithm>
#include <random>

namespace app {

    Application::Application(std::vector<model::Map> maps) {
        for (auto& map : maps) {
            MapState state;
            state.map = std::move(map);
            maps_.emplace(state.map.GetId(), std::move(state));
        }
    }

    void Application::Tick(Milliseconds delta) {
        // Обновляем позиции всех собак
        for (auto& [map_id, map_state] : maps_) {
            for (auto& dog : map_state.dogs) {
                const auto& speed = dog.GetSpeed();
                if (speed.x != 0.0 || speed.y != 0.0) {
                    double time_sec = delta.count() / 1000.0;
                    geom::Point2D new_pos = dog.GetPosition();
                    new_pos.x += speed.x * time_sec;
                    new_pos.y += speed.y * time_sec;
                    dog.SetPosition(new_pos);
                }
            }
        }

        // Уведомляем подписчиков
        tick_signal_(delta);
    }

    model::Dog::Id Application::AddDog(const model::Map::Id& map_id, const std::string& name, geom::Point2D pos) {
        static uint32_t next_dog_id = 1;
        model::Dog::Id dog_id{ next_dog_id++ };
        auto& map_state = GetMapState(map_id);
        size_t bag_capacity = 3; // например
        map_state.dogs.emplace_back(dog_id, name, pos, bag_capacity);
        return dog_id;
    }

    model::Dog* Application::GetDog(const model::Dog::Id& dog_id) {
        for (auto& [map_id, map_state] : maps_) {
            for (auto& dog : map_state.dogs) {
                if (dog.GetId() == dog_id) {
                    return &dog;
                }
            }
        }
        return nullptr;
    }

    const model::Dog* Application::GetDog(const model::Dog::Id& dog_id) const {
        for (const auto& [map_id, map_state] : maps_) {
            for (const auto& dog : map_state.dogs) {
                if (dog.GetId() == dog_id) {
                    return &dog;
                }
            }
        }
        return nullptr;
    }

    std::vector<model::Dog*> Application::GetDogsOnMap(const model::Map::Id& map_id) {
        auto& map_state = GetMapState(map_id);
        std::vector<model::Dog*> result;
        result.reserve(map_state.dogs.size());
        for (auto& dog : map_state.dogs) {
            result.push_back(&dog);
        }
        return result;
    }

    std::vector<const model::Dog*> Application::GetDogsOnMap(const model::Map::Id& map_id) const {
        const auto& map_state = GetMapState(map_id);
        std::vector<const model::Dog*> result;
        result.reserve(map_state.dogs.size());
        for (const auto& dog : map_state.dogs) {
            result.push_back(&dog);
        }
        return result;
    }

    Token Application::CreateSession(const model::Dog::Id& dog_id, const model::Map::Id& map_id) {
        // Генерируем случайный токен
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        const char* hex = "0123456789abcdef";
        std::string token(32, ' ');
        for (int i = 0; i < 32; ++i) {
            token[i] = hex[dis(gen)];
        }

        static uint32_t next_player_id = 1;
        PlayerId player_id{ next_player_id++ };

        sessions_.push_back({ token, player_id, dog_id, map_id });
        return token;
    }

    const Session* Application::FindSession(const Token& token) const {
        auto it = std::find_if(sessions_.begin(), sessions_.end(),
            [&token](const Session& s) { return s.token == token; });
        return it != sessions_.end() ? &*it : nullptr;
    }

    Session* Application::FindSession(const Token& token) {
        auto it = std::find_if(sessions_.begin(), sessions_.end(),
            [&token](const Session& s) { return s.token == token; });
        return it != sessions_.end() ? &*it : nullptr;
    }

    void Application::AddLoot(const model::Map::Id& map_id, model::FoundObject loot) {
        auto& map_state = GetMapState(map_id);
        map_state.loot.push_back(std::move(loot));
    }

    const std::vector<model::FoundObject>& Application::GetLootOnMap(const model::Map::Id& map_id) const {
        const auto& map_state = GetMapState(map_id);
        return map_state.loot;
    }

    Application::MapState& Application::GetMapState(const model::Map::Id& map_id) {
        auto it = maps_.find(map_id);
        if (it == maps_.end()) {
            throw std::runtime_error("Map not found");
        }
        return it->second;
    }

    const Application::MapState& Application::GetMapState(const model::Map::Id& map_id) const {
        auto it = maps_.find(map_id);
        if (it == maps_.end()) {
            throw std::runtime_error("Map not found");
        }
        return it->second;
    }

    serialization::GameStateRepr Application::GetGameStateRepr() const {
        serialization::GameStateRepr repr;

        // Сохраняем состояние каждой карты
        for (const auto& [map_id, map_state] : maps_) {
            serialization::MapStateRepr map_repr;
            map_repr.map_id = map_id;

            // Сохраняем собак
            for (const auto& dog : map_state.dogs) {
                map_repr.dogs.emplace_back(dog);
            }

            // Сохраняем предметы
            for (const auto& loot : map_state.loot) {
                serialization::LootRepr loot_repr;
                loot_repr.id = loot.id;
                loot_repr.type = loot.type;
                loot_repr.position = loot.position;
                map_repr.loot_items.push_back(loot_repr);
            }

            repr.maps.push_back(std::move(map_repr));
        }

        // Сохраняем сессии (игроков)
        for (const auto& session : sessions_) {
            serialization::PlayerRepr player_repr;
            player_repr.player_id = session.player_id;
            player_repr.dog_id = session.dog_id;
            player_repr.token = session.token;
            player_repr.map_id = session.map_id;
            repr.players.push_back(player_repr);
        }

        return repr;
    }

    void Application::RestoreFromRepr(const serialization::GameStateRepr& repr) {
        // Очищаем текущее состояние
        maps_.clear();
        sessions_.clear();

        // Восстанавливаем карты
        for (const auto& map_repr : repr.maps) {
            // Создаем карту с ID
            model::Map map(map_repr.map_id, "Restored Map");
            MapState map_state;
            map_state.map = std::move(map);

            // Восстанавливаем собак
            for (const auto& dog_repr : map_repr.dogs) {
                model::Dog dog = dog_repr.Restore();
                map_state.dogs.push_back(std::move(dog));
            }

            // Восстанавливаем предметы
            for (const auto& loot_repr : map_repr.loot_items) {
                model::FoundObject loot;
                loot.id = loot_repr.id;
                loot.type = loot_repr.type;
                loot.position = loot_repr.position;
                map_state.loot.push_back(loot);
            }

            maps_.emplace(map_repr.map_id, std::move(map_state));
        }

        // Восстанавливаем сессии
        for (const auto& player_repr : repr.players) {
            Session session;
            session.token = player_repr.token;
            session.player_id = player_repr.player_id;
            session.dog_id = player_repr.dog_id;
            session.map_id = player_repr.map_id;
            sessions_.push_back(session);
        }
    }

} // namespace app