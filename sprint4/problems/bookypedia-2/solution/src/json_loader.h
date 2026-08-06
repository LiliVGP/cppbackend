#pragma once

#include <filesystem>

#include <boost/json.hpp>

#include "model.h"
#include "extra_data.h"

namespace json_loader {

namespace json = boost::json;

namespace detail {

void LoadRoads(const json::object& map_obj, model::Map& map);
void LoadBuildings(const json::object& map_obj, model::Map& map);
void LoadOffices(const json::object& map_obj, model::Map& map);
model::Map LoadMap(const json::object& map_obj, extra_data::MapExtraData& extra);

}  // namespace detail

model::Game LoadGame(const std::filesystem::path& json_path, extra_data::GameExtraData& extra_data);

}  // namespace json_loader
