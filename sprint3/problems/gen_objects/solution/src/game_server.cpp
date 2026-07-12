#include "game_server.h"
#include <iostream>
#include <thread>

GameServer::GameServer(const std::string& config_file) {
    try {
        config_ = std::make_unique<ConfigLoader>(ConfigLoader::LoadFromFile(config_file));
        InitializeMaps();
        InitializeGameState();
        std::cout << "Game server initialized successfully!" << std::endl;
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Failed to initialize game server: " + std::string(e.what()));
    }
}

void GameServer::InitializeMaps() {
    const auto& map_infos = config_->GetMaps();
    for (const auto& map_info : map_infos) {
        auto map = std::make_unique<Map>(
            map_info.id,
            map_info.name,
            map_info.loot_types.size()
        );

        for (const auto& road : map_info.roads) {
            map->AddRoad(road);
        }
        for (const auto& building : map_info.buildings) {
            map->AddBuilding(building);
        }
        for (const auto& office : map_info.offices) {
            map->AddOffice(office);
        }

        maps_[map_info.id] = std::move(map);
    }
}

void GameServer::InitializeGameState() {
    auto& gen_config = config_->GetLootGeneratorConfig();
    auto loot_gen = std::make_shared<loot_gen::LootGenerator>(
        gen_config.period,
        gen_config.probability
    );
    auto rng = std::make_unique<std::mt19937>(std::random_device{}());
    game_state_ = std::make_unique<GameState>(loot_gen, std::move(rng));

    // Устанавливаем первую карту как текущую
    if (!maps_.empty()) {
        game_state_->SetCurrentMap(maps_.begin()->second.get());
    }
}

void GameServer::ProcessTick(std::chrono::milliseconds delta) {
    time_ += delta;
    game_state_->GenerateLoot(delta);
}

void GameServer::Run() {
    const auto tick_duration = std::chrono::milliseconds(100); // 10 FPS

    std::cout << "Game server running... (Ctrl+C to stop)" << std::endl;

    while (true) {
        auto start = std::chrono::steady_clock::now();

        ProcessTick(tick_duration);

        // Здесь будет обработка REST API запросов
        // (пока заглушка)

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (elapsed < tick_duration) {
            std::this_thread::sleep_for(tick_duration - elapsed);
        }
    }
}