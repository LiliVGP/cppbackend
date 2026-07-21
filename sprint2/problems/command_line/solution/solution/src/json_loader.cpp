#include "json_loader.h"
#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

namespace json = boost::json;

namespace json_loader {

    void LoadGame(const std::filesystem::path& json_path, model::Game& game) {
        std::ifstream file(json_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open config file");
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_str = buffer.str();

        auto value = json::parse(json_str);
        auto obj = value.as_object();

        if (obj.contains("defaultDogSpeed") && obj.at("defaultDogSpeed").is_number()) {
            game.SetDefaultDogSpeed(obj.at("defaultDogSpeed").as_double());
            std::cout << "Default dog speed: " << obj.at("defaultDogSpeed").as_double() << std::endl;
        }

        if (obj.contains("maps") && obj.at("maps").is_array()) {
            auto maps_array = obj.at("maps").as_array();

            std::cout << "Loading " << maps_array.size() << " maps from config file." << std::endl;

            for (const auto& map_value : maps_array) {
                if (!map_value.is_object()) continue;
                auto map_obj = map_value.as_object();

                if (!map_obj.contains("id") || !map_obj.at("id").is_string()) continue;
                if (!map_obj.contains("name") || !map_obj.at("name").is_string()) continue;

                std::string id = std::string(map_obj.at("id").as_string());
                std::string name = std::string(map_obj.at("name").as_string());

                std::cout << "Loading map: " << id << " (" << name << ")" << std::endl;

                model::Map map(model::Map::Id(std::move(id)), std::move(name));

                if (map_obj.contains("dogSpeed") && map_obj.at("dogSpeed").is_number()) {
                    map.SetDogSpeed(map_obj.at("dogSpeed").as_double());
                    std::cout << "  Dog speed: " << map_obj.at("dogSpeed").as_double() << std::endl;
                }

                if (map_obj.contains("roads") && map_obj.at("roads").is_array()) {
                    auto roads_array = map_obj.at("roads").as_array();
                    for (const auto& road_value : roads_array) {
                        if (!road_value.is_object()) continue;
                        auto road_obj = road_value.as_object();

                        if (road_obj.contains("x0") && road_obj.contains("y0")) {
                            int x0 = road_obj.at("x0").as_int64();
                            int y0 = road_obj.at("y0").as_int64();

                            if (road_obj.contains("x1")) {
                                int x1 = road_obj.at("x1").as_int64();
                                map.AddRoad(model::Road(model::Road::HORIZONTAL, { x0, y0 }, x1));
                            }
                            else if (road_obj.contains("y1")) {
                                int y1 = road_obj.at("y1").as_int64();
                                map.AddRoad(model::Road(model::Road::VERTICAL, { x0, y0 }, y1));
                            }
                        }
                    }
                }

                if (map_obj.contains("buildings") && map_obj.at("buildings").is_array()) {
                    auto buildings_array = map_obj.at("buildings").as_array();
                    for (const auto& building_value : buildings_array) {
                        if (!building_value.is_object()) continue;
                        auto building_obj = building_value.as_object();

                        if (!building_obj.contains("x") || !building_obj.contains("y") ||
                            !building_obj.contains("w") || !building_obj.contains("h")) continue;

                        int x = building_obj.at("x").as_int64();
                        int y = building_obj.at("y").as_int64();
                        int w = building_obj.at("w").as_int64();
                        int h = building_obj.at("h").as_int64();

                        map.AddBuilding(model::Building({ {x, y}, {w, h} }));
                    }
                }

                if (map_obj.contains("offices") && map_obj.at("offices").is_array()) {
                    auto offices_array = map_obj.at("offices").as_array();
                    for (const auto& office_value : offices_array) {
                        if (!office_value.is_object()) continue;
                        auto office_obj = office_value.as_object();

                        if (!office_obj.contains("id") || !office_obj.at("id").is_string()) continue;
                        if (!office_obj.contains("x") || !office_obj.contains("y") ||
                            !office_obj.contains("offsetX") || !office_obj.contains("offsetY")) continue;

                        std::string office_id = std::string(office_obj.at("id").as_string());
                        int x = office_obj.at("x").as_int64();
                        int y = office_obj.at("y").as_int64();
                        int offset_x = office_obj.at("offsetX").as_int64();
                        int offset_y = office_obj.at("offsetY").as_int64();

                        map.AddOffice(model::Office(
                            model::Office::Id(std::move(office_id)),
                            { x, y },
                            { offset_x, offset_y }
                        ));
                    }
                }

                game.AddMap(std::move(map));
            }
        }
    }

} // namespace json_loader