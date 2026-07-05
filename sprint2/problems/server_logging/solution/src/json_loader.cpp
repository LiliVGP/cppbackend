#include "json_loader.h"
#include "boost/json/parse.hpp"
#include "boost/json/value_to.hpp"
#include "model.h"
#include <boost/json.hpp>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace json = boost::json;
namespace json_loader {

void LogErr(const std::string& exception){
    using namespace std::literals;
    json::object resp_obj;
    resp_obj["code"] = EXIT_FAILURE;
    resp_obj["exception"] = exception;
    BOOST_LOG_TRIVIAL(info)
    << logging::add_value("data", resp_obj)
    << logging::add_value("msg", "server exited"s);
}

void OfficeBuilder(model::Map& map, const json::object& map_obj){
    for(const auto& offices_val : map_obj.at("offices").as_array()){
        auto offices_obj = offices_val.as_object();
        model::Office::Id office_id{json::value_to<std::string>(offices_obj.at("id"))};
        model::Point position{
            .x = static_cast<int>(offices_obj.at("x").as_int64()),
            .y = static_cast<int>(offices_obj.at("y").as_int64())
        };
        model::Offset offset{
            .dx = static_cast<int>(offices_obj.at("offsetX").as_int64()),
            .dy = static_cast<int>(offices_obj.at("offsetY").as_int64())
        };
        map.AddOffice({office_id, position, offset});
    }
}

void BuildingBuilder(model::Map& map, const json::object& map_obj){
    for(const auto& buildings_val : map_obj.at("buildings").as_array()){
        auto building_obj = buildings_val.as_object();
        model::Point position{
            .x = static_cast<int>(building_obj.at("x").as_int64()),
            .y = static_cast<int>(building_obj.at("y").as_int64())
        };
        model::Size size{
            .width = static_cast<int>(building_obj.at("w").as_int64()),
            .height = static_cast<int>(building_obj.at("h").as_int64())
        };
        map.AddBuilding(model::Building{model::Rectangle{position, size}});
    }
}

void RoadBuilder(model::Map& map, const json::object& map_obj){
    for(const auto& road_val : map_obj.at("roads").as_array()){
        auto road_object = road_val.as_object();
        model::Point start{
            .x = static_cast<int>(road_object.at("x0").as_int64()),
            .y = static_cast<int>(road_object.at("y0").as_int64())
        };
        if(road_object.contains("x1")){
            int end_x = static_cast<int>(road_object.at("x1").as_int64());
            map.AddRoad(model::Road(model::Road::HORIZONTAL, start, end_x));
        }else{
            int end_y = static_cast<int>(road_object.at("y1").as_int64());
            map.AddRoad(model::Road(model::Road::VERTICAL, start, end_y));
        }
    }
}

model::Map MapBuilder(const json::object& map_obj){
    model::Map::Id map_id{json::value_to<std::string>(map_obj.at("id"))};
    std::string map_name = json::value_to<std::string>(map_obj.at("name"));
    model::Map map{map_id, map_name};
    RoadBuilder(map, map_obj);
    BuildingBuilder(map, map_obj);
    OfficeBuilder(map, map_obj);
    return map;
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream in(json_path);
    if(!in.is_open()){
        LogErr("json file dont open");
        throw std::runtime_error("json file dont open");
    }
    json::value value;
    json::object root;
    try{
        std::string str_json{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        value = json::parse(str_json);
        root = value.as_object();
    }catch(const std::exception& e){
        LogErr(e.what());
        throw std::runtime_error("json is invalid");
    }


    model::Game game;
    for(const auto& map_val : root.at("maps").as_array()){
        auto map_obj = map_val.as_object();
        model::Map map = MapBuilder(map_obj);
        game.AddMap(map);
    }


    return game;
}

}  // namespace json_loader
