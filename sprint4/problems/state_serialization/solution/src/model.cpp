#include "model.h"
#include "model_serialization.h"
#include <stdexcept>
#include <algorithm>

namespace model {

    Game::Game(std::vector<Map> maps)
        : maps_(std::move(maps)) {
        // Инициализируем состояния для каждой карты
        for (const auto& map : maps_) {
            MapState state;
            state.map = map;
            map_states_.emplace(map.GetId(), std::move(state));
        }
    }

    const Map* Game::FindMap(const Map::Id& id) const {
        auto it = std::find_if(maps_.begin(), maps_.end(),
            [&id](const Map& map) { return map.GetId() == id; });
        return it != maps_.end() ? &*it : nullptr;
    }

    Map* Game::FindMap(const Map::Id& id) {
        auto it = std::find_if(maps_.begin(), maps_.end(),
            [&id](const Map& map) { return map.GetId() == id; });
        return it != maps_.end() ? &*it : nullptr;
    }

    Dog::Id Game::AddDog(const Map::Id& map_id, const std::string& name, geom::Point2D pos) {
        static uint32_t next_dog_id = 1;
        Dog::Id dog_id{ next_dog_id++ };
        auto& map_state = GetMapState(map_id);
        size_t bag_capacity = 3; // можно брать из конфига карты
        map_state.dogs.emplace_back(dog_id, name, pos, bag_capacity);
        return dog_id;
    }

    Dog* Game::GetDog(const Dog::Id& dog_id) {
        for (auto& [map_id, map_state] : map_states_) {
            for (auto& dog : map_state.dogs) {
                if (dog.GetId() == dog_id) {
                    return &dog;
                }
            }
        }
        return nullptr;
    }

    const Dog* Game::GetDog(const Dog::Id& dog_id) const {
        for (const auto& [map_id, map_state] : map_states_) {
            for (const auto& dog : map_state.dogs) {
                if (dog.GetId() == dog_id) {
                    return &dog;
                }
            }
        }
        return nullptr;
    }

    std::vector<Dog*> Game::GetDogsOnMap(const Map::Id& map_id) {
        auto& map_state = GetMapState(map_id);
        std::vector<Dog*> result;
        result.reserve(map_state.dogs.size());
        for (auto& dog : map_state.dogs) {
            result.push_back(&dog);
        }
        return result;
    }

    std::vector<const Dog*> Game::GetDogsOnMap(const Map::Id& map_id) const {
        const auto& map_state = GetMapState(map_id);
        std::vector<const Dog*> result;
        result.reserve(map_state.dogs.size());
        for (const auto& dog : map_state.dogs) {
            result.push_back(&dog);
        }
        return result;
    }

    void Game::AddLoot(const Map::Id& map_id, FoundObject loot) {
        auto& map_state = GetMapState(map_id);
        map_state.loot.push_back(std::move(loot));
    }

    const std::vector<FoundObject>& Game::GetLootOnMap(const Map::Id& map_id) const {
        const auto& map_state = GetMapState(map_id);
        return map_state.loot;
    }

    Game::MapState& Game::GetMapState(const Map::Id& map_id) {
        auto it = map_states_.find(map_id);
        if (it == map_states_.end()) {
            throw std::runtime_error("Map not found");
        }
        return it->second;
    }

    const Game::MapState& Game::GetMapState(const Map::Id& map_id) const {
        auto it = map_states_.find(map_id);
        if (it == map_states_.end()) {
            throw std::runtime_error("Map not found");
        }
        return it->second;
    }

    serialization::GameStateRepr Game::GetGameStateRepr() const {
        serialization::GameStateRepr repr;

        // Сохраняем состояние каждой карты
        for (const auto& [map_id, map_state] : map_states_) {
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

        return repr;
    }

    void Game::RestoreFromRepr(const serialization::GameStateRepr& repr) {
        // Очищаем текущее состояние
        map_states_.clear();

        // Восстанавливаем карты
        for (const auto& map_repr : repr.maps) {
            // Проверяем, что карта существует в статическом списке
            auto map_ptr = FindMap(map_repr.map_id);
            if (!map_ptr) {
                // Если карта не найдена, создаём новую (только для восстановления)
                Map map(map_repr.map_id, "Restored Map");
                MapState map_state;
                map_state.map = std::move(map);

                // Восстанавливаем собак
                for (const auto& dog_repr : map_repr.dogs) {
                    Dog dog = dog_repr.Restore();
                    map_state.dogs.push_back(std::move(dog));
                }

                // Восстанавливаем предметы
                for (const auto& loot_repr : map_repr.loot_items) {
                    FoundObject loot;
                    loot.id = loot_repr.id;
                    loot.type = loot_repr.type;
                    loot.position = loot_repr.position;
                    map_state.loot.push_back(loot);
                }

                map_states_.emplace(map_repr.map_id, std::move(map_state));
            }
            else {
                // Карта существует, восстанавливаем её состояние
                MapState map_state;
                map_state.map = *map_ptr;

                // Восстанавливаем собак
                for (const auto& dog_repr : map_repr.dogs) {
                    Dog dog = dog_repr.Restore();
                    map_state.dogs.push_back(std::move(dog));
                }

                // Восстанавливаем предметы
                for (const auto& loot_repr : map_repr.loot_items) {
                    FoundObject loot;
                    loot.id = loot_repr.id;
                    loot.type = loot_repr.type;
                    loot.position = loot_repr.position;
                    map_state.loot.push_back(loot);
                }

                map_states_.emplace(map_repr.map_id, std::move(map_state));
            }
        }
    }

} // namespace model