#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "../src/tv.h"

class TVByDefault : public testing::Test {
protected:
    TV tv_;
};

TEST_F(TVByDefault, IsOff) {
    EXPECT_FALSE(tv_.IsTurnedOn());
}

TEST_F(TVByDefault, DoesntShowAChannelWhenItIsOff) {
    EXPECT_FALSE(tv_.GetChannel().has_value());
}

TEST_F(TVByDefault, CantSelectAnyChannel) {
    EXPECT_THROW(tv_.SelectChannel(10), std::logic_error);
    EXPECT_EQ(tv_.GetChannel(), std::nullopt);
}

TEST_F(TVByDefault, CantSelectPreviousChannel) {
    EXPECT_THROW(tv_.SelectLastViewedChannel(), std::logic_error);
}

TEST_F(TVByDefault, TurnOffDoesNothing) {
    tv_.TurnOff();
    EXPECT_FALSE(tv_.IsTurnedOn());
    EXPECT_EQ(tv_.GetChannel(), std::nullopt);
}

TEST_F(TVByDefault, TurnOnTurnsTVOn) {
    tv_.TurnOn();
    EXPECT_TRUE(tv_.IsTurnedOn());
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
}

class TurnedOnTV : public TVByDefault {
protected:
    void SetUp() override {
        tv_.TurnOn();
    }
};

TEST_F(TurnedOnTV, ShowsChannel1) {
    EXPECT_TRUE(tv_.IsTurnedOn());
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
}

TEST_F(TurnedOnTV, AfterTurningOffTurnsOffAndDoesntShowAnyChannel) {
    tv_.TurnOff();
    EXPECT_FALSE(tv_.IsTurnedOn());
    EXPECT_EQ(tv_.GetChannel(), std::nullopt);
}

TEST_F(TurnedOnTV, CanSelectChannelFrom1To99) {
    for (int channel = TV::MIN_CHANNEL; channel <= TV::MAX_CHANNEL; ++channel) {
        tv_.SelectChannel(channel);
        EXPECT_THAT(tv_.GetChannel(), testing::Optional(channel));
    }
}

TEST_F(TurnedOnTV, CantSelectChannelOutOfRange) {
    EXPECT_THROW(tv_.SelectChannel(TV::MIN_CHANNEL - 1), std::out_of_range);
    EXPECT_THROW(tv_.SelectChannel(TV::MAX_CHANNEL + 1), std::out_of_range);
    EXPECT_THROW(tv_.SelectChannel(0), std::out_of_range);
    EXPECT_THROW(tv_.SelectChannel(100), std::out_of_range);
}

TEST_F(TurnedOnTV, SelectingSameChannelDoesNothing) {
    tv_.SelectChannel(5);
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(5));
    tv_.SelectChannel(5);
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(5));
    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(5));
}

TEST_F(TurnedOnTV, SelectLastViewedChannelSwitchesBetweenTwoChannels) {
    tv_.SelectChannel(5);
    tv_.SelectChannel(10);
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(10));

    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(5));

    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(10));

    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(5));
}

TEST_F(TurnedOnTV, SelectLastViewedChannelWithoutPreviousDoesNothing) {
    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));

    tv_.SelectChannel(5);
    tv_.SelectLastViewedChannel();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
}

TEST_F(TurnedOnTV, TurnOffAndOnResetsToChannel1) {
    tv_.SelectChannel(5);
    tv_.TurnOff();
    tv_.TurnOn();
    EXPECT_THAT(tv_.GetChannel(), testing::Optional(1));
}