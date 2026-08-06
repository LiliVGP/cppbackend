#include "database.h"

#include <pqxx/transaction>
#include <pqxx/result>
#include <pqxx/row>
#include <iostream>

namespace database {

void InitializeDatabase(ConnectionPool& pool) {
    auto conn = pool.GetConnection();
    pqxx::work tx{*conn};
    tx.exec(
        "CREATE TABLE IF NOT EXISTS retired_players ("
        "    id SERIAL PRIMARY KEY,"
        "    name VARCHAR NOT NULL,"
        "    score INTEGER NOT NULL,"
        "    play_time DOUBLE PRECISION NOT NULL"
        ")"
    );
    tx.exec(
        "CREATE INDEX IF NOT EXISTS idx_retired_players_sort "
        "ON retired_players (score DESC, play_time ASC, name ASC)"
    );
    tx.commit();
}

void AddRetiredPlayer(ConnectionPool& pool, const RetiredPlayer& player) {
    auto conn = pool.GetConnection();
    pqxx::work tx{*conn};
    tx.exec_params(
        "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3)",
        player.name, player.score, player.play_time
    );
    tx.commit();
}

std::vector<RetiredPlayer> GetRecords(ConnectionPool& pool, int start, int max_items) {
    auto conn = pool.GetConnection();
    pqxx::read_transaction tx{*conn};
    auto rows = tx.exec_params(
        "SELECT name, score, play_time FROM retired_players "
        "ORDER BY score DESC, play_time ASC, name ASC "
        "LIMIT $1 OFFSET $2",
        max_items, start
    );

    std::vector<RetiredPlayer> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        RetiredPlayer player;
        player.name = row["name"].as<std::string>();
        player.score = row["score"].as<int>();
        player.play_time = row["play_time"].as<double>();
        result.push_back(std::move(player));
    }
    return result;
}

}  // namespace database
