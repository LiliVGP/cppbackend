#include "player.h"

#include <iomanip>
#include <sstream>

namespace model {

    Player& Players::Add(std::string name, Map::Id map_id, model::Game& game) {
        // 1. Проверяем, существует ли карта
        const auto* map = game.FindMap(map_id);
        if (!map) {
            throw std::runtime_error("Map not found");
        }

        // 2. Создаём собаку на карте.
        // В этом задании мы только создаём игрока, собака нужна для привязки.
        // В реальной игре здесь будет более сложная логика добавления собаки на карту.
        auto dog = std::make_shared<Dog>();

        // 3. Генерируем токен
        auto token = GenerateToken();

        // 4. Создаём игрока
        auto player = std::make_shared<Player>(next_player_id_++, std::move(name), dog, token);

        // 5. Сохраняем игрока в обоих хранилищах
        players_[player->GetId()] = player;
        token_to_player_[token] = player.get();

        return *player;
    }

    Player::Token Players::GenerateToken() {
        // Получаем два 64-битных числа
        uint64_t part1 = generator1_();
        uint64_t part2 = generator2_();

        // Форматируем их в hex-строки (по 16 символов) и склеиваем
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << part1
            << std::setw(16) << part2;

        return ss.str();
    }

} // namespace model