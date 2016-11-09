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


// Test basic calibration calculation error handling in CalibrationParameters, eg with bad inputs.
TEST(CurrentSenseValveMotorDirect,CalibrationParametersError)
{
    // Check that default calibration state is 'error', ie 'cannot run proportional'.
    OTRadValve::CurrentSenseValveMotorDirect::CalibrationParameters cp0;
    ASSERT_TRUE(cp0.cannotRunProportional());

    // Test that we cannot encounter divide-by-zero and other horrors with bad input, eg from a stuck actuator.
    OTRadValve::CurrentSenseValveMotorDirect::CalibrationParameters cp;
    // Test the calculations with one plausible calibration data set.
    EXPECT_FALSE(cp.updateAndCompute(1000U, 1000U, 0)); // Must fail (illegal minTicks).
    EXPECT_TRUE(cp.cannotRunProportional());
    // Test the calculations with one plausible calibration data set to check that error state is not sticky.
    EXPECT_TRUE(cp.updateAndCompute(1601U, 1105U, 35)); // Must not fail.
    EXPECT_EQ(4, cp.getApproxPrecisionPC());
    EXPECT_FALSE(cp.cannotRunProportional());
    EXPECT_FALSE(cp.updateAndCompute(0U, 0U, 35)); // Must fail (jammed actuator?).
    EXPECT_TRUE(cp.cannotRunProportional());
    const uint8_t mup = OTRadValve::CurrentSenseValveMotorDirect::CalibrationParameters::max_usuable_precision;
    EXPECT_LT(mup, cp.getApproxPrecisionPC());
    EXPECT_FALSE(cp.updateAndCompute(1U, 1U, 35)); // Must fail (not enough precision).
    EXPECT_TRUE(cp.cannotRunProportional());
    EXPECT_LT(mup, cp.getApproxPrecisionPC());

    // Check that hugely unbalanced inputs are not accepted
    EXPECT_FALSE(cp.updateAndCompute(4000U, 1105U, 35)); // Must fail (hugely unbalanced).
    EXPECT_TRUE(cp.cannotRunProportional());
    EXPECT_FALSE(cp.updateAndCompute(1601U, 4000U, 35)); // Must fail (hugely unbalanced).
    EXPECT_TRUE(cp.cannotRunProportional());

    // TODO: check behaviour on very large inputs.
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
    EXPECT_EQ(35, minTicks);
    // Compute maximum sub-cycle time to start valve movement as for V0p2/REV7 20161018.
    const uint8_t gst_max = 255;
    const uint8_t minimumMotorRunupTicks = 4; // OTRadValve::ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks as at 20161018.
    const uint8_t sctAbsLimit = OTRadValve::CurrentSenseValveMotorDirect::computeSctAbsLimit(subcycleTicksRoundedDown_ms, gst_max, minimumMotorRunupTicks);
    EXPECT_EQ(230, sctAbsLimit);

    // Check that default calibration state is 'error', ie 'cannot run proportional'.
    OTRadValve::CurrentSenseValveMotorDirect::CalibrationParameters cp0;
    ASSERT_TRUE(cp0.cannotRunProportional());

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
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::init, csbp->_getState());
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
    EXPECT_TRUE(csvmd1.inNonProportionalMode());
    // Nothing passed in requires deferral of (re)calibration.
    EXPECT_FALSE(csvmd1.shouldDeferCalibration());
}

class SVL final : public OTV0P2BASE::SupplyVoltageLow
    {
    public:
      SVL() { setAllLowFlags(false); }
      void setAllLowFlags(const bool f) { isLow = f; isVeryLow = f; }
      // Force a read/poll of the supply voltage and return the value sensed.
      // When battery is not low, read()/get() will return a non-zero value.
      // NOT thread-safe or usable within ISRs (Interrupt Service Routines).
      virtual uint16_t read() override { return(get()); }
      virtual uint16_t get() const override { return(isLow ? 0 : 1); }
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
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirectBase::init, csv->_getState());
//    // Check deferral of (re)calibration.
//    EXPECT_EQ(batteryLow, csv->shouldDeferCalibration());
    // Verify NOT marked as in normal run state immediately upon initialisation.
    EXPECT_TRUE(!csv->isInNormalRunState());
    // Verify NOT marked as in error state immediately upon initialisation.
    EXPECT_TRUE(!csv->isInErrorState());
    csv->poll();
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::initWaiting, csv->_getState());
    // Within a reasonable time to (10s of seconds) should move to new state, but not instantly.
    csv->poll();
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::initWaiting, csv->_getState());
    csv->poll();
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::initWaiting, csv->_getState());
    for(int i = 100; --i > 0 && OTRadValve::CurrentSenseValveMotorDirect::initWaiting == csv->_getState(); ) { csv->poll(); }
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::valvePinWithdrawing, csv->_getState());
    // Fake hardware hits end-stop immediate, so leaves 'withdrawing' state.
    csv->poll();
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::valvePinWithdrawn, csv->_getState());
    EXPECT_LE(95, csv->getCurrentPC()) << "valve must now be fully open, or very nearly so";
    // Wait indefinitely for valve to be signalled that it has been fitted before starting operation...
    for(int i = 1000; --i > 0 ; ) { csv->poll(); }
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::valvePinWithdrawn, csv->_getState());
    csv->signalValveFitted();
    csv->poll();
    EXPECT_EQ(OTRadValve::CurrentSenseValveMotorDirect::valveCalibrating, csv->_getState());
    csv->poll();
    // Valve should now start calibrating, but calibration is skipped with low battery or low light...
    EXPECT_EQ((batteryLow || csv->isNonProportionalOnly())
        ? OTRadValve::CurrentSenseValveMotorDirect::valveNormal :
          OTRadValve::CurrentSenseValveMotorDirect::valveCalibrating, csv->_getState()) << (batteryLow ? "low" : "normal");
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

// Test walk-through for normal state space.
// Check that eventually valve gets to requested % open or close enough to it.
// This allows for binary-mode (ie non-proportional) drivers.
// This is more of a black box test,
// ie laregly blind to the internal implementation/state like a normal human being would be.
static void normalStateWalkthrough(OTRadValve::CurrentSenseValveMotorDirectBase *const csv, const bool batteryLow)
    {
    // Run up driver/valve into 'normal' state by signalling the valve is fitted until good things happen.
    // May take a few minutes but no more (at 30 polls/ticks per minute, 100 polls should be enough).
    for(int i = 100; --i > 0 && !csv->isInNormalRunState(); ) { csv->signalValveFitted(); csv->poll(); }
    EXPECT_TRUE(!csv->isInErrorState());
    EXPECT_TRUE(csv->isInNormalRunState()) << csv->_getState();

    // Target % values to try to reach.
    // Some are listed repeatedly to ensure no significant sticky state.
    const uint8_t randomTarget1 = uint8_t(((unsigned) random()) % 101);
    uint8_t targetValues[] = { 0, 100, 1, 2, 25, 50, 75, randomTarget1, 0, 100, OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN, OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN, OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN };
    for(int i = 0; i < sizeof(targetValues); ++i)
        {
        const uint8_t target = targetValues[i];
        csv->setTargetPC(target);
        // Allow at most a minute or three (at 30 ticks/s) to reach the target (or close enough).
        for(int i = 100; --i > 0 && (target != csv->getCurrentPC()); ) { csv->poll(); }
        // Work out if we have got close enough:
        //   * fully open and fully closed should always be achieved
        //   * generally within an absolute tolerance (absTolerancePC) of the target value (eg 10--25%)
        //   * when target is below DEFAULT_VALVE_PC_SAFER_OPEN then any value at/below target is acceptable
        //   * when target is at or above DEFAULT_VALVE_PC_SAFER_OPEN then any value at/above target is acceptable
        const uint8_t currentPC = csv->getCurrentPC();
        const bool isCloseEnough = OTRadValve::CurrentSenseValveMotorDirectBase::closeEnoughToTarget(target, currentPC);
        if(target == currentPC) { EXPECT_TRUE(isCloseEnough) << "should always be 'close enough' with values equal"; }
        // Attempts to close the valve may be legitimately ignored when the battery is low.
        // But attempts to open fully should always be accepted, eg as anti-frost protection.
        if((!batteryLow) || (target == 100))
            { EXPECT_TRUE(isCloseEnough) << "target="<<((int)target) << ", current="<<((int)currentPC) << ", batteryLow="<<batteryLow; }
        // Ensure that driver has not reached an error (or other strange) state.
        EXPECT_TRUE(!csv->isInErrorState());
        EXPECT_TRUE(csv->isInNormalRunState()) << csv->_getState();
        }
    }
TEST(CurrentSenseValveMotorDirect,normalStateWalkthrough)
{
    const bool verbose = false;

    // Seed random() for use in simulator; --gtest_shuffle will force it to change.
    srandom(::testing::UnitTest::GetInstance()->random_seed());

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
        normalStateWalkthrough(&csvmdbo1, low);

        // Test full impl.
        DummyHardwareDriverHitEndstop dhw1;
        OTRadValve::CurrentSenseValveMotorDirect csvmd1(&dhw1, dummyGetSubCycleTime,
            OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::computeMinMotorDRTicks(subcycleTicksRoundedDown_ms),
            OTRadValve::CurrentSenseValveMotorDirectBinaryOnly::computeSctAbsLimit(subcycleTicksRoundedDown_ms,
                                                                         gsct_max,
                                                                         minimumMotorRunupTicks),
            &svl,
            [](){return(false);});
        normalStateWalkthrough(&csvmd1, low);
        EXPECT_TRUE(csvmd1.inNonProportionalMode()) << "with instant-end-stop driver, should be in non-prop mode";
        }
}
