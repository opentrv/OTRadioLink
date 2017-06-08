/*
 * BoilerDriverTest.cpp
 *
 *  Created on: 8 Jun 2017
 *      Author: denzo
 */

#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>

#include <OTRadValve.h>


// Test for general sanity of BoilerCallForHeat
TEST(BoilerDriverTest, basicBoilerHub)
{
    constexpr uint8_t heatCallPin = 0; // unused in unit tests.
    OTRadValve::BoilerCallForHeat<heatCallPin> bh;
    EXPECT_FALSE(bh.isBoilerOn());  // Should initialise to off
}

// Test that call for heat triggers boiler when in hub mode.
TEST(BoilerDriverTest, boilerHubModeHeatCall)
{
    constexpr uint8_t heatCallPin = 0; // unused in unit tests.
    constexpr bool inHubMode = true;
    OTRadValve::BoilerCallForHeat<heatCallPin> bh;
    // Trick boiler hub into believeing 10 minutes have passed.
    for(auto i = 0; i < 10; ++i) {
        bh.processCallsForHeat(true, inHubMode);
    }
    EXPECT_FALSE(bh.isBoilerOn());  // Should initialise to off
    bh.remoteCallForHeatRX(0, 100, 1);
    EXPECT_FALSE(bh.isBoilerOn());  // Should still be off, until heat call processed.
    bh.processCallsForHeat(false, inHubMode);
    EXPECT_TRUE(bh.isBoilerOn());
}

// Test that call for heat triggers boiler when in hub mode.
TEST(BoilerDriverTest, boilerNotHubModeHeatCall)
{
    constexpr uint8_t heatCallPin = 0; // unused in unit tests.
    constexpr bool inHubMode = false;
    OTRadValve::BoilerCallForHeat<heatCallPin> bh;
    // Trick boiler hub into believeing 10 minutes have passed.
    for(auto i = 0; i < 10; ++i) {
        bh.processCallsForHeat(true, inHubMode);
    }
    EXPECT_FALSE(bh.isBoilerOn());  // Should initialise to off
    bh.remoteCallForHeatRX(0, 100, 1);
    EXPECT_FALSE(bh.isBoilerOn());  // Should still be off, until heat call processed.
    bh.processCallsForHeat(0, inHubMode);
    EXPECT_FALSE(bh.isBoilerOn());
}

// Test that boiler hub is only triggered when boilerNoCallM goes above DEFAULT_MIN_BOILER_ON_MINS (5).
TEST(BoilerDriverTest, boilerHubModeStartup)
{
    constexpr uint8_t heatCallPin = 0; // unused in unit tests.
    constexpr bool inHubMode = true;

    OTRadValve::BoilerCallForHeat<heatCallPin> bh;
    EXPECT_FALSE(bh.isBoilerOn());  // Should initialise to off
    // Trick boiler hub into believeing 10 minutes have passed.
    for(auto i = 0; i < 6; ++i) {
        bh.remoteCallForHeatRX(0, 100, 1);
        bh.processCallsForHeat(true, inHubMode);
        EXPECT_FALSE(bh.isBoilerOn());  // Should still be off, until heat call processed.
    }
    bh.remoteCallForHeatRX(0, 100, 1);
    bh.processCallsForHeat(true, inHubMode);
    EXPECT_TRUE(bh.isBoilerOn());
}

// Test that boilerNoCallM is not advanced when second0 is false.
TEST(BoilerDriverTest, boilerHubModeIncBoilerNoCallM)
{
    constexpr uint8_t heatCallPin = 0; // unused in unit tests.
    constexpr bool inHubMode = true;

    OTRadValve::BoilerCallForHeat<heatCallPin> bh;
    EXPECT_FALSE(bh.isBoilerOn());  // Should initialise to off
    // Check clock is not advanced when second0 is false
    for(auto i = 0; i < 1000; ++i) {
        bh.remoteCallForHeatRX(0, 100, 1);
        bh.processCallsForHeat(false, inHubMode);
        EXPECT_FALSE(bh.isBoilerOn());  // Should still be off, until heat call processed.
    }
    for(auto i = 0; i < 10; ++i) {
        bh.remoteCallForHeatRX(0, 100, 1);
        bh.processCallsForHeat(true, inHubMode);
    }
    EXPECT_TRUE(bh.isBoilerOn());
}
