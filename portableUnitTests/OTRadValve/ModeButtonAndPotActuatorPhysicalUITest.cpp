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

Author(s) / Copyright (s): Damon Hart-Davis 2016
*/

/*
 * OTRadValve ModeButtonAndPotActuatorPhysicalUI tests.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <OTV0P2BASE_QuickPRNG.h>

#include "OTRadValve_ActuatorPhysicalUI.h"


// Test for general sanity of ModeButtonAndPotActuatorPhysicalUI.
// Test that an instance can be constructed.
TEST(ModeButtonAndPotActuatorPhysicalUI,basics)
{
//    // If true then be more verbose.
//    const static bool verbose = false;

    OTRadValve::ValveMode vm;
    OTRadValve::TempControlBase tc;
    OTRadValve::NULLRadValve rv;
    OTV0P2BASE::PseudoSensorOccupancyTracker occupancy;
    OTV0P2BASE::SensorAmbientLightMock ambLight;
    OTRadValve::ModeButtonAndPotActuatorPhysicalUI mbpUI(
          &vm,
          &tc,
          &rv,
          &occupancy,
          &ambLight,
          NULL,
          NULL,
          [](){}, [](){}, NULL);
    ASSERT_FALSE(mbpUI.recentUIControlUse());
    ASSERT_FALSE(mbpUI.veryRecentUIControlUse());
    ASSERT_FALSE(occupancy.isLikelyOccupied());
    ASSERT_FALSE(occupancy.reportedRecently());
}


// Test for general sanity of ModeButtonAndPotActuatorPhysicalUI.
// Test that an instance can be constructed and has sensible initial state.
namespace startState
  {
constexpr uint8_t usefulScale = 47; // hiEndStop - loEndStop + 1;
constexpr uint8_t loEndStop = 200; // Arbitrary.
constexpr uint8_t hiEndStop = loEndStop + usefulScale - 1;

OTV0P2BASE::SensorTemperaturePotMock tp(loEndStop, hiEndStop);
// Parameters as for REV7/DORM1/TRV1 at 2016/10/27.
typedef OTRadValve::ValveControlParameters<
    6,  // Target FROST temperature for ECO bias.
    14, // Target FROST temperature for Comfort bias.
    17, // Target WARM temperature for ECO bias.
    21  // Target WARM temperature for Comfort bias.
    > TRV1ValveControlParameters;
OTRadValve::TempControlTempPot <decltype(tp), &tp, TRV1ValveControlParameters> tctp0;
  }
TEST(ModeButtonAndPotActuatorPhysicalUI,startState)
{
    // Simulate system boot with dial in low/mid/high positions.
    const uint8_t potPositions[] = { 0, startState::loEndStop + startState::usefulScale/2, 255 };
    for(unsigned i = 0; i < sizeof(potPositions); ++i)
        {
        const uint8_t pp = potPositions[i];
        startState::tp.set(pp);
        OTRadValve::ValveMode vm;
        ASSERT_FALSE(vm.inWarmMode());
        OTRadValve::TempControlBase tc;
        OTRadValve::NULLRadValve rv;
        OTV0P2BASE::PseudoSensorOccupancyTracker occupancy;
        occupancy.read();
        OTV0P2BASE::SensorAmbientLightMock ambLight;
        ambLight.read();
        OTRadValve::ModeButtonAndPotActuatorPhysicalUI mbpUI(
              &vm,
              &startState::tctp0,
              &rv,
              &occupancy,
              &ambLight,
              &startState::tp,
              NULL,
              [](){}, [](){}, NULL);
        ASSERT_FALSE(mbpUI.recentUIControlUse());
        ASSERT_FALSE(mbpUI.veryRecentUIControlUse());
        for(int i = 10; --i > 0; ) { mbpUI.read(); } // Spin a few ticks to warm up...
        EXPECT_EQ((0 != pp), vm.inWarmMode()) << "Should only boot to FROST mode if dial is in FROST position";
        ASSERT_FALSE(occupancy.isLikelyOccupied()) << "Forcing WARM mode should not trigger any occupancy indication";
        ASSERT_FALSE(occupancy.reportedRecently()) << "Forcing WARM mode should not trigger any occupancy indication";
    }
}
