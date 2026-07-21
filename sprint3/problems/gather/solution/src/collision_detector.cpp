#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

    CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
        assert(b.x != a.x || b.y != a.y);
        const double u_x = c.x - a.x;
        const double u_y = c.y - a.y;
        const double v_x = b.x - a.x;
        const double v_y = b.y - a.y;
        const double u_dot_v = u_x * v_x + u_y * v_y;
        const double u_len2 = u_x * u_x + u_y * u_y;
        const double v_len2 = v_x * v_x + v_y * v_y;
        const double proj_ratio = u_dot_v / v_len2;
        const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

        return CollectionResult(sq_distance, proj_ratio);
    }

    std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
        std::vector<GatheringEvent> events;

        for (size_t g = 0; g < provider.GatherersCount(); ++g) {
            Gatherer gatherer = provider.GetGatherer(g);
            geom::Point2D start = gatherer.start_pos;
            geom::Point2D end = gatherer.end_pos;

            if (start.x == end.x && start.y == end.y) {
                continue;
            }

            double gatherer_radius = gatherer.width;

            for (size_t i = 0; i < provider.ItemsCount(); ++i) {
                Item item = provider.GetItem(i);
                double item_radius = item.width;

                CollectionResult result = TryCollectPoint(start, end, item.position);

                double total_radius = gatherer_radius + item_radius;

                if (result.IsCollected(total_radius)) {
                    events.push_back({ i, g, result.sq_distance, result.proj_ratio });
                }
            }
        }

        std::sort(events.begin(), events.end(),
            [](const GatheringEvent& a, const GatheringEvent& b) {
                if (a.time != b.time) {
                    return a.time < b.time;
                }
                if (a.gatherer_id != b.gatherer_id) {
                    return a.gatherer_id < b.gatherer_id;
                }
                return a.item_id < b.item_id;
            });

        return events;
    }

}  // namespace collision_detector