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
// Test that an instance can be constructed and has sensible initial state.
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
}
