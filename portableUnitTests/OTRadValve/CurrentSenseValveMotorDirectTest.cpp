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

#include "OTRadValve_Parameters.h"
#include "OTRadValve_ValveMotorDirectV1.h"


class DummyHardwareDriver : public OTRadValve::HardwareMotorDriverInterface
  {
  public:
    // Detect if end-stop is reached or motor current otherwise very high.
    virtual bool isCurrentHigh(OTRadValve::HardwareMotorDriverInterface::motor_drive mdir = motorDriveOpening) const { return(currentHigh); }
  public:
    DummyHardwareDriver() : currentHigh(false) { }
    virtual void motorRun(uint8_t maxRunTicks, motor_drive dir, OTRadValve::HardwareMotorDriverInterfaceCallbackHandler &callback)
      { }
    // isCurrentHigh() returns this value.
    bool currentHigh;
  };

static uint8_t dummyGetSubCycleTime() { return(0); }

// Test that direct abstract motor drive logic is sane.
//
// Adapted 2016/10/18 from test_VALVEMODEL.ino testCurrentSenseValveMotorDirect().
TEST(CurrentSenseValveMotorDirect,bascis)
{
    const uint8_t subcycleTicksRoundedDown_ms = 7; // For REV7: OTV0P2BASE::SUBCYCLE_TICK_MS_RD.
    const uint8_t gsct_max = 255; // For REV7: OTV0P2BASE::GSCT_MAX.
    const uint8_t minimumMotorRunupTicks = 4; // For REV7: OTRadValve::ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks.
    DummyHardwareDriver dhw;
    OTRadValve::CurrentSenseValveMotorDirect csvmd1(&dhw, dummyGetSubCycleTime,
        OTRadValve::CurrentSenseValveMotorDirect::computeMinMotorDRTicks(subcycleTicksRoundedDown_ms),
        OTRadValve::CurrentSenseValveMotorDirect::computeSctAbsLimit(subcycleTicksRoundedDown_ms,
                                                                     gsct_max,
                                                                     minimumMotorRunupTicks));

    // POWER UP
    // Whitebox test of internal state: should be init.
    ASSERT_EQ(OTRadValve::CurrentSenseValveMotorDirect::init, csvmd1.getState());
    // Verify NOT marked as in normal run state immediately upon initialisation.
    ASSERT_TRUE(!csvmd1.isInNormalRunState());
    // Verify NOT marked as in error state immediately upon initialisation.
    ASSERT_TRUE(!csvmd1.isInErrorState());
    // Target % open must start off in a sensible state; fully-closed is good.
    ASSERT_EQ(0, csvmd1.getTargetPC());

//    // FIRST POLL(S) AFTER POWER_UP; RETRACTING THE PIN.
//    csvmd1.poll();
    // DHD20160123: now waits randomised number of ticks before starting to withdraw.
  //  // Whitebox test of internal state: should be valvePinWithdrawing.
  //  AssertIsEqual(OTRadValve::CurrentSenseValveMotorDirect::valvePinWithdrawing, csvmd1.getState());
  //  // More polls shouldn't make any difference initially.
  //  csvmd1.poll();
  //  // Whitebox test of internal state: should be valvePinWithdrawing.
  //  AssertIsEqual(OTRadValve::CurrentSenseValveMotorDirect::valvePinWithdrawing, csvmd1.getState());
  //  csvmd1.poll();
  //  // Whitebox test of internal state: should be valvePinWithdrawing.
  //  AssertIsEqual(OTRadValve::CurrentSenseValveMotorDirect::valvePinWithdrawing, csvmd1.getState());
  //  // Simulate hitting end-stop (high current).
  //  dhw.currentHigh = true;
  //  AssertIsTrue(dhw.isCurrentHigh());
  //  csvmd1.poll();
  //  // Whitebox test of internal state: should be valvePinWithdrawn.
  //  AssertIsEqual(CurrentSenseValveMotorDirect::valvePinWithdrawn, csvmd1.getState());
  //  dhw.currentHigh = false;
    // TODO
}


// Test calibration calculations in CurrentSenseValveMotorDirect for REV7/DORM1/TRV1 board.
// Also check some of the use of those calculations.
// In particular test the logic in ModelledRadValveState for starting from extreme positions.
//
// Adapted 2016/10/18 from test_VALVEMODEL.ino testCSVMDC().
TEST(CurrentSenseValveMotorDirect,REV7CSVMDC)
{
    // Compute minimum dead-reckoning ticks as for V0p2/REV7 20161018.
    const uint8_t subcycleTicksRoundedDown_ms = 7;
    const uint8_t minTicks = OTRadValve::CurrentSenseValveMotorDirect::computeMinMotorDRTicks(subcycleTicksRoundedDown_ms);
    ASSERT_EQ(35, minTicks);
    // Compute maximum sub-cycle time to start valve movement as for V0p2/REV7 20161018.
    const uint8_t gst_max = 255;
    const uint8_t minimumMotorRunupTicks = 4; // OTRadValve::ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks as at 20161018.
    const uint8_t sctAbsLimit = OTRadValve::CurrentSenseValveMotorDirect::computeSctAbsLimit(subcycleTicksRoundedDown_ms, gst_max, minimumMotorRunupTicks);
    ASSERT_EQ(230, sctAbsLimit);

    // Create calibration parameters with values that happen to work for V0p2/REV7.
    OTRadValve::CurrentSenseValveMotorDirect::CalibrationParameters cp(minTicks);
    // Test the calculations with one plausible calibration data set.
    ASSERT_TRUE(cp.updateAndCompute(1601U, 1105U)); // Must not fail...
    ASSERT_EQ(4, cp.getApproxPrecisionPC());
    ASSERT_EQ(25, cp.getTfotcSmall());
    ASSERT_EQ(17, cp.getTfctoSmall());
    // Check that a calibration instance can be reused correctly.
    const uint16_t tfo2 = 1803U;
    const uint16_t tfc2 = 1373U;
    ASSERT_TRUE(cp.updateAndCompute(tfo2, tfc2)); // Must not fail...
    ASSERT_EQ(3, cp.getApproxPrecisionPC());
    ASSERT_EQ(28, cp.getTfotcSmall());
    ASSERT_EQ(21, cp.getTfctoSmall());

    // Check that computing position works...
    // Simple case: fully closed, no accumulated reverse ticks.
    volatile uint16_t ticksFromOpen, ticksReverse;
    ticksFromOpen = tfo2;
    ticksReverse = 0;
    ASSERT_EQ(0, cp.computePosition(ticksFromOpen, ticksReverse));
    ASSERT_EQ(tfo2, ticksFromOpen);
    ASSERT_EQ(0, ticksReverse);
    // Simple case: fully open, no accumulated reverse ticks.
    ticksFromOpen = 0;
    ticksReverse = 0;
    ASSERT_EQ(100, cp.computePosition(ticksFromOpen, ticksReverse));
    ASSERT_EQ(0, ticksFromOpen);
    ASSERT_EQ(0, ticksReverse);
    // Try at half-way mark, no reverse ticks.
    ticksFromOpen = tfo2 / 2;
    ticksReverse = 0;
    ASSERT_EQ(50, cp.computePosition(ticksFromOpen, ticksReverse));
    ASSERT_EQ(tfo2/2, ticksFromOpen);
    ASSERT_EQ(0, ticksReverse);
    // Try at half-way mark with just one reverse tick (nothing should change).
    ticksFromOpen = tfo2 / 2;
    ticksReverse = 1;
    ASSERT_EQ(50, cp.computePosition(ticksFromOpen, ticksReverse));
    ASSERT_EQ(tfo2/2, ticksFromOpen);
    ASSERT_EQ(1, ticksReverse);
    // Try at half-way mark with a big-enough block of reverse ticks to be significant.
    ticksFromOpen = tfo2 / 2;
    ticksReverse = cp.getTfctoSmall();
    ASSERT_EQ(51, cp.computePosition(ticksFromOpen, ticksReverse));
    ASSERT_EQ(tfo2/2 - cp.getTfotcSmall(), ticksFromOpen);
    ASSERT_EQ(0, ticksReverse);
  // DHD20151025: one set of actual measurements during calibration.
  //    ticksFromOpenToClosed: 1529
  //    ticksFromClosedToOpen: 1295
}
