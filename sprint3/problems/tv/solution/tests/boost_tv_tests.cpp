#define BOOST_TEST_MODULE TV tests
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <sstream>

#include "../src/tv.h"
#include "boost_test_helpers.h"

struct TVFixture {
    TV tv;
};

BOOST_FIXTURE_TEST_SUITE(TV_, TVFixture)

BOOST_AUTO_TEST_CASE(is_off_by_default) {
    BOOST_TEST(!tv.IsTurnedOn());
}

BOOST_AUTO_TEST_CASE(doesnt_show_any_channel_by_default) {
    BOOST_TEST(!tv.GetChannel().has_value());
}

BOOST_AUTO_TEST_CASE(cant_select_any_channel_when_it_is_off) {
    BOOST_CHECK_THROW(tv.SelectChannel(10), std::logic_error);
    BOOST_TEST(tv.GetChannel() == std::nullopt);
}

BOOST_AUTO_TEST_CASE(cant_select_previous_channel_when_it_is_off) {
    BOOST_CHECK_THROW(tv.SelectLastViewedChannel(), std::logic_error);
}

BOOST_AUTO_TEST_CASE(turn_on_turns_tv_on) {
    tv.TurnOn();
    BOOST_TEST(tv.IsTurnedOn());
    BOOST_TEST(tv.GetChannel() == 1);
}

BOOST_AUTO_TEST_CASE(turn_off_when_off_does_nothing) {
    tv.TurnOff();
    BOOST_TEST(!tv.IsTurnedOn());
    BOOST_TEST(!tv.GetChannel().has_value());
}

BOOST_AUTO_TEST_SUITE_END()

struct TurnedOnTVFixture : TVFixture {
    TurnedOnTVFixture() {
        tv.TurnOn();
    }
};

BOOST_FIXTURE_TEST_SUITE(After_turning_on_, TurnedOnTVFixture)

BOOST_AUTO_TEST_CASE(shows_channel_1) {
    BOOST_TEST(tv.IsTurnedOn());
    BOOST_TEST(tv.GetChannel() == 1);
}

BOOST_AUTO_TEST_CASE(can_be_turned_off) {
    tv.TurnOff();
    BOOST_TEST(!tv.IsTurnedOn());
    BOOST_TEST(tv.GetChannel() == std::nullopt);
}

BOOST_AUTO_TEST_CASE(can_select_channel_from_1_to_99) {
    for (int channel = TV::MIN_CHANNEL; channel <= TV::MAX_CHANNEL; ++channel) {
        tv.SelectChannel(channel);
        BOOST_TEST(tv.GetChannel() == channel);
    }
}

BOOST_AUTO_TEST_CASE(cant_select_channel_out_of_range) {
    BOOST_CHECK_THROW(tv.SelectChannel(TV::MIN_CHANNEL - 1), std::out_of_range);
    BOOST_CHECK_THROW(tv.SelectChannel(TV::MAX_CHANNEL + 1), std::out_of_range);
    BOOST_CHECK_THROW(tv.SelectChannel(0), std::out_of_range);
    BOOST_CHECK_THROW(tv.SelectChannel(100), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(selecting_same_channel_does_nothing) {
    tv.SelectChannel(5);
    BOOST_TEST(tv.GetChannel() == 5);
    tv.SelectChannel(5);
    BOOST_TEST(tv.GetChannel() == 5);
    tv.SelectLastViewedChannel();
    BOOST_TEST(tv.GetChannel() == 5);
}

BOOST_AUTO_TEST_CASE(select_last_viewed_channel_switches_between_two_channels) {
    tv.SelectChannel(5);
    tv.SelectChannel(10);
    BOOST_TEST(tv.GetChannel() == 10);

    tv.SelectLastViewedChannel();
    BOOST_TEST(tv.GetChannel() == 5);

    tv.SelectLastViewedChannel();
    BOOST_TEST(tv.GetChannel() == 10);

    tv.SelectLastViewedChannel();
    BOOST_TEST(tv.GetChannel() == 5);
}

BOOST_AUTO_TEST_CASE(select_last_viewed_channel_without_previous_does_nothing) {
    tv.SelectLastViewedChannel();
    BOOST_TEST(tv.GetChannel() == 1);

    tv.SelectChannel(5);
    tv.SelectLastViewedChannel();
    BOOST_TEST(tv.GetChannel() == 1);
}

BOOST_AUTO_TEST_CASE(turn_off_after_channel_change_remembers_last_channel) {
    tv.SelectChannel(5);
    tv.SelectChannel(10);
    tv.TurnOff();
    tv.TurnOn();
    BOOST_TEST(tv.GetChannel() == 1);

    tv.SelectChannel(10);
    tv.SelectChannel(5);
    tv.SelectLastViewedChannel();
    BOOST_TEST(tv.GetChannel() == 10);

    tv.TurnOff();
    tv.TurnOn();
    BOOST_TEST(tv.GetChannel() == 1);
    tv.SelectChannel(10);
    BOOST_TEST(tv.GetChannel() == 10);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()