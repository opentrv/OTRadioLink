/*
The OpenTRV project licenses this file to you
under the Apache Licence, Version 2.0 (the "Licence");
you may not use this file except in compliance
with the Licence. You may obtain a copy of the Licence at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the Licence is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied. See the Licence for the
specific language governing permissions and limitations
under the Licence.

Author(s) / Copyright (s): Deniz Erbilgin 2017
*/

#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>

#include <OTRadValve.h>

// Max stack usage in bytes
namespace BoilerDriverTest
{
    static constexpr unsigned int maxStackProcessCallsForHeat = 100;
    static constexpr unsigned int maxStackRemoteCallForHeatRX = 100;
}

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

#if 1  // Stack usage checks
// Measure stack usage of remoteCallForHeatRX.
// (20170609): 80 bytes
TEST(BoilerDriverTest, remoteCallForHeatRXStackUsage) {
    // Instantiate boiler driver
    constexpr uint8_t heatCallPin = 0; // unused in unit tests.
    OTRadValve::BoilerCallForHeat<heatCallPin> bh;

    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();

    bh.remoteCallForHeatRX(0, 100, 1);
    std::cout << baseStack - OTV0P2BASE::MemoryChecks::getMinSP() << "\n";
    EXPECT_GT(BoilerDriverTest::maxStackRemoteCallForHeatRX, baseStack - OTV0P2BASE::MemoryChecks::getMinSP());
}

// Measure stack usage of remoteCallForHeatRX.
// (20170609): 64 bytes
TEST(BoilerDriverTest, processCallsForHeatStackUsage) {
    // Instantiate boiler driver
    constexpr uint8_t heatCallPin = 0; // unused in unit tests.
    constexpr bool inHubMode = true;

    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();

    OTRadValve::BoilerCallForHeat<heatCallPin> bh;
    bh.processCallsForHeat(false, inHubMode);
    std::cout << baseStack - OTV0P2BASE::MemoryChecks::getMinSP() << "\n";
    EXPECT_GT(BoilerDriverTest::maxStackProcessCallsForHeat, baseStack - OTV0P2BASE::MemoryChecks::getMinSP());
}
#endif
