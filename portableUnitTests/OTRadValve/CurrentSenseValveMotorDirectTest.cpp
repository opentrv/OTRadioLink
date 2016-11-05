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

#include <OTRadValve.h>


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
    EXPECT_EQ(35, minTicks);
    // Compute maximum sub-cycle time to start valve movement as for V0p2/REV7 20161018.
    const uint8_t gst_max = 255;
    const uint8_t minimumMotorRunupTicks = 4; // OTRadValve::ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks as at 20161018.
    const uint8_t sctAbsLimit = OTRadValve::CurrentSenseValveMotorDirect::computeSctAbsLimit(subcycleTicksRoundedDown_ms, gst_max, minimumMotorRunupTicks);
    EXPECT_EQ(230, sctAbsLimit);

    // Create calibration parameters with values that happen to work for V0p2/REV7.
    OTRadValve::CurrentSenseValveMotorDirect::CalibrationParameters cp;
    // Test the calculations with one plausible calibration data set.
    EXPECT_TRUE(cp.updateAndCompute(1601U, 1105U, minTicks)); // Must not fail...
    EXPECT_EQ(4, cp.getApproxPrecisionPC());
    EXPECT_EQ(25, cp.getTfotcSmall());
    EXPECT_EQ(17, cp.getTfctoSmall());
    // Check that a calibration instance can be reused correctly.
    const uint16_t tfo2 = 1803U;
    const uint16_t tfc2 = 1373U;
    EXPECT_TRUE(cp.updateAndCompute(tfo2, tfc2, minTicks)); // Must not fail...
    EXPECT_EQ(3, cp.getApproxPrecisionPC());
    EXPECT_EQ(28, cp.getTfotcSmall());
    EXPECT_EQ(21, cp.getTfctoSmall());

    // Check that computing position works...
    // Simple case: fully closed, no accumulated reverse ticks.
    volatile uint16_t ticksFromOpen, ticksReverse;
    ticksFromOpen = tfo2;
    ticksReverse = 0;
    EXPECT_EQ(0, cp.computePosition(ticksFromOpen, ticksReverse));
    EXPECT_EQ(tfo2, ticksFromOpen);
    EXPECT_EQ(0, ticksReverse);
    // Simple case: fully open, no accumulated reverse ticks.
    ticksFromOpen = 0;
    ticksReverse = 0;
    EXPECT_EQ(100, cp.computePosition(ticksFromOpen, ticksReverse));
    EXPECT_EQ(0, ticksFromOpen);
    EXPECT_EQ(0, ticksReverse);
    // Try at half-way mark, no reverse ticks.
    ticksFromOpen = tfo2 / 2;
    ticksReverse = 0;
    EXPECT_EQ(50, cp.computePosition(ticksFromOpen, ticksReverse));
    EXPECT_EQ(tfo2/2, ticksFromOpen);
    EXPECT_EQ(0, ticksReverse);
    // Try at half-way mark with just one reverse tick (nothing should change).
    ticksFromOpen = tfo2 / 2;
    ticksReverse = 1;
    EXPECT_EQ(50, cp.computePosition(ticksFromOpen, ticksReverse));
    EXPECT_EQ(tfo2/2, ticksFromOpen);
    EXPECT_EQ(1, ticksReverse);
    // Try at half-way mark with a big-enough block of reverse ticks to be significant.
    ticksFromOpen = tfo2 / 2;
    ticksReverse = cp.getTfctoSmall();
    EXPECT_EQ(51, cp.computePosition(ticksFromOpen, ticksReverse));
    EXPECT_EQ(tfo2/2 - cp.getTfotcSmall(), ticksFromOpen);
    EXPECT_EQ(0, ticksReverse);
  // DHD20151025: one set of actual measurements during calibration.
  //    ticksFromOpenToClosed: 1529
  //    ticksFromClosedToOpen: 1295
}

class DummyHardwareDriver : public OTRadValve::HardwareMotorDriverInterface
  {
  public:
    // Detect if end-stop is reached or motor current otherwise very high.
    bool currentHigh = false;
    virtual bool isCurrentHigh(OTRadValve::HardwareMotorDriverInterface::motor_drive mdir = motorDriveOpening) const override { return(currentHigh); }
    virtual void motorRun(uint8_t maxRunTicks, motor_drive dir, OTRadValve::HardwareMotorDriverInterfaceCallbackHandler &callback) override
      { }
  };

// Always claims to be at the start of a major cycle.
static uint8_t dummyGetSubCycleTime() { return(0); }

// Test that direct abstract motor drive logic is constructable and minimally sane..
//
// Adapted 2016/10/18 from test_VALVEMODEL.ino testCurrentSenseValveMotorDirect().
static void basics(OTRadValve::CurrentSenseValveMotorDirectBase *const csbp)
    {
    // POWER UP
    // Whitebox test of internal state: should be init.
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::init, csbp->getState());
    // Verify NOT marked as in normal run state immediately upon initialisation.
    EXPECT_TRUE(!csbp->isInNormalRunState());
    // Verify NOT marked as in error state immediately upon initialisation.
    EXPECT_TRUE(!csbp->isInErrorState());
    // Target % open must start off in a sensible state.
    EXPECT_GE(100, csbp->getTargetPC());
    // Current % open must start off in a sensible state.
    EXPECT_GE(100, csbp->getCurrentPC());
    }
TEST(CurrentSenseValveMotorDirect,basics)
{
    const uint8_t subcycleTicksRoundedDown_ms = 7; // For REV7: OTV0P2BASE::SUBCYCLE_TICK_MS_RD.
    const uint8_t gsct_max = 255; // For REV7: OTV0P2BASE::GSCT_MAX.
    const uint8_t minimumMotorRunupTicks = 4; // For REV7: OTRadValve::ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks.

    DummyHardwareDriver dhw0;
    OTRadValve::CurrentSenseValveMotorDirectBinaryOnly csvmdbo1(&dhw0, dummyGetSubCycleTime,
        OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::computeMinMotorDRTicks(subcycleTicksRoundedDown_ms),
        OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::computeSctAbsLimit(subcycleTicksRoundedDown_ms,
                                                                     gsct_max,
                                                                     minimumMotorRunupTicks));
    basics(&csvmdbo1);

    DummyHardwareDriver dhw1;
    OTRadValve::CurrentSenseValveMotorDirect csvmd1(&dhw1, dummyGetSubCycleTime,
        OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::computeMinMotorDRTicks(subcycleTicksRoundedDown_ms),
        OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::computeSctAbsLimit(subcycleTicksRoundedDown_ms,
                                                                     gsct_max,
                                                                     minimumMotorRunupTicks));
    basics(&csvmd1);
    // Until calibration has been successfully run, this should be in non-proportional mode.
    EXPECT_TRUE(csvmd1.inNonProprtionalMode());
    // Nothing passed in requires deferral of (re)calibration.
    EXPECT_FALSE(csvmd1.shouldDeferCalibration());
}

class SVL final : public OTV0P2BASE::SupplyVoltageLow
    {
    public:
      SVL() { setAllLowFlags(false); }
      void setAllLowFlags(const bool f) { isLow = f; isVeryLow = f; }
      virtual uint16_t get() const override { return(0); }
      virtual uint16_t read() override { return(0); }
    };
// State of ambient lighting.
static bool _isDark;
static bool isDark() { return(_isDark); }

// Test logic for potentially deferring (re)calibration.
TEST(CurrentSenseValveMotorDirect,calibrationDeferral)
{
    const uint8_t subcycleTicksRoundedDown_ms = 7; // For REV7: OTV0P2BASE::SUBCYCLE_TICK_MS_RD.
    const uint8_t gsct_max = 255; // For REV7: OTV0P2BASE::GSCT_MAX.
    const uint8_t minimumMotorRunupTicks = 4; // For REV7: OTRadValve::ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks.
    DummyHardwareDriver dhw;
    SVL svl;
    svl.setAllLowFlags(false);
    _isDark = false;
    OTRadValve::CurrentSenseValveMotorDirect csvmd1(&dhw, dummyGetSubCycleTime,
        OTRadValve::CurrentSenseValveMotorDirect::computeMinMotorDRTicks(subcycleTicksRoundedDown_ms),
        OTRadValve::CurrentSenseValveMotorDirect::computeSctAbsLimit(subcycleTicksRoundedDown_ms,
                                                                     gsct_max,
                                                                     minimumMotorRunupTicks),
        &svl,
        isDark);
    // Nothing yet requires deferral of (re)calibration.
    ASSERT_FALSE(csvmd1.shouldDeferCalibration());
    svl.setAllLowFlags(true);
    // Low supply voltage requires deferral of (re)calibration.
    ASSERT_TRUE(csvmd1.shouldDeferCalibration());
    svl.setAllLowFlags(false);
    ASSERT_FALSE(csvmd1.shouldDeferCalibration());
    _isDark = true;
    // Low light level requires deferral of (re)calibration.
    ASSERT_TRUE(csvmd1.shouldDeferCalibration());
    svl.setAllLowFlags(true);
    ASSERT_TRUE(csvmd1.shouldDeferCalibration());
    _isDark = false;
    svl.setAllLowFlags(false);
    // Nothing requires deferral of (re)calibration.
    ASSERT_FALSE(csvmd1.shouldDeferCalibration());
}

// This hits the end stops (current is high) immediately that the motor is driven.
class DummyHardwareDriverHitEndstop : public OTRadValve::HardwareMotorDriverInterface
  {
    // Detect if end-stop is reached or motor current otherwise very high.
    bool currentHigh = false;
  public:
    virtual bool isCurrentHigh(OTRadValve::HardwareMotorDriverInterface::motor_drive mdir = motorDriveOpening) const override { return(currentHigh); }
    virtual void motorRun(uint8_t maxRunTicks, motor_drive dir, OTRadValve::HardwareMotorDriverInterfaceCallbackHandler &callback) override
      {
      currentHigh = (OTRadValve::HardwareMotorDriverInterface::motorOff != dir);
      callback.signalHittingEndStop(true);
      }
  };

// Test initial state walk-through without and with calibration deferral.
static void initStateWalkthrough(OTRadValve::CurrentSenseValveMotorDirectBase *const csv, const bool batteryLow)
    {
    // Whitebox test of internal state: should be init.
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirectBase::init, csv->getState());
//    // Check deferral of (re)calibration.
//    EXPECT_EQ(batteryLow, csv->shouldDeferCalibration());
    // Verify NOT marked as in normal run state immediately upon initialisation.
    EXPECT_TRUE(!csv->isInNormalRunState());
    // Verify NOT marked as in error state immediately upon initialisation.
    EXPECT_TRUE(!csv->isInErrorState());
    csv->poll();
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::initWaiting, csv->getState());
    // Within a reasonable time to (10s of seconds) should move to new state, but not instantly.
    csv->poll();
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::initWaiting, csv->getState());
    csv->poll();
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::initWaiting, csv->getState());
    for(int i = 100; --i > 0 && OTRadValve::CurrentSenseValveMotorDirect::initWaiting == csv->getState(); ) { csv->poll(); }
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::valvePinWithdrawing, csv->getState());
    // Fake hardware hits end-stop immediate, so leaves 'withdrawing' state.
    csv->poll();
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::valvePinWithdrawn, csv->getState());
    EXPECT_EQ(100, csv->getCurrentPC()) << "valve must now be fully open";
    // Waiting for value to be signalled that it has been fitted...
    csv->poll();
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::valvePinWithdrawn, csv->getState());
    csv->signalValveFitted();
    csv->poll();
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::valveCalibrating, csv->getState());
    csv->poll();
//    // Check deferral of (re)calibration.
//    EXPECT_EQ(batteryLow, csv->shouldDeferCalibration());
    // Valve should now start calibrating, but calibration is skipped with low battery...
    EXPECT_EQ((batteryLow || csv->isNonProprtionalOnly())
        ? OTRadValve::CurrentSenseValveMotorDirect::valveNormal :
          OTRadValve::CurrentSenseValveMotorDirect::valveCalibrating, csv->getState()) << (batteryLow ? "low" : "normal");
    }
TEST(CurrentSenseValveMotorDirect,initStateWalkthrough)
{
    const bool verbose = false;

    const uint8_t subcycleTicksRoundedDown_ms = 7; // For REV7: OTV0P2BASE::SUBCYCLE_TICK_MS_RD.
    const uint8_t gsct_max = 255; // For REV7: OTV0P2BASE::GSCT_MAX.
    const uint8_t minimumMotorRunupTicks = 4; // For REV7: OTRadValve::ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks.
    for(int s = 0; s < 2; ++s)
        {
        const bool low = (1 == s); // Is battery low...
        if(verbose) { printf("Battery low %s...\n", low ? "true" : "false"); }
        SVL svl;
        svl.setAllLowFlags(low);

        // Test non-proportional impl.
        DummyHardwareDriverHitEndstop dhw0;
        OTRadValve::CurrentSenseValveMotorDirectBinaryOnly csvmdbo1(&dhw0, dummyGetSubCycleTime,
            OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::computeMinMotorDRTicks(subcycleTicksRoundedDown_ms),
            OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::computeSctAbsLimit(subcycleTicksRoundedDown_ms,
                                                                         gsct_max,
                                                                         minimumMotorRunupTicks),
            &svl,
            [](){return(false);});
        initStateWalkthrough(&csvmdbo1, low);

        // Test full impl.
        DummyHardwareDriverHitEndstop dhw1;
        OTRadValve::CurrentSenseValveMotorDirect csvmd1(&dhw1, dummyGetSubCycleTime,
            OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::computeMinMotorDRTicks(subcycleTicksRoundedDown_ms),
            OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::computeSctAbsLimit(subcycleTicksRoundedDown_ms,
                                                                         gsct_max,
                                                                         minimumMotorRunupTicks),
            &svl,
            [](){return(false);});
        // Check deferral of (re)calibration.
        EXPECT_EQ(low, csvmd1.shouldDeferCalibration());
        initStateWalkthrough(&csvmd1, low);
        // Check deferral of (re)calibration.
        EXPECT_EQ(low, csvmd1.shouldDeferCalibration());
        }
}
