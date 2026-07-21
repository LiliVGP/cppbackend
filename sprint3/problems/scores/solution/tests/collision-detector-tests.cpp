#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <sstream>
#include <vector>
#include <cmath>

using namespace collision_detector;
using namespace Catch::Matchers;

namespace Catch {
    template <>
    struct StringMaker<collision_detector::GatheringEvent> {
        static std::string convert(collision_detector::GatheringEvent const& value) {
            std::ostringstream tmp;
            tmp << "{gatherer=" << value.gatherer_id
                << ", item=" << value.item_id
                << ", sq_dist=" << value.sq_distance
                << ", time=" << value.time << "}";
            return tmp.str();
        }
    };
}  // namespace Catch

class TestProvider : public ItemGathererProvider {
public:
    TestProvider(std::vector<Gatherer> gatherers, std::vector<Item> items)
        : gatherers_(std::move(gatherers)), items_(std::move(items)) {
    }

    size_t ItemsCount() const override { return items_.size(); }
    Item GetItem(size_t idx) const override { return items_[idx]; }

    size_t GatherersCount() const override { return gatherers_.size(); }
    Gatherer GetGatherer(size_t idx) const override { return gatherers_[idx]; }

private:
    std::vector<Gatherer> gatherers_;
    std::vector<Item> items_;
};

geom::Point2D P(double x, double y) { return { x, y }; }

Gatherer G(double x1, double y1, double x2, double y2, double width = 0.0) {
    return { P(x1, y1), P(x2, y2), width };
}

Item I(double x, double y, double width = 0.0) {
    return { P(x, y), width };
}

struct GatheringEventMatcher : Catch::Matchers::MatcherBase<std::vector<GatheringEvent>> {
    GatheringEventMatcher(std::vector<GatheringEvent> target) : target_(std::move(target)) {}

    bool match(const std::vector<GatheringEvent>& source) const override {
        if (source.size() != target_.size()) return false;
        for (size_t i = 0; i < source.size(); ++i) {
            const auto& s = source[i];
            const auto& t = target_[i];
            if (s.gatherer_id != t.gatherer_id) return false;
            if (s.item_id != t.item_id) return false;
            if (!WithinAbsMatcher(t.sq_distance, 1e-10).match(s.sq_distance)) return false;
            if (!WithinAbsMatcher(t.time, 1e-10).match(s.time)) return false;
        }
        return true;
    }

    std::string describe() const override {
        std::ostringstream ss;
        ss << "equals [";
        for (size_t i = 0; i < target_.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << StringMaker<GatheringEvent>::convert(target_[i]);
        }
        ss << "]";
        return ss.str();
    }

private:
    std::vector<GatheringEvent> target_;
};

inline GatheringEventMatcher EqualsEvents(std::vector<GatheringEvent> events) {
    return GatheringEventMatcher(std::move(events));
}


SCENARIO("Collision detection") {

    GIVEN("A single gatherer moving straight") {
        auto gatherer = G(0, 0, 10, 0);

        WHEN("The item is exactly on the path") {
            TestProvider provider({ gatherer }, { I(5, 0) });
            auto events = FindGatherEvents(provider);

            THEN("An event should be detected at time 0.5") {
                std::vector<GatheringEvent> expected = { {0, 0, 0.0, 0.5} };
                REQUIRE_THAT(events, EqualsEvents(std::move(expected)));
            }
        }

        WHEN("The item is slightly off the path but within radius") {
            TestProvider provider({ G(0, 0, 10, 0, 1.0) }, { I(5, 0.9, 1.0) });
            auto events = FindGatherEvents(provider);

            THEN("An event should be detected") {
                REQUIRE(events.size() == 1);
                auto& e = events[0];
                REQUIRE(e.item_id == 0);
                REQUIRE(e.gatherer_id == 0);
                REQUIRE_THAT(e.time, WithinAbs(0.5, 1e-10));
                REQUIRE_THAT(e.sq_distance, WithinAbs(0.81, 1e-10));
            }
        }

        WHEN("The item is behind the gatherer (proj_ratio < 0)") {
            TestProvider provider({ gatherer }, { I(-1, 0) });
            auto events = FindGatherEvents(provider);
            THEN("No event should be detected") {
                REQUIRE(events.empty());
            }
        }

        WHEN("The item is ahead of the gatherer (proj_ratio > 1)") {
            TestProvider provider({ gatherer }, { I(11, 0) });
            auto events = FindGatherEvents(provider);
            THEN("No event should be detected") {
                REQUIRE(events.empty());
            }
        }

        WHEN("The item is too far from the path") {
            TestProvider provider({ gatherer }, { I(5, 3) });
            auto events = FindGatherEvents(provider);
            THEN("No event should be detected") {
                REQUIRE(events.empty());
            }
        }
    }

    GIVEN("A gatherer moving backwards") {
        auto gatherer = G(10, 0, 0, 0);

        WHEN("An item is on the path from start to end") {
            TestProvider provider({ gatherer }, { I(5, 0) });
            auto events = FindGatherEvents(provider);

            THEN("The event time should be 0.5") {
                REQUIRE(events.size() == 1);
                REQUIRE_THAT(events[0].time, WithinAbs(0.5, 1e-10));
            }
        }

        WHEN("An item is at the start position") {
            TestProvider provider({ gatherer }, { I(10, 0) });
            auto events = FindGatherEvents(provider);
            THEN("An event should be detected at time 0") {
                REQUIRE(events.size() == 1);
                REQUIRE_THAT(events[0].time, WithinAbs(0.0, 1e-10));
            }
        }

        WHEN("An item is at the end position") {
            TestProvider provider({ gatherer }, { I(0, 0) });
            auto events = FindGatherEvents(provider);
            THEN("An event should be detected at time 1") {
                REQUIRE(events.size() == 1);
                REQUIRE_THAT(events[0].time, WithinAbs(1.0, 1e-10));
            }
        }
    }

    GIVEN("Multiple gatherers and items") {
        Gatherer g1 = G(0, 0, 10, 0);
        Gatherer g2 = G(0, 2, 10, 2);

        Item item1 = I(5, 0);
        Item item2 = I(5, 2.1, 1.0);

        WHEN("Finding events") {
            TestProvider provider({ g1, g2 }, { item1, item2 });
            auto events = FindGatherEvents(provider);

            THEN("All valid events should be detected and sorted by time") {

                std::vector<GatheringEvent> expected = {
                    {0, 0, 0.0, 0.5},
                    {1, 1, 0.0, 0.5}
                };

                REQUIRE(events.size() == 2);

                auto find_event = [&](size_t g, size_t i) {
                    return std::find_if(events.begin(), events.end(),
                        [&](const GatheringEvent& e) {
                            return e.gatherer_id == g && e.item_id == i;
                        });
                    };

                auto it1 = find_event(0, 0);
                auto it2 = find_event(1, 1);

                REQUIRE(it1 != events.end());
                REQUIRE(it2 != events.end());

                auto bad = find_event(1, 0);
                REQUIRE(bad == events.end());
            }
        }
    }
}