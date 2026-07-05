#include "model.h"

#include <cstdlib>
#include <stdexcept>

namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        json::object stop_obj;
        stop_obj["code"] = EXIT_FAILURE;
        stop_obj["exception"] = "Map with id "s + *map.GetId() + " already exists"s;
        BOOST_LOG_TRIVIAL(fatal)
        << logging::add_value("data", stop_obj)
        << logging::add_value("msg", "error"s);
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            json::object stop_obj;
            stop_obj["code"] = EXIT_FAILURE;
            stop_obj["exception"] = ""s;
            BOOST_LOG_TRIVIAL(fatal)
            << logging::add_value("data", stop_obj)
            << logging::add_value("msg", ""s);
            throw;
        }
    }
}

}  // namespace model
