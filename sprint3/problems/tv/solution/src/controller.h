#pragma once
#include <cassert>
#include <charconv>
#include <string>
#include <string_view>

#include "menu.h"
#include "tv.h"

class Controller {
public:
    Controller(TV& tv, Menu& menu)
        : tv_{ tv }
        , menu_{ menu } {
        using namespace std::literals;
        menu_.AddAction(std::string{ INFO_COMMAND }, {}, "Prints info about the TV"s,
            [this](auto& input, auto& output) {
                return ShowInfo(input, output);
            });
        menu_.AddAction(std::string{ TURN_ON_COMMAND }, {}, "Turns on the TV"s,
            [this](auto& input, auto& output) {
                return TurnOn(input, output);
            });
        menu_.AddAction(std::string{ TURN_OFF_COMMAND }, {}, "Turns off the TV"s,
            [this](auto& input, auto& output) {
                return TurnOff(input, output);
            });
        menu_.AddAction(std::string{ SELECT_CHANNEL_COMMAND }, "CHANNEL"s,
            "Selects the specified channel"s, [this](auto& input, auto& output) {
                return SelectChannel(input, output);
            });
        menu_.AddAction(std::string{ SELECT_PREVIOUS_CHANNEL_COMMAND }, {},
            "Selects the previously selected channel"s,
            [this](auto& input, auto& output) {
                return SelectPreviousChannel(input, output);
            });
    }

private:
    [[nodiscard]] bool ShowInfo(std::istream& input, std::ostream& output) const {
        using namespace std::literals;

        if (EnsureNoArgsInInput(INFO_COMMAND, input, output)) {
            if (tv_.IsTurnedOn()) {
                output << "TV is turned on"sv << std::endl;
                output << "Channel number is "sv << *tv_.GetChannel() << std::endl;
            }
            else {
                output << "TV is turned off"sv << std::endl;
            }
        }

        return true;
    }

    [[nodiscard]] bool TurnOn(std::istream& input, std::ostream& output) const {
        using namespace std::literals;

        if (EnsureNoArgsInInput(TURN_ON_COMMAND, input, output)) {
            tv_.TurnOn();
        }
        return true;
    }

    [[nodiscard]] bool TurnOff(std::istream& input, std::ostream& output) const {
        using namespace std::literals;

        if (EnsureNoArgsInInput(TURN_OFF_COMMAND, input, output)) {
            tv_.TurnOff();
        }
        return true;
    }

    [[nodiscard]] bool SelectChannel(std::istream& input, std::ostream& output) const {
        using namespace std::literals;

        std::string channel_str;
        if (!(input >> channel_str)) {
            output << "Invalid channel"sv << std::endl;
            return true;
        }

        if (std::string extra; input >> extra) {
            output << "Error: the " << SELECT_CHANNEL_COMMAND
                << " command requires exactly one argument"sv << std::endl;
            return true;
        }

        int channel;
        auto [ptr, ec] = std::from_chars(channel_str.data(),
            channel_str.data() + channel_str.size(),
            channel);

        if (ec != std::errc() || ptr != channel_str.data() + channel_str.size()) {
            output << "Invalid channel"sv << std::endl;
            return true;
        }

        try {
            tv_.SelectChannel(channel);
        }
        catch (const std::out_of_range&) {
            output << "Channel is out of range"sv << std::endl;
        }
        catch (const std::logic_error&) {
            output << "TV is turned off"sv << std::endl;
        }

        return true;
    }

    [[nodiscard]] bool SelectPreviousChannel(std::istream& input, std::ostream& output) const {
        using namespace std::literals;

        if (EnsureNoArgsInInput(SELECT_PREVIOUS_CHANNEL_COMMAND, input, output)) {
            try {
                tv_.SelectLastViewedChannel();
            }
            catch (const std::logic_error&) {
                output << "TV is turned off"sv << std::endl;
            }
        }
        return true;
    }

    [[nodiscard]] bool EnsureNoArgsInInput(std::string_view command, std::istream& input,
        std::ostream& output) const {
        using namespace std::literals;
        assert(input);
        if (std::string data; input >> data) {
            output << "Error: the " << command << " command does not require any arguments"sv
                << std::endl;
            return false;
        }
        return true;
    }

    constexpr static std::string_view INFO_COMMAND = "Info";
    constexpr static std::string_view TURN_ON_COMMAND = "TurnOn";
    constexpr static std::string_view TURN_OFF_COMMAND = "TurnOff";
    constexpr static std::string_view SELECT_CHANNEL_COMMAND = "SelectChannel";
    constexpr static std::string_view SELECT_PREVIOUS_CHANNEL_COMMAND = "SelectPreviousChannel";

    TV& tv_;
    Menu& menu_;
};