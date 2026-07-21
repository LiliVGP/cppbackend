#include "json_loader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json_loader {

    namespace detail {

        void LoadRoads(const json::object& map_obj, model::Map& map) {
            if (!map_obj.if_contains("roads")) {
                return;
            }

            for (const json::value& road_val : map_obj.at("roads").as_array()) {
                const json::object& road_obj = road_val.as_object();
                model::Point start{
                    static_cast<model::Coord>(road_obj.at("x0").as_int64()),
                    static_cast<model::Coord>(road_obj.at("y0").as_int64())
                };

                if (road_obj.if_contains("x1")) {
                    model::Coord end_x = static_cast<model::Coord>(road_obj.at("x1").as_int64());
                    map.AddRoad(model::Road(model::Road::HORIZONTAL, start, end_x));
                }
                else {
                    model::Coord end_y = static_cast<model::Coord>(road_obj.at("y1").as_int64());
                    map.AddRoad(model::Road(model::Road::VERTICAL, start, end_y));
                }
            }
        }

        void LoadBuildings(const json::object& map_obj, model::Map& map) {
            if (!map_obj.if_contains("buildings")) {
                return;
            }

            for (const json::value& building_val : map_obj.at("buildings").as_array()) {
                const json::object& building_obj = building_val.as_object();
                model::Point position{
                    static_cast<model::Coord>(building_obj.at("x").as_int64()),
                    static_cast<model::Coord>(building_obj.at("y").as_int64())
                };
                model::Size size{
                    static_cast<model::Dimension>(building_obj.at("w").as_int64()),
                    static_cast<model::Dimension>(building_obj.at("h").as_int64())
                };
                map.AddBuilding(model::Building(model::Rectangle{ position, size }));
            }
        }

        void LoadOffices(const json::object& map_obj, model::Map& map) {
            if (!map_obj.if_contains("offices")) {
                return;
            }

            for (const json::value& office_val : map_obj.at("offices").as_array()) {
                const json::object& office_obj = office_val.as_object();
                model::Office::Id office_id(std::string(office_obj.at("id").as_string()));
                model::Point position{
                    static_cast<model::Coord>(office_obj.at("x").as_int64()),
                    static_cast<model::Coord>(office_obj.at("y").as_int64())
                };
                model::Offset offset{
                    static_cast<model::Dimension>(office_obj.at("offsetX").as_int64()),
                    static_cast<model::Dimension>(office_obj.at("offsetY").as_int64())
                };
                map.AddOffice(model::Office(std::move(office_id), position, offset));
            }
        }

        model::Map LoadMap(const json::object& map_obj, extra_data::MapExtraData& extra) {
            model::Map::Id map_id(std::string(map_obj.at("id").as_string()));
            std::string map_name(map_obj.at("name").as_string());

            model::Map map(std::move(map_id), std::move(map_name));

            if (map_obj.contains("dogSpeed")) {
                map.SetDogSpeed(map_obj.at("dogSpeed").as_double());
            }

            if (map_obj.contains("bagCapacity")) {
                map.SetBagCapacity(map_obj.at("bagCapacity").as_int64());
            }

            if (map_obj.contains("lootTypes")) {
                const auto& loot_types = map_obj.at("lootTypes").as_array();
                extra.SetLootTypes(json::value(loot_types));
                map.SetLootTypeCount(loot_types.size());
            }

            LoadRoads(map_obj, map);
            LoadBuildings(map_obj, map);
            LoadOffices(map_obj, map);

            return map;
        }

    }  // namespace detail

    model::Game LoadGame(const std::filesystem::path& json_path, extra_data::GameExtraData& extra_data) {
        std::ifstream file(json_path);
        if (!file) {
            throw std::runtime_error("Failed to open file: " + json_path.string());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        json::value json_val = json::parse(buffer.str());
        json::object& root = json_val.as_object();

        model::Game game;

        if (root.contains("defaultDogSpeed")) {
            game.SetDefaultDogSpeed(root.at("defaultDogSpeed").as_double());
        }

        if (root.contains("defaultBagCapacity")) {
            game.SetDefaultBagCapacity(root.at("defaultBagCapacity").as_int64());
        }

        if (root.contains("lootGeneratorConfig")) {
            const auto& lg = root.at("lootGeneratorConfig").as_object();
            double period = lg.at("period").as_double();
            double probability = lg.at("probability").as_double();
            game.SetLootGeneratorConfig(period, probability);
        }

        for (const json::value& map_val : root.at("maps").as_array()) {
            const json::object& map_obj = map_val.as_object();
            extra_data::MapExtraData map_extra;
            game.AddMap(detail::LoadMap(map_obj, map_extra));
            extra_data.SetMapExtraData(game.GetMaps().back().GetId(), std::move(map_extra));
        }

        return game;
    }

}  // namespace json_loader