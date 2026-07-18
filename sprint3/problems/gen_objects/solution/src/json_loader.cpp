#include "json_loader.h"
#include <fstream>
#include <sstream>
#include <boost/json.hpp>

namespace json = boost::json;

static std::chrono::milliseconds ParsePeriod(double seconds) {
    return std::chrono::milliseconds(static_cast<int>(seconds * 1000));
}

ConfigLoader ConfigLoader::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    json::value root;
    try {
        root = json::parse(buffer.str());
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse JSON from " + path + ": " + e.what());
    }

    if (!root.is_object()) {
        throw std::runtime_error("Config root is not an object in " + path);
    }

    json::object obj = root.as_object();
    ConfigLoader loader;

    auto loot_gen_it = obj.find("lootGeneratorConfig");
    if (loot_gen_it != obj.end()) {
        auto& config_obj = loot_gen_it->value().as_object();
        double period = config_obj.at("period").as_double();
        double probability = config_obj.at("probability").as_double();
        loader.loot_gen_config_ = { ParsePeriod(period), probability };
    }

    auto maps_it = obj.find("maps");
    if (maps_it != obj.end()) {
        auto& maps_arr = maps_it->value().as_array();
        for (const auto& map_val : maps_arr) {
            auto& map_obj = map_val.as_object();

            ConfigLoader::MapInfo map_info;
            map_info.id = std::string(map_obj.at("id").as_string());
            map_info.name = std::string(map_obj.at("name").as_string());

            auto roads_it = map_obj.find("roads");
            if (roads_it != map_obj.end()) {
                auto& roads_arr = roads_it->value().as_array();
                for (const auto& road_val : roads_arr) {
                    auto& road_obj = road_val.as_object();
                    double x0 = road_obj.at("x0").as_double();
                    double y0 = road_obj.at("y0").as_double();
                    double x1 = road_obj.contains("x1") ? road_obj.at("x1").as_double() : road_obj.at("x0").as_double();
                    double y1 = road_obj.contains("y1") ? road_obj.at("y1").as_double() : road_obj.at("y0").as_double();
                    map_info.roads.emplace_back(x0, y0, x1, y1);
                }
            }

            auto buildings_it = map_obj.find("buildings");
            if (buildings_it != map_obj.end()) {
                auto& buildings_arr = buildings_it->value().as_array();
                for (const auto& building_val : buildings_arr) {
                    auto& building_obj = building_val.as_object();
                    Map::Building building;
                    building.position.x = building_obj.at("x").as_double();
                    building.position.y = building_obj.at("y").as_double();
                    building.width = building_obj.at("w").as_double();
                    building.height = building_obj.at("h").as_double();
                    map_info.buildings.push_back(building);
                }
            }

            auto offices_it = map_obj.find("offices");
            if (offices_it != map_obj.end()) {
                auto& offices_arr = offices_it->value().as_array();
                for (const auto& office_val : offices_arr) {
                    auto& office_obj = office_val.as_object();
                    Map::Office office;
                    office.position.x = office_obj.at("x").as_double();
                    office.position.y = office_obj.at("y").as_double();
                    office.offset_x = office_obj.at("offsetX").as_double();
                    office.offset_y = office_obj.at("offsetY").as_double();
                    map_info.offices.push_back(office);
                }
            }

            auto loot_types_it = map_obj.find("lootTypes");
            if (loot_types_it != map_obj.end()) {
                auto& loot_types_arr = loot_types_it->value().as_array();
                for (const auto& loot_type_val : loot_types_arr) {
                    auto& loot_type_obj = loot_type_val.as_object();
                    ConfigLoader::LootTypeInfo type_info;
                    type_info.name = std::string(loot_type_obj.at("name").as_string());
                    type_info.file = std::string(loot_type_obj.at("file").as_string());
                    type_info.type = std::string(loot_type_obj.at("type").as_string());

                    if (loot_type_obj.contains("rotation")) {
                        type_info.rotation = loot_type_obj.at("rotation").as_double();
                    }
                    if (loot_type_obj.contains("color")) {
                        type_info.color = std::string(loot_type_obj.at("color").as_string());
                    }
                    if (loot_type_obj.contains("scale")) {
                        type_info.scale = loot_type_obj.at("scale").as_double();
                    }
                    map_info.loot_types.push_back(type_info);
                }
            }

            loader.maps_.push_back(std::move(map_info));
        }
    }

    return loader;
}