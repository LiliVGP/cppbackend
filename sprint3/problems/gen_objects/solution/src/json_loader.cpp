#include "json_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <boost/json.hpp>

namespace json = boost::json;

static std::chrono::milliseconds ParsePeriod(double seconds) {
    return std::chrono::milliseconds(static_cast<int>(seconds * 1000));
}

ConfigLoader ConfigLoader::LoadFromFile(const std::string& path) {
    std::cout << "DEBUG: Entered LoadFromFile" << std::endl;

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + path);
    }
    std::cout << "DEBUG: File opened successfully" << std::endl;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::cout << "DEBUG: File read into buffer, size = " << buffer.str().size() << " bytes" << std::endl;

    json::value root;
    try {
        root = json::parse(buffer.str());
        std::cout << "DEBUG: JSON parsed successfully" << std::endl;
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse JSON from " + path + ": " + e.what());
    }

    if (!root.is_object()) {
        throw std::runtime_error("Config root is not an object in " + path);
    }
    std::cout << "DEBUG: Root is an object" << std::endl;

    json::object obj = root.as_object();
    ConfigLoader loader;

    auto loot_gen_it = obj.find("lootGeneratorConfig");
    if (loot_gen_it != obj.end()) {
        auto& config_obj = loot_gen_it->value().as_object();
        double period = config_obj.at("period").as_double();
        double probability = config_obj.at("probability").as_double();
        loader.loot_gen_config_ = { ParsePeriod(period), probability };
        std::cout << "DEBUG: LootGeneratorConfig parsed: period=" << period << ", probability=" << probability << std::endl;
    }

    auto maps_it = obj.find("maps");
    if (maps_it != obj.end()) {
        auto& maps_arr = maps_it->value().as_array();
        std::cout << "DEBUG: Found " << maps_arr.size() << " maps to parse" << std::endl;

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

                    double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;

                    if (road_obj.contains("x0") && road_obj.at("x0").is_number()) {
                        x0 = road_obj.at("x0").to_number<double>();
                    }
                    if (road_obj.contains("y0") && road_obj.at("y0").is_number()) {
                        y0 = road_obj.at("y0").to_number<double>();
                    }

                    if (road_obj.contains("x1") && road_obj.at("x1").is_number()) {
                        x1 = road_obj.at("x1").to_number<double>();
                    }
                    else {
                        x1 = x0;
                    }
                    if (road_obj.contains("y1") && road_obj.at("y1").is_number()) {
                        y1 = road_obj.at("y1").to_number<double>();
                    }
                    else {
                        y1 = y0;
                    }

                    map_info.roads.emplace_back(x0, y0, x1, y1);
                }
            }

            auto buildings_it = map_obj.find("buildings");
            if (buildings_it != map_obj.end()) {
                auto& buildings_arr = buildings_it->value().as_array();
                for (const auto& building_val : buildings_arr) {
                    auto& building_obj = building_val.as_object();
                    Map::Building building = {};

                    if (building_obj.contains("x") && building_obj.at("x").is_number()) {
                        building.position.x = building_obj.at("x").to_number<double>();
                    }
                    if (building_obj.contains("y") && building_obj.at("y").is_number()) {
                        building.position.y = building_obj.at("y").to_number<double>();
                    }
                    if (building_obj.contains("w") && building_obj.at("w").is_number()) {
                        building.width = building_obj.at("w").to_number<double>();
                    }
                    if (building_obj.contains("h") && building_obj.at("h").is_number()) {
                        building.height = building_obj.at("h").to_number<double>();
                    }

                    map_info.buildings.push_back(building);
                }
            }

            auto offices_it = map_obj.find("offices");
            if (offices_it != map_obj.end()) {
                auto& offices_arr = offices_it->value().as_array();
                for (const auto& office_val : offices_arr) {
                    auto& office_obj = office_val.as_object();
                    Map::Office office = {};

                    if (office_obj.contains("id") && office_obj.at("id").is_string()) {
                        office.id = office_obj.at("id").as_string().c_str();
                    }

                    if (office_obj.contains("x") && office_obj.at("x").is_number()) {
                        office.position.x = office_obj.at("x").to_number<double>();
                    }
                    if (office_obj.contains("y") && office_obj.at("y").is_number()) {
                        office.position.y = office_obj.at("y").to_number<double>();
                    }
                    if (office_obj.contains("offsetX") && office_obj.at("offsetX").is_number()) {
                        office.offset_x = office_obj.at("offsetX").to_number<double>();
                    }
                    if (office_obj.contains("offsetY") && office_obj.at("offsetY").is_number()) {
                        office.offset_y = office_obj.at("offsetY").to_number<double>();
                    }

                    map_info.offices.push_back(office);
                }
            }

            auto loot_types_it = map_obj.find("lootTypes");
            if (loot_types_it != map_obj.end()) {
                map_info.loot_types = loot_types_it->value().as_array();
            }

            loader.maps_.push_back(std::move(map_info));
            std::cout << "DEBUG: Map '" << map_info.id << "' parsed successfully" << std::endl;
        }
    }

    std::cout << "DEBUG: LoadFromFile completed successfully" << std::endl;
    return loader;
}