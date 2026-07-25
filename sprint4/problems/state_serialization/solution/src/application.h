#pragma once

#include <boost/signals2.hpp>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

#include "model.h"
#include "geom.h"
#include "types.h"

namespace app {

    using Milliseconds = std::chrono::milliseconds;

    // Сессия игрока: связывает токен, игрока и собаку на карте
    struct Session {
        Token token;
        PlayerId player_id;
        model::Dog::Id dog_id;
        model::Map::Id map_id;
    };

    // Предварительное объявление для сериализации
    namespace serialization {
        class GameStateRepr;
    }

    // Класс приложения, управляющий игровым состоянием
    class Application {
    public:
        using TickSignal = boost::signals2::signal<void(Milliseconds delta)>;

        // Конструктор получает все карты (статическая информация)
        explicit Application(std::vector<model::Map> maps);

        // Подписка на тики
        [[nodiscard]] boost::signals2::connection DoOnTick(const TickSignal::slot_type& handler) {
            return tick_signal_.connect(handler);
        }

        // Вызывается при каждом тике (из REST API или автоматического таймера)
        void Tick(Milliseconds delta);

        // --- Методы для управления собаками и игроками ---

        // Добавить собаку на карту (возвращает ID собаки)
        model::Dog::Id AddDog(const model::Map::Id& map_id, const std::string& name, geom::Point2D pos);

        // Получить собаку по ID
        model::Dog* GetDog(const model::Dog::Id& dog_id);
        const model::Dog* GetDog(const model::Dog::Id& dog_id) const;

        // Получить все собаки на карте
        std::vector<model::Dog*> GetDogsOnMap(const model::Map::Id& map_id);
        std::vector<const model::Dog*> GetDogsOnMap(const model::Map::Id& map_id) const;

        // --- Методы для управления сессиями (игроками) ---

        // Создать сессию для игрока (возвращает токен)
        Token CreateSession(const model::Dog::Id& dog_id, const model::Map::Id& map_id);

        // Найти сессию по токену
        const Session* FindSession(const Token& token) const;
        Session* FindSession(const Token& token);

        // Получить все сессии
        const std::vector<Session>& GetSessions() const { return sessions_; }

        // --- Методы для управления потерянными предметами ---

        // Добавить потерянный предмет на карту
        void AddLoot(const model::Map::Id& map_id, model::FoundObject loot);

        // Получить все предметы на карте
        const std::vector<model::FoundObject>& GetLootOnMap(const model::Map::Id& map_id) const;

        // --- Сериализация/восстановление состояния ---

        // Получить полное состояние игры для сериализации
        serialization::GameStateRepr GetGameStateRepr() const;

        // Восстановить состояние из репрезентации
        void RestoreFromRepr(const serialization::GameStateRepr& repr);

    private:
        struct MapState {
            model::Map map;                        // статическая информация о карте
            std::vector<model::Dog> dogs;          // все собаки на карте
            std::vector<model::FoundObject> loot;  // все потерянные предметы
        };

        std::unordered_map<model::Map::Id, MapState> maps_;
        std::vector<Session> sessions_;
        TickSignal tick_signal_;

        // Вспомогательные методы
        MapState& GetMapState(const model::Map::Id& map_id);
        const MapState& GetMapState(const model::Map::Id& map_id) const;
    };

} // namespace app