#pragma once

#include <filesystem>
#include <boost/json.hpp>
#include "model.h"
namespace json = boost::json;
namespace json_loader {

void OfficeBuilder(model::Map& map, const json::object& map_obj);
void BuildingBuilder(model::Map& map, const json::object& map_obj); 
void RoadBuilder(model::Map& map, const json::object& map_obj);
model::Map MapBuilder(const json::object& map_obj);
model::Game LoadGame(const std::filesystem::path& json_path);

}  
