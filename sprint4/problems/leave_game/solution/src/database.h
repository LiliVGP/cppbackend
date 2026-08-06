#pragma once

#include "connection_pool.h"

#include <string>
#include <vector>

namespace database {

struct RetiredPlayer {
    std::string name;
    int score;
    double play_time;
};

void InitializeDatabase(ConnectionPool& pool);

void AddRetiredPlayer(ConnectionPool& pool, const RetiredPlayer& player);

std::vector<RetiredPlayer> GetRecords(ConnectionPool& pool, int start, int max_items);

}  // namespace database
