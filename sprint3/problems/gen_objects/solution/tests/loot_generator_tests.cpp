#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <chrono>
#include <memory>

#include "../src/model.h"
#include "../src/loot_generator.h"

using namespace std::literals;

class TestLootGenerator : public loot_gen::LootGenerator {
public:
    TestLootGenerator(unsigned loot_per_tick = 0)
        : loot_gen::LootGenerator{ std::chrono::milliseconds{1000}, 1.0, [] { return 1.0; } }
        , loot_per_tick_{ loot_per_tick } {}

    unsigned Generate(std::chrono::milliseconds time_delta, unsigned loot_count, unsigned looter_count) override {
        return loot_per_tick_;
    }

private:
    unsigned loot_per_tick_;
};

TEST_CASE("GameState generates loot", "[GameState]") {
    GIVEN("A map with roads and loot types") {
        Map map{ "map1", "Map 1", 3 }; // 3 типа трофеев
        map.AddRoad(Road{ 0, 0, 10, 0 });
        map.AddRoad(Road{ 10, 0, 10, 10 });
        map.AddRoad(Road{ 10, 10, 0, 10 });
        map.AddRoad(Road{ 0, 10, 0, 0 });

        auto rng = std::make_unique<std::mt19937>(42);
        auto loot_gen = std::make_shared<TestLootGenerator>(5);
        GameState state{ loot_gen, std::move(rng) };
        state.SetCurrentMap(&map);

        WHEN("Loot is generated") {
            state.GenerateLoot(std::chrono::milliseconds{ 1000 });

            THEN("Loot appears on the map") {
                CHECK(state.GetLoot().size() == 5);
            }

            THEN("Each loot has a valid type") {
                for (const auto& [id, loot] : state.GetLoot()) {
                    CHECK(loot.type.GetId() < 3);
                }
            }

            THEN("Loot appears on roads") {
                for (const auto& [id, loot] : state.GetLoot()) {
                    // Проверяем, что точка лежит на одной из дорог
                    bool on_road = false;
                    for (const auto& road : map.GetRoads()) {
                        if (loot.position.x >= std::min(road.x0, road.x1) - 0.01 &&
                            loot.position.x <= std::max(road.x0, road.x1) + 0.01 &&
                            loot.position.y >= std::min(road.y0, road.y1) - 0.01 &&
                            loot.position.y <= std::max(road.y0, road.y1) + 0.01) {
                            on_road = true;
                            break;
                        }
                    }
                    CHECK(on_road);
                }
            }
        }
    }
}

TEST_CASE("GameState respects loot limits", "[GameState]") {
    GIVEN("A map with roads and loot types") {
        Map map{ "map1", "Map 1", 2 };
        map.AddRoad(Road{ 0, 0, 10, 0 });

        auto rng = std::make_unique<std::mt19937>(42);
        auto loot_gen = std::make_shared<TestLootGenerator>(3);
        GameState state{ loot_gen, std::move(rng) };
        state.SetCurrentMap(&map);

        // Добавляем 2 игроков
        state.AddPlayer(PlayerId{ 1 }, GameState::Player{
            .position = {0, 0},
            .speed = {0, 0},
            .direction = GameState::Direction::U
            });
        state.AddPlayer(PlayerId{ 2 }, GameState::Player{
            .position = {5, 0},
            .speed = {0, 0},
            .direction = GameState::Direction::U
            });

        WHEN("Loot is generated multiple times") {
            state.GenerateLoot(std::chrono::milliseconds{ 1000 });
            state.GenerateLoot(std::chrono::milliseconds{ 1000 });
            state.GenerateLoot(std::chrono::milliseconds{ 1000 });

            THEN("Total loot should not exceed number of players") {
                CHECK(state.GetLoot().size() <= 2);
            }
        }
    }
}

TEST_CASE("Road random point generation", "[Road]") {
    GIVEN("A horizontal road") {
        Road road{ 0, 0, 10, 0 };
        std::mt19937 rng(42);

        WHEN("A random point is generated") {
            Point p = road.random_point(rng);

            THEN("The point lies on the road") {
                CHECK(p.x >= 0);
                CHECK(p.x <= 10);
                CHECK(p.y == Catch::Approx(0));
            }
        }
    }

    GIVEN("A vertical road") {
        Road road{ 5, 0, 5, 10 };
        std::mt19937 rng(42);

        WHEN("A random point is generated") {
            Point p = road.random_point(rng);

            THEN("The point lies on the road") {
                CHECK(p.y >= 0);
                CHECK(p.y <= 10);
                CHECK(p.x == Catch::Approx(5));
            }
        }
    }

    GIVEN("A diagonal road") {
        Road road{ 0, 0, 10, 10 };
        std::mt19937 rng(42);

        WHEN("A random point is generated") {
            Point p = road.random_point(rng);

            THEN("The point lies on the road") {
                CHECK(p.x >= 0);
                CHECK(p.x <= 10);
                CHECK(p.y >= 0);
                CHECK(p.y <= 10);
                CHECK(p.x == Catch::Approx(p.y));
            }
        }
    }
}

TEST_CASE("LootGenerator with deterministic random", "[LootGenerator]") {
    using namespace std::chrono;

    GIVEN("A loot generator with fixed probability") {
        loot_gen::LootGenerator gen{ milliseconds{1000}, 0.5, [] { return 0.5; } };

        WHEN("Loot shortage is 4 and probability is 0.5") {
            // За один период должно сгенерироваться 2 трофея
            unsigned result = gen.Generate(milliseconds{ 1000 }, 0, 4);

            THEN("Generated loot is 2") {
                CHECK(result == 2);
            }
        }

        WHEN("Time passes multiple periods") {
            unsigned result = gen.Generate(milliseconds{ 2000 }, 0, 4);

            THEN("Generated loot is 3") {
                CHECK(result == 3);
            }
        }
    }
}