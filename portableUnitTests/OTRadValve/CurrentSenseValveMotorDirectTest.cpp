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
 * OTRadValve CurrentSenseValveMotorDirect tests.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <OTV0P2BASE_QuickPRNG.h>

#include "OTRadValve_Parameters.h"
#include "OTRadValve_ValveMotorDirectV1.h"


// Test calibration calculations in CurrentSenseValveMotorDirect.
// Also check some of the use of those calculations.
// In particular test the logic in ModelledRadValveState for starting from extreme positions.
//
// Adapted 2016/10/18 from test_VALVEMODEL.ino testCSVMDC().
TEST(CurrentSenseValveMotorDirect,CSVMDC)
{
    OTRadValve::CurrentSenseValveMotorDirect::CalibrationParameters cp;
//    volatile uint16_t ticksFromOpen, ticksReverse;
//    // Test the calculations with one plausible calibration data set.
//    ASSERT_TRUE(cp.updateAndCompute(1601U, 1105U)); // Must not fail...
//    ASSERT_EQ(4, cp.getApproxPrecisionPC());
//    ASSERT_EQ(25, cp.getTfotcSmall());
//    ASSERT_EQ(17, cp.getTfctoSmall());
//    // Check that a calibration instance can be reused correctly.
//    const uint16_t tfo2 = 1803U;
//    const uint16_t tfc2 = 1373U;
//    ASSERT_TRUE(cp.updateAndCompute(tfo2, tfc2)); // Must not fail...
//    ASSERT_EQ(3, cp.getApproxPrecisionPC());
//    ASSERT_EQ(28, cp.getTfotcSmall());
//    ASSERT_EQ(21, cp.getTfctoSmall());
//    // Check that computing position works...
//    // Simple case: fully closed, no accumulated reverse ticks.
//    ticksFromOpen = tfo2;
//    ticksReverse = 0;
//    ASSERT_EQ(0, cp.computePosition(ticksFromOpen, ticksReverse));
//    ASSERT_EQ(tfo2, ticksFromOpen);
//    ASSERT_EQ(0, ticksReverse);
//    // Simple case: fully open, no accumulated reverse ticks.
//    ticksFromOpen = 0;
//    ticksReverse = 0;
//    ASSERT_EQ(100, cp.computePosition(ticksFromOpen, ticksReverse));
//    ASSERT_EQ(0, ticksFromOpen);
//    ASSERT_EQ(0, ticksReverse);
//    // Try at half-way mark, no reverse ticks.
//    ticksFromOpen = tfo2 / 2;
//    ticksReverse = 0;
//    ASSERT_EQ(50, cp.computePosition(ticksFromOpen, ticksReverse));
//    ASSERT_EQ(tfo2/2, ticksFromOpen);
//    ASSERT_EQ(0, ticksReverse);
//    // Try at half-way mark with just one reverse tick (nothing should change).
//    ticksFromOpen = tfo2 / 2;
//    ticksReverse = 1;
//    ASSERT_EQ(50, cp.computePosition(ticksFromOpen, ticksReverse));
//    ASSERT_EQ(tfo2/2, ticksFromOpen);
//    ASSERT_EQ(1, ticksReverse);
//    // Try at half-way mark with a big-enough block of reverse ticks to be significant.
//    ticksFromOpen = tfo2 / 2;
//    ticksReverse = cp.getTfctoSmall();
//    ASSERT_EQ(51, cp.computePosition(ticksFromOpen, ticksReverse));
//    ASSERT_EQ(tfo2/2 - cp.getTfotcSmall(), ticksFromOpen);
//    ASSERT_EQ(0, ticksReverse);
//  // DHD20151025: one set of actual measurements during calibration.
//  //    ticksFromOpenToClosed: 1529
//  //    ticksFromClosedToOpen: 1295
}
