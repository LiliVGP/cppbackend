#include <boost/test/unit_test.hpp>
#include <iostream>
#include <sstream>

#include "../src/controller.h"
#include "boost_test_helpers.h"

using namespace std::literals;

struct ControllerFixture {
    TV tv;
    std::istringstream input;
    std::ostringstream output;
    Menu menu{ input, output };
    Controller controller{ tv, menu };

    void RunMenuCommand(std::string command) {
        input.str(std::move(command));
        input.clear();
        menu.Run();
    }

    void ExpectExtraArgumentsErrorInOutput(std::string_view command) const {
        ExpectOutput(
            "Error: the "s.append(command).append(" command does not require any arguments\n"sv));
    }

    void ExpectOutput(std::string_view expected) const {
        BOOST_TEST(output.str() == expected);
    }
};

struct WhenTVIsOffFixture : ControllerFixture {
    WhenTVIsOffFixture() {
        BOOST_REQUIRE(!tv.IsTurnedOn());
    }
};

BOOST_AUTO_TEST_SUITE(Controller_)

BOOST_FIXTURE_TEST_SUITE(WhenTVIsOff, WhenTVIsOffFixture)

BOOST_AUTO_TEST_CASE(on_Info_command_prints_that_tv_is_off) {
    RunMenuCommand("Info"s);
    ExpectOutput("TV is turned off\n"sv);
    BOOST_TEST(!tv.IsTurnedOn());
}

BOOST_AUTO_TEST_CASE(on_Info_command_prints_error_message_if_command_has_any_args) {
    RunMenuCommand("Info some extra args"s);
    BOOST_TEST(!tv.IsTurnedOn());
    ExpectExtraArgumentsErrorInOutput("Info"sv);
}

BOOST_AUTO_TEST_CASE(on_Info_command_ignores_trailing_spaces) {
    RunMenuCommand("Info  "s);
    ExpectOutput("TV is turned off\n"sv);
}

BOOST_AUTO_TEST_CASE(on_TurnOn_command_turns_TV_on) {
    RunMenuCommand("TurnOn"s);
    BOOST_TEST(tv.IsTurnedOn());
    ExpectEmptyOutput();
}

BOOST_AUTO_TEST_CASE(on_TurnOn_command_with_some_arguments_prints_error_message) {
    RunMenuCommand("TurnOn some args"s);
    BOOST_TEST(!tv.IsTurnedOn());
    ExpectExtraArgumentsErrorInOutput("TurnOn"sv);
}

BOOST_AUTO_TEST_CASE(on_SelectChannel_command_without_channel_prints_error) {
    RunMenuCommand("SelectChannel"s);
    ExpectOutput("Invalid channel\n"sv);
    BOOST_TEST(!tv.IsTurnedOn());
}

BOOST_AUTO_TEST_CASE(on_SelectChannel_command_with_invalid_channel_prints_error) {
    RunMenuCommand("SelectChannel abc"s);
    ExpectOutput("Invalid channel\n"sv);
    BOOST_TEST(!tv.IsTurnedOn());
}

BOOST_AUTO_TEST_CASE(on_SelectChannel_command_with_extra_args_prints_error) {
    RunMenuCommand("SelectChannel 5 extra"s);
    ExpectOutput("Error: the SelectChannel command requires exactly one argument\n"sv);
    BOOST_TEST(!tv.IsTurnedOn());
}

BOOST_AUTO_TEST_CASE(on_SelectChannel_command_does_nothing_when_tv_is_off) {
    RunMenuCommand("SelectChannel 5"s);
    ExpectOutput("TV is turned off\n"sv);
    BOOST_TEST(!tv.IsTurnedOn());
    BOOST_TEST(tv.GetChannel() == std::nullopt);
}

BOOST_AUTO_TEST_CASE(on_SelectPreviousChannel_command_does_nothing_when_tv_is_off) {
    RunMenuCommand("SelectPreviousChannel"s);
    ExpectOutput("TV is turned off\n"sv);
    BOOST_TEST(!tv.IsTurnedOn());
}

BOOST_AUTO_TEST_CASE(on_SelectPreviousChannel_command_with_args_prints_error) {
    RunMenuCommand("SelectPreviousChannel extra"s);
    ExpectExtraArgumentsErrorInOutput("SelectPreviousChannel"sv);
    BOOST_TEST(!tv.IsTurnedOn());
}

BOOST_AUTO_TEST_SUITE_END()

struct WhenTVIsOnFixture : ControllerFixture {
    WhenTVIsOnFixture() {
        tv.TurnOn();
    }
};

BOOST_FIXTURE_TEST_SUITE(WhenTVIsOn, WhenTVIsOnFixture)

BOOST_AUTO_TEST_CASE(on_TurnOff_command_turns_tv_off) {
    RunMenuCommand("TurnOff"s);
    BOOST_TEST(!tv.IsTurnedOn());
    ExpectEmptyOutput();
}

BOOST_AUTO_TEST_CASE(on_TurnOff_command_with_some_arguments_prints_error_message) {
    RunMenuCommand("TurnOff some args"s);
    BOOST_TEST(tv.IsTurnedOn());
    ExpectExtraArgumentsErrorInOutput("TurnOff"sv);
}

BOOST_AUTO_TEST_CASE(on_Info_prints_current_channel) {
    tv.SelectChannel(42);
    RunMenuCommand("Info"s);
    ExpectOutput("TV is turned on\nChannel number is 42\n"sv);
}

BOOST_AUTO_TEST_CASE(on_Info_prints_channel_1_initially) {
    RunMenuCommand("Info"s);
    ExpectOutput("TV is turned on\nChannel number is 1\n"sv);
}

BOOST_AUTO_TEST_CASE(on_SelectChannel_selects_valid_channel) {
    RunMenuCommand("SelectChannel 5"s);
    ExpectEmptyOutput();
    BOOST_TEST(tv.GetChannel() == 5);
}

BOOST_AUTO_TEST_CASE(on_SelectChannel_prints_error_for_out_of_range_channel) {
    RunMenuCommand("SelectChannel 100"s);
    ExpectOutput("Channel is out of range\n"sv);
    BOOST_TEST(tv.GetChannel() == 1);
}

BOOST_AUTO_TEST_CASE(on_SelectChannel_prints_error_for_invalid_channel) {
    RunMenuCommand("SelectChannel abc"s);
    ExpectOutput("Invalid channel\n"sv);
    BOOST_TEST(tv.GetChannel() == 1);
}

BOOST_AUTO_TEST_CASE(on_SelectChannel_prints_error_for_extra_args) {
    RunMenuCommand("SelectChannel 5 extra"s);
    ExpectOutput("Error: the SelectChannel command requires exactly one argument\n"sv);
    BOOST_TEST(tv.GetChannel() == 1);
}

BOOST_AUTO_TEST_CASE(on_SelectPreviousChannel_switches_to_previous) {
    tv.SelectChannel(5);
    tv.SelectChannel(10);
    RunMenuCommand("SelectPreviousChannel"s);
    ExpectEmptyOutput();
    BOOST_TEST(tv.GetChannel() == 5);
}

BOOST_AUTO_TEST_CASE(on_SelectPreviousChannel_without_previous_does_nothing) {
    RunMenuCommand("SelectPreviousChannel"s);
    ExpectEmptyOutput();
    BOOST_TEST(tv.GetChannel() == 1);
}

BOOST_AUTO_TEST_CASE(on_SelectPreviousChannel_with_args_prints_error) {
    RunMenuCommand("SelectPreviousChannel extra"s);
    ExpectExtraArgumentsErrorInOutput("SelectPreviousChannel"sv);
    BOOST_TEST(tv.GetChannel() == 1);
}

BOOST_AUTO_TEST_CASE(on_SelectChannel_then_SelectPreviousChannel_works) {
    RunMenuCommand("SelectChannel 5"s);
    RunMenuCommand("SelectChannel 10"s);
    RunMenuCommand("SelectPreviousChannel"s);
    BOOST_TEST(tv.GetChannel() == 5);
    RunMenuCommand("SelectPreviousChannel"s);
    BOOST_TEST(tv.GetChannel() == 10);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()