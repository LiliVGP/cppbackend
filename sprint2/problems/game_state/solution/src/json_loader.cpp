#include "json_loader.h"
#include <boost/json.hpp>
#include <fstream>
#include <sstream>

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

        if (obj.contains("maps") && obj.at("maps").is_array()) {
            auto maps_array = obj.at("maps").as_array();

            for (const auto& map_value : maps_array) {
                auto map_obj = map_value.as_object();

                std::string id = std::string(map_obj.at("id").as_string());
                std::string name = std::string(map_obj.at("name").as_string());

                model::Map map(model::Map::Id(std::move(id)), std::move(name));

                if (map_obj.contains("roads") && map_obj.at("roads").is_array()) {
                    auto roads_array = map_obj.at("roads").as_array();

                    for (const auto& road_value : roads_array) {
                        auto road_obj = road_value.as_object();

                        if (road_obj.contains("x0") && road_obj.contains("y0")) {
                            int x0 = road_obj.at("x0").as_int64();
                            int y0 = road_obj.at("y0").as_int64();

                            if (road_obj.contains("x1")) {
                                int x1 = road_obj.at("x1").as_int64();
                                map.AddRoad(model::Road(model::Road::HORIZONTAL,
                                    { x0, y0 }, x1));
                            }
                            else if (road_obj.contains("y1")) {
                                int y1 = road_obj.at("y1").as_int64();
                                map.AddRoad(model::Road(model::Road::VERTICAL,
                                    { x0, y0 }, y1));
                            }
                        }
                    }
                }

                if (map_obj.contains("buildings") && map_obj.at("buildings").is_array()) {
                    auto buildings_array = map_obj.at("buildings").as_array();

                    for (const auto& building_value : buildings_array) {
                        auto building_obj = building_value.as_object();

                        int x = building_obj.at("x").as_int64();
                        int y = building_obj.at("y").as_int64();
                        int w = building_obj.at("w").as_int64();
                        int h = building_obj.at("h").as_int64();

                        model::Rectangle rect{ {x, y}, {w, h} };
                        map.AddBuilding(model::Building(rect));
                    }
                }

                if (map_obj.contains("offices") && map_obj.at("offices").is_array()) {
                    auto offices_array = map_obj.at("offices").as_array();

                    for (const auto& office_value : offices_array) {
                        auto office_obj = office_value.as_object();

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

}  // namespace json_loader