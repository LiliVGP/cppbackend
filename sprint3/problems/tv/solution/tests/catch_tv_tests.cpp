#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "../src/tv.h"

namespace Catch {

    template <>
    struct StringMaker<std::nullopt_t> {
        static std::string convert(std::nullopt_t) {
            using namespace std::literals;
            return "nullopt"s;
        }
    };

    template <typename T>
    struct StringMaker<std::optional<T>> {
        static std::string convert(const std::optional<T>& opt_value) {
            if (opt_value) {
                return StringMaker<T>::convert(*opt_value);
            }
            else {
                return StringMaker<std::nullopt_t>::convert(std::nullopt);
            }
        }
    };

}  // namespace Catch

SCENARIO("TV", "[TV]") {
    GIVEN("A TV") {
        TV tv;

        SECTION("Initially it is off and doesn't show any channel") {
            CHECK(!tv.IsTurnedOn());
            CHECK(!tv.GetChannel().has_value());
        }

        SECTION("When it is turned off initially") {
            tv.TurnOff();
            CHECK(!tv.IsTurnedOn());
            CHECK(!tv.GetChannel().has_value());
        }

        SECTION("When it is turned on first time") {
            tv.TurnOn();

            THEN("it is turned on and shows channel #1") {
                CHECK(tv.IsTurnedOn());
                CHECK(tv.GetChannel() == 1);

                AND_WHEN("it is turned off") {
                    tv.TurnOff();

                    THEN("it is turned off and doesn't show any channel") {
                        CHECK(!tv.IsTurnedOn());
                        CHECK(tv.GetChannel() == std::nullopt);
                    }
                }

                AND_WHEN("it can select channel from 1 to 99") {
                    for (int channel = TV::MIN_CHANNEL; channel <= TV::MAX_CHANNEL; ++channel) {
                        tv.SelectChannel(channel);
                        CHECK(tv.GetChannel() == channel);
                    }
                }

                AND_WHEN("it tries to select channel out of range") {
                    CHECK_THROWS_AS(tv.SelectChannel(TV::MIN_CHANNEL - 1), std::out_of_range);
                    CHECK_THROWS_AS(tv.SelectChannel(TV::MAX_CHANNEL + 1), std::out_of_range);
                    CHECK_THROWS_AS(tv.SelectChannel(0), std::out_of_range);
                    CHECK_THROWS_AS(tv.SelectChannel(100), std::out_of_range);
                }

                AND_WHEN("it selects the same channel") {
                    tv.SelectChannel(5);
                    CHECK(tv.GetChannel() == 5);
                    tv.SelectChannel(5);
                    CHECK(tv.GetChannel() == 5);
                    tv.SelectLastViewedChannel();
                    CHECK(tv.GetChannel() == 5);
                }

                AND_WHEN("it selects last viewed channel") {
                    tv.SelectChannel(5);
                    tv.SelectChannel(10);
                    CHECK(tv.GetChannel() == 10);

                    tv.SelectLastViewedChannel();
                    CHECK(tv.GetChannel() == 5);

                    tv.SelectLastViewedChannel();
                    CHECK(tv.GetChannel() == 10);

                    tv.SelectLastViewedChannel();
                    CHECK(tv.GetChannel() == 5);
                }

                AND_WHEN("it selects last viewed channel without previous") {
                    tv.SelectLastViewedChannel();
                    CHECK(tv.GetChannel() == 1);

                    tv.SelectChannel(5);
                    tv.SelectLastViewedChannel();
                    CHECK(tv.GetChannel() == 1);
                }

                AND_WHEN("it is turned off and on again") {
                    tv.SelectChannel(5);
                    tv.TurnOff();
                    tv.TurnOn();
                    CHECK(tv.GetChannel() == 1);
                }
            }
        }

        SECTION("When it is off, it can't select any channel") {
            CHECK_THROWS_AS(tv.SelectChannel(10), std::logic_error);
            CHECK(tv.GetChannel() == std::nullopt);
        }

        SECTION("When it is off, it can't select last viewed channel") {
            CHECK_THROWS_AS(tv.SelectLastViewedChannel(), std::logic_error);
        }
    }
}