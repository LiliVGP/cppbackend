#pragma once

#include "model.h"  // Добавили полное определение классов Map, Game, Dog
#include <memory>
#include <random>
#include <string>
#include <unordered_map>

namespace model {

// Класс Игрок
class Player {
public:
    // Токен будем хранить как строку для удобства работы с JSON
    using Token = std::string;

    Player(int id, std::string name, std::shared_ptr<Dog> dog, Token token)
        : id_(id)
        , name_(std::move(name))
        , dog_(std::move(dog))
        , token_(std::move(token)) {
    }

    int GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    std::shared_ptr<Dog> GetDog() const noexcept { return dog_; }
    const Token& GetToken() const noexcept { return token_; }

private:
    int id_;
    std::string name_;
    std::shared_ptr<Dog> dog_;
    Token token_;
};

// Класс для управления игроками и токенами
class Players {
public:
    // Добавляет нового игрока, генерирует для него токен и создаёт собаку на карте
    Player& Add(std::string name, Map::Id map_id, Game& game);

    // Находит игрока по токену. Возвращает nullptr, если не найден.
    Player* FindByToken(const Player::Token& token) {
        auto it = token_to_player_.find(token);
        if (it != token_to_player_.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Возвращает всех игроков
    const std::unordered_map<int, std::shared_ptr<Player>>& GetPlayers() const {
        return players_;
    }

private:
    // Генерирует случайный 128-битный токен в hex-формате
    Player::Token GenerateToken();

    // Хранилище игроков по id
    std::unordered_map<int, std::shared_ptr<Player>> players_;
    // Индекс для быстрого поиска по токену
    std::unordered_map<Player::Token, Player*> token_to_player_;

    // Генераторы случайных чисел
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

    int next_player_id_ = 0;
};

} // namespace model
