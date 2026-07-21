#pragma once
#include "tagged.h"
#include <random>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iomanip>

namespace model {

    class Map;

    namespace detail {
        struct TokenTag {};
        struct PlayerTag {};
    }  // namespace detail

    using Token = util::Tagged<std::string, detail::TokenTag>;
    using PlayerId = util::Tagged<size_t, detail::PlayerTag>;

    struct Position {
        double x;
        double y;
    };

    struct Velocity {
        double vx;
        double vy;
    };

    enum class Direction {
        NORTH,
        SOUTH,
        WEST,
        EAST
    };

    class Player {
    public:
        Player(PlayerId id, const std::string& name, Token token, Map* map)
            : id_(id), name_(name), token_(std::move(token)), map_(map),
            position_{ 0.0, 0.0 }, velocity_{ 0.0, 0.0 }, direction_{ Direction::NORTH } {
        }

        const PlayerId& GetId() const noexcept { return id_; }
        const std::string& GetName() const noexcept { return name_; }
        const Token& GetToken() const noexcept { return token_; }
        Map* GetMap() const noexcept { return map_; }

        const Position& GetPosition() const noexcept { return position_; }
        const Velocity& GetVelocity() const noexcept { return velocity_; }
        Direction GetDirection() const noexcept { return direction_; }

        void SetPosition(Position pos) { position_ = pos; }
        void SetVelocity(Velocity vel) { velocity_ = vel; }
        void SetDirection(Direction dir) { direction_ = dir; }

        void UpdatePosition(double delta_time, const Map& map);
        void Stop();

    private:
        PlayerId id_;
        std::string name_;
        Token token_;
        Map* map_;

        Position position_;
        Velocity velocity_;
        Direction direction_;
    };

    // Объявляем GetStartPosition ПЕРЕД PlayerManager
    Position GetStartPosition(const Map& map);

    class PlayerManager {
    public:
        PlayerManager() : generator1_([this] {
            std::uniform_int_distribution<std::mt19937_64::result_type> dist;
            return dist(random_device_);
            }()), generator2_([this] {
                std::uniform_int_distribution<std::mt19937_64::result_type> dist;
                return dist(random_device_);
                }()) {
            }

            Token GenerateToken() {
                uint64_t part1 = generator1_();
                uint64_t part2 = generator2_();
                std::stringstream ss;
                ss << std::hex << std::setfill('0') << std::setw(16) << part1
                    << std::setw(16) << part2;
                return Token{ ss.str() };
            }

            Player& AddPlayer(const std::string& name, Map* map) {
                Token token = GenerateToken();
                PlayerId id = PlayerId{ next_id_++ };
                
                players_.emplace_back(id, name, std::move(token), map);
                Player& player = players_.back();
                
                player.SetPosition(GetStartPosition(*map));
                player.SetVelocity({0.0, 0.0});
                
                token_to_player_.emplace(*player.GetToken(), &player);
                
                return player;
            }

            Player* FindPlayerByToken(const Token& token) {
                auto it = token_to_player_.find(*token);
                if (it == token_to_player_.end()) {
                    return nullptr;
                }
                return it->second;
            }

            std::vector<Player>& GetPlayers() noexcept { return players_; }
            const std::vector<Player>& GetPlayers() const noexcept { return players_; }

    private:
        std::vector<Player> players_;
        std::map<std::string, Player*> token_to_player_;
        size_t next_id_ = 0;

        std::random_device random_device_;
        std::mt19937_64 generator1_;
        std::mt19937_64 generator2_;
    };

}  // namespace model