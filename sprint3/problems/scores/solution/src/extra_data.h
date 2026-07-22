#pragma once

#include <boost/json.hpp>
#include <unordered_map>
#include <string>

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

private:
    json::value loot_types_;
};

class GameExtraData {
public:
    void SetMapExtraData(const model::Map::Id& map_id, MapExtraData data) {
        map_data_[map_id] = std::move(data);
    }

    const MapExtraData* GetMapExtraData(const model::Map::Id& map_id) const {
        auto it = map_data_.find(map_id);
        return it != map_data_.end() ? &it->second : nullptr;
    }

private:
    using MapIdHasher = util::TaggedHasher<model::Map::Id>;
    std::unordered_map<model::Map::Id, MapExtraData, MapIdHasher> map_data_;
};

}  // namespace extra_data
