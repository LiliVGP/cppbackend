#pragma once

#include <boost/json.hpp>
#include <unordered_map>
#include <string>
#include <vector>

#include "tagged.h"
#include "model.h"

namespace extra_data {

    namespace json = boost::json;

    class MapExtraData {
    public:
        void SetLootTypes(json::value loot_types) {
            loot_types_ = std::move(loot_types);
        }
        const json::value& GetLootTypes() const noexcept {
            return loot_types_;
        }

        // Новый метод для хранения стоимости предметов
        void SetLootValues(std::vector<int> values) {
            loot_values_ = std::move(values);
        }
        const std::vector<int>& GetLootValues() const noexcept {
            return loot_values_;
        }

    private:
        json::value loot_types_;
        std::vector<int> loot_values_; // Стоимость каждого типа предмета
    };

    class GameExtraData {
    public:
        void SetMapExtraData(const model::Map::Id& map_id, MapExtraData data) {
            map_data_[map_id] = std::move(data);
        }
        const MapExtraData* GetMapExtraData(const model::Map::Id& map_id) const {
            auto it = map_data_.find(map_id);
            if (it == map_data_.end()) {
                return nullptr;
            }
            return &it->second;
        }

    private:
        using MapIdHasher = util::TaggedHasher<model::Map::Id>;
        std::unordered_map<model::Map::Id, MapExtraData, MapIdHasher> map_data_;
    };

}  // namespace extra_data