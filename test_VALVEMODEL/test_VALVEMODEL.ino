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

Author(s) / Copyright (s): Damon Hart-Davis 2015
*/

/*
 * EXTRA unit test routines for valve-control-model library code.
 *
 * Should all be runnable on any UNO-alike or emulator;
 * does not need V0p2 boards or hardware.
 *
 * Some tests here may duplicate those in the core libraries,
 * which is OK in order to double-check some core functionality.
 */

// Arduino libraries imported here (even for use in other .cpp files).

#define UNIT_TESTS

// Include the library under test.
#include <Wire.h>
#include <OTV0p2Base.h>
#include <OTRadValve.h>

#if F_CPU == 1000000 // 1MHz CPU indicates V0p2 board.
#define ON_V0P2_BOARD
#endif

void setup()
  {
#ifdef ON_V0P2_BOARD
  // initialize serial communications at 4800 bps for typical use with V0p2 board.
  Serial.begin(4800);
#else
  // initialize serial communications at 9600 bps for typical use with (eg) Arduino UNO.
  Serial.begin(9600);
#endif
  }



// Error exit from failed unit test, one int parameter and the failing line number to print...
// Expects to terminate like panic() with flashing light can be detected by eye or in hardware if required.
static void error(int err, int line)
  {
  for( ; ; )
    {
    Serial.print(F("***Test FAILED*** val="));
    Serial.print(err, DEC);
    Serial.print(F(" =0x"));
    Serial.print(err, HEX);
    if(0 != line)
      {
      Serial.print(F(" at line "));
      Serial.print(line);
      }
    Serial.println();
//    LED_HEATCALL_ON();
//    tinyPause();
//    LED_HEATCALL_OFF();
//    sleepLowPowerMs(1000);
    delay(1000);
    }
  }

// Deal with common equality test.
static inline void errorIfNotEqual(int expected, int actual, int line) { if(expected != actual) { error(actual, line); } }
// Allowing a delta.
static inline void errorIfNotEqual(int expected, int actual, int delta, int line) { if(abs(expected - actual) > delta) { error(actual, line); } }

// Test expression and bucket out with error if false, else continue, including line number.
// Macros allow __LINE__ to work correctly.
#define AssertIsTrueWithErr(x, err) { if(!(x)) { error((err), __LINE__); } }
#define AssertIsTrue(x) AssertIsTrueWithErr((x), 0)
#define AssertIsEqual(expected, x) { errorIfNotEqual((expected), (x), __LINE__); }
#define AssertIsEqualWithDelta(expected, x, delta) { errorIfNotEqual((expected), (x), (delta), __LINE__); }


// Check that correct version of this library is under test.
static void testLibVersion()
  {
  Serial.println("LibVersion");
#if !(1 == ARDUINO_LIB_OTRADVALVE_VERSION_MAJOR) || !(0 == ARDUINO_LIB_OTRADVALVE_VERSION_MINOR)
#error Wrong library version!
#endif 
  }

// Check that correct versions of underlying libraries are in use.
static void testLibVersions()
  {
  Serial.println("LibVersions");
#if !(1 == ARDUINO_LIB_OTV0P2BASE_VERSION_MAJOR) || !(0 == ARDUINO_LIB_OTV0P2BASE_VERSION_MINOR)
#error Wrong library version!
#endif  
  }


// OTRadValve

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

// Test calibration calculations in CurrentSenseValveMotorDirect.
// Also check some of the use of those calculations.
static void testCSVMDC()
  {
  Serial.println("CSVMDC");
  OTRadValve::CurrentSenseValveMotorDirect::CalibrationParameters cp;
  volatile uint16_t ticksFromOpen, ticksReverse;
  // Test the calculations with one plausible calibration data set.
  AssertIsTrue(cp.updateAndCompute(1601U, 1105U)); // Must not fail...
  AssertIsEqual(4, cp.getApproxPrecisionPC());
  AssertIsEqual(25, cp.getTfotcSmall());
  AssertIsEqual(17, cp.getTfctoSmall());
  // Check that a calibration instance can be reused correctly.
  const uint16_t tfo2 = 1803U;
  const uint16_t tfc2 = 1373U;
  AssertIsTrue(cp.updateAndCompute(tfo2, tfc2)); // Must not fail...
  AssertIsEqual(3, cp.getApproxPrecisionPC());
  AssertIsEqual(28, cp.getTfotcSmall());
  AssertIsEqual(21, cp.getTfctoSmall());
  // Check that computing position works...
  // Simple case: fully closed, no accumulated reverse ticks.
  ticksFromOpen = tfo2;
  ticksReverse = 0;
  AssertIsEqual(0, cp.computePosition(ticksFromOpen, ticksReverse));
  AssertIsEqual(tfo2, ticksFromOpen);
  AssertIsEqual(0, ticksReverse);
  // Simple case: fully open, no accumulated reverse ticks.
  ticksFromOpen = 0;
  ticksReverse = 0;
  AssertIsEqual(100, cp.computePosition(ticksFromOpen, ticksReverse));
  AssertIsEqual(0, ticksFromOpen);
  AssertIsEqual(0, ticksReverse);
  // Try at half-way mark, no reverse ticks.
  ticksFromOpen = tfo2 / 2;
  ticksReverse = 0;
  AssertIsEqual(50, cp.computePosition(ticksFromOpen, ticksReverse));
  AssertIsEqual(tfo2/2, ticksFromOpen);
  AssertIsEqual(0, ticksReverse);
  // Try at half-way mark with just one reverse tick (nothing should change).
  ticksFromOpen = tfo2 / 2;
  ticksReverse = 1;
  AssertIsEqual(50, cp.computePosition(ticksFromOpen, ticksReverse));
  AssertIsEqual(tfo2/2, ticksFromOpen);
  AssertIsEqual(1, ticksReverse);
  // Try at half-way mark with a big-enough block of reverse ticks to be significant.
  ticksFromOpen = tfo2 / 2;
  ticksReverse = cp.getTfctoSmall();
  AssertIsEqual(51, cp.computePosition(ticksFromOpen, ticksReverse));
  AssertIsEqual(tfo2/2 - cp.getTfotcSmall(), ticksFromOpen);
  AssertIsEqual(0, ticksReverse);
// DHD20151025: one set of actual measurements during calibration.
//    ticksFromOpenToClosed: 1529
//    ticksFromClosedToOpen: 1295
  }

// Test that direct abstract motor drive logic is sane.
static void testCurrentSenseValveMotorDirect()
  {
  Serial.println("CurrentSenseValveMotorDirect");
  DummyHardwareDriver dhw;
  OTRadValve::CurrentSenseValveMotorDirect csvmd1(&dhw);
  // POWER IP
  // Whitebox test of internal state: should be init.
  AssertIsEqual(OTRadValve::CurrentSenseValveMotorDirect::init, csvmd1.getState());
  // Verify NOT marked as in normal run state immediately upon initialisation.
  AssertIsTrue(!csvmd1.isInNormalRunState());
  // Verify NOT marked as in error state immediately upon initialisation.
  AssertIsTrue(!csvmd1.isInErrorState());
  // Target % open must start off in a sensible state; fully-closed is good.
  AssertIsEqual(0, csvmd1.getTargetPC());

  // FIRST POLL(S) AFTER POWER_UP; RETRACTING THE PIN.
  csvmd1.poll();
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


// Test for general sanity of computation of desired valve position.
// In particular test the logic in ModelledRadValveState for starting from extreme positions.
static void testMRVSExtremes()
  {
  Serial.println("MRVSExtremes");
  // Test that if the real temperature is zero
  // and the initial valve position is anything less than 100%
  // that after one tick (with mainly defaults)
  // that the valve is being opened (and more than glacially),
  // ie that when below any possible legal target FROST/WARM/BAKE temperature the valve will open monotonically,
  // and also test that the fully-open state is reached in a bounded number of ticks ie bounded time.
  static const int maxFullTravelMins = 25;
//  V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("open...");
  OTRadValve::ModelledRadValveInputState is0(0);
  is0.targetTempC = OTV0P2BASE::randRNG8NextBoolean() ? 5 : 25;
  OTRadValve::ModelledRadValveState rs0;
  const uint8_t valvePCOpenInitial0 = OTV0P2BASE::randRNG8() % 100;
  volatile uint8_t valvePCOpen = valvePCOpenInitial0;
  for(int i = maxFullTravelMins; --i >= 0; ) // Must fully open in reasonable time.
    {
    // Simulates one minute on each iteration.
    // Futz some input parameters that should not matter.
    is0.widenDeadband = OTV0P2BASE::randRNG8NextBoolean();
    is0.hasEcoBias = OTV0P2BASE::randRNG8NextBoolean();
    const uint8_t oldValvePos = valvePCOpen;
    rs0.tick(valvePCOpen, is0);
    const uint8_t newValvePos = valvePCOpen;
    AssertIsTrue(newValvePos > 0);
    AssertIsTrue(newValvePos <= 100);
    AssertIsTrue(newValvePos > oldValvePos);
    if(oldValvePos < is0.minPCOpen) { AssertIsTrue(is0.minPCOpen <= newValvePos); } // Should open to at least minimum-really-open-% on first step.
    AssertIsTrue(rs0.valveMoved == (oldValvePos != newValvePos));
    if(100 == newValvePos) { break; }
    }
  AssertIsEqual(100, valvePCOpen);
  AssertIsEqual(100 - valvePCOpenInitial0, rs0.cumulativeMovementPC);
  // Equally test that if the temperature is much higher than any legit target
  // the valve will monotonically close to 0% in bounded time.
  // Check for superficially correct linger behaviour:
  //   * minPCOpen-1 % must be hit (lingering close) if starting anywhere above that.
  //   * Once in linger all reductions should be by 1% until possible final jump to 0.
  //   * Check that linger was long enough (if linger threshold is higher enough to allow it).
  // Also check for some correct initialisation and 'velocity'/smoothing behaviour.
//  V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("close...");
  OTRadValve::ModelledRadValveInputState is1(100<<4);
  is1.targetTempC = OTV0P2BASE::randRNG8NextBoolean() ? 5 : 25;
  OTRadValve::ModelledRadValveState rs1;
  AssertIsTrue(!rs1.initialised); // Initialisation not yet complete.
  const uint8_t valvePCOpenInitial1 = 1 + (OTV0P2BASE::randRNG8() % 100);
  valvePCOpen = valvePCOpenInitial1;
  const bool lookForLinger = (valvePCOpenInitial1 >= is1.minPCOpen);
  bool hitLinger = false; // True if the linger value was hit.
  uint8_t lingerMins = 0; // Approx mins spent in linger.
  for(int i = maxFullTravelMins; --i >= 0; ) // Must fully close in reasonable time.
    {
    // Simulates one minute on each iteration.
    // Futz some input parameters that should not matter.
    is1.widenDeadband = OTV0P2BASE::randRNG8NextBoolean();
    is1.hasEcoBias = OTV0P2BASE::randRNG8NextBoolean();
    const uint8_t oldValvePos = valvePCOpen;
    rs1.tick(valvePCOpen, is1);
    const uint8_t newValvePos = valvePCOpen;
    AssertIsTrue(rs1.initialised); // Initialisation must have completed.
    AssertIsTrue(newValvePos < 100);
//    AssertIsTrue(newValvePos >= 0);
    AssertIsTrue(newValvePos < oldValvePos);
    if(hitLinger) { ++lingerMins; }
    if(hitLinger && (0 != newValvePos)) { AssertIsEqual(oldValvePos - 1, newValvePos); }
    if(newValvePos == is1.minPCOpen-1) { hitLinger = true; }
    AssertIsTrue(rs1.valveMoved == (oldValvePos != newValvePos));
    if(0 == newValvePos) { break; }
    }
  AssertIsEqual(0, valvePCOpen);
  AssertIsEqual(valvePCOpenInitial1, rs1.cumulativeMovementPC);
  AssertIsTrue(hitLinger == lookForLinger);
  if(lookForLinger) { AssertIsTrue(lingerMins >= min(is1.minPCOpen, OTRadValve::DEFAULT_MAX_RUN_ON_TIME_M)); }
  // Filtering should not have been engaged and velocity should be zero (temperature is flat).
  for(int i = OTRadValve::ModelledRadValveState::filterLength; --i >= 0; ) { AssertIsEqual(100<<4, rs1.prevRawTempC16[i]); }
  AssertIsEqual(100<<4, rs1.getSmoothedRecent());
//  AssertIsEqual(0, rs1.getVelocityC16PerTick());
  AssertIsTrue(!rs1.isFiltering);
  // Some tests of basic velocity computation.
//  ModelledRadValveState rs2;
//  // Test with steady rising/falling value.
//  const int step2C16 = (randRNG8() & 0x1f) - 16;
//V0P2BASE_DEBUG_SERIAL_PRINT(step2C16);
//V0P2BASE_DEBUG_SERIAL_PRINTLN();
//  const int base2C16 = (FROST + (randRNG8() % (WARM - FROST))) << 16;
//  rs2.prevRawTempC16[0] = base2C16;
//  for(int i = 1; i < ModelledRadValveState::filterLength; ++i)
//    { rs2.prevRawTempC16[i] = rs2.prevRawTempC16[i-1] - step2C16; }
////V0P2BASE_DEBUG_SERIAL_PRINT(rs2.getVelocityC16PerTick());
////V0P2BASE_DEBUG_SERIAL_PRINTLN();
//  AssertIsEqualWithDelta(step2C16, rs2.getVelocityC16PerTick(), 2);
  // Test that soft setback works as expected to support dark-based quick setback.
  // ENERGY SAVING RULE TEST (TODO-442 2a: "Setback in WARM mode must happen in dark (quick response) or long vacant room.")
    // ENERGY SAVING RULE TEST (TODO-442 2a: "Setback in WARM mode must happen in dark (quick response) or long vacant room.")
    OTRadValve::ModelledRadValveInputState is3(100<<4);
    is3.targetTempC = 25;
    // Try a range of (whole-degree) offsets...
    for(int offset = -2; offset <= +2; ++offset)
      {
      // Try soft setback off and on.
      for(int s = 0; s < 2; ++s) 
        {
        // Other than in the proportional range, valve should unconditionally be driven off/on by gross temperature error.
        if(0 != offset)
          {
          is3.refTempC16 = (is3.targetTempC + offset) << 4;
          // Where adjusted reference temperature is (well) below target, valve should be driven on.
          OTRadValve::ModelledRadValveState rs3a;
          valvePCOpen = 0;
          rs3a.tick(valvePCOpen, is3);
//V0P2BASE_DEBUG_SERIAL_PRINT('@');
//V0P2BASE_DEBUG_SERIAL_PRINT(offset);
//V0P2BASE_DEBUG_SERIAL_PRINT(' ');
//V0P2BASE_DEBUG_SERIAL_PRINT(valvePCOpen);
//V0P2BASE_DEBUG_SERIAL_PRINTLN();
          AssertIsTrue((offset < 0) ? (valvePCOpen > 0) : (0 == valvePCOpen));
          // Where adjusted reference temperature is (well) above target, valve should be driven off.
          OTRadValve::ModelledRadValveState rs3b;
          valvePCOpen = 100;
          rs3b.tick(valvePCOpen, is3);
          AssertIsTrue((offset < 0) ? (100 == valvePCOpen) : (valvePCOpen < 100));
          }
        else
          {
          // Below the half way mark the valve should always be opened (from off), soft setback or not.
          is3.refTempC16 = (is3.targetTempC << 4) + 0x4;
          OTRadValve::ModelledRadValveState rs3c;
          valvePCOpen = 0;
          rs3c.tick(valvePCOpen, is3);
          AssertIsTrue(valvePCOpen > 0);
          // Above the half way mark the valve should only be opened without soft setback.
          is3.refTempC16 = (is3.targetTempC << 4) + 0xc;
          OTRadValve::ModelledRadValveState rs3d;
          valvePCOpen = 0;
          rs3d.tick(valvePCOpen, is3);
          AssertIsTrue(0 == valvePCOpen);
          }
        }
      }
  }

// Test the logic in ModelledRadValveState to open fast from well below target (TODO-593).
// This is to cover the case where the use manually turns on/up the valve
// and expects quick response from the valve
// and the remote boiler (which may require >= DEFAULT_VALVE_PC_MODERATELY_OPEN to start).
// This relies on no widened deadband being set.
// It may also require filtering (from gyrating temperatures) not to have been invoked.
static void testMRVSOpenFastFromCold593()
  {
  Serial.println("MRVSOpenFastFromCold593");
  // Test that if the real temperature is at least 2 degrees below the target
  // and the initial valve position is 0/closed
  // (or any below OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN)
  // and a widened deadband has not been requested
  // (and filtering is not switched on)
  // that after one tick
  // that the valve is open to at least DEFAULT_VALVE_PC_MODERATELY_OPEN.
  // Starting temp >2C below target, even with 0.5 offset.
  OTRadValve::ModelledRadValveInputState is0(OTV0P2BASE::randRNG8() & 0xf8);
  is0.targetTempC = 18; // Modest target temperature.
  OTRadValve::ModelledRadValveState rs0;
  volatile uint8_t valvePCOpen = OTV0P2BASE::randRNG8() % OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN;
  // Futz some input parameters that should not matter.
  is0.widenDeadband = false;
  rs0.isFiltering = OTV0P2BASE::randRNG8NextBoolean();
  is0.hasEcoBias = OTV0P2BASE::randRNG8NextBoolean();
  // Run the algorithm one tick.
  rs0.tick(valvePCOpen, is0);
  const uint8_t newValvePos = valvePCOpen;
  AssertIsTrue(newValvePos >= OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN);
  AssertIsTrue(newValvePos <= 100);
  AssertIsTrue(rs0.valveMoved);
  }


// Test (fast) mappings back and forth between [0,100] valve open percentage and [0,255] FS20 representation.
static void testFHT8VPercentage()
  {
  Serial.println("FHT8VPercentage");
  // Test that end-points are mapped correctly from % -> FS20 scale.
  AssertIsEqual(0, OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(0));
  AssertIsEqual(255, OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(100));
  // Test that illegal/over values handled sensibly.
  AssertIsEqual(255, OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(101));
  AssertIsEqual(255, OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(255));
  // Test that end-points are mapped correctly from FS20 scale to %.
  AssertIsEqual(0, OTRadValve::FHT8VRadValveBase::convert255ScaleToPercent(0));
  AssertIsEqual(100, OTRadValve::FHT8VRadValveBase::convert255ScaleToPercent(255));
  // Test that some critical thresholds are mapped back and forth (round-trip) correctly.
  AssertIsEqual(OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN, OTRadValve::FHT8VRadValveBase::convert255ScaleToPercent(OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN)));
  AssertIsEqual(OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN, OTRadValve::FHT8VRadValveBase::convert255ScaleToPercent(OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN)));
  // Check that all mappings back and forth (round-trip) are reasonably close to target.
  const uint8_t eps = 2; // Tolerance in %
  for(uint8_t i = 0; i <= 100; ++i)
    {
    AssertIsEqualWithDelta(i, OTRadValve::FHT8VRadValveBase::convert255ScaleToPercent(OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(i)), eps);  
    }
  // Check for monotonicity of mapping % -> FS20.
  for(uint8_t i = 0; i < 100; ++i)
    {
    AssertIsTrue(
      OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(i) <=
      OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(i+1));  
    }
  // Check for monotonicity of mapping FS20 => %.
  for(uint8_t i = 0; i < 100; ++i)
    {
    AssertIsTrue(
      OTRadValve::FHT8VRadValveBase::convert255ScaleToPercent(OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(i)) <=
      OTRadValve::FHT8VRadValveBase::convert255ScaleToPercent(OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(i+1)));  
    }
  // Check for monotonicity of round-trip.
  for(uint8_t i = 0; i < 100; ++i)
    {
    AssertIsTrue(
      OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(i) <=
      OTRadValve::FHT8VRadValveBase::convertPercentTo255Scale(i+1));  
    }
  }


// Tests generally flag an error and stop the test cycle with a call to panic() or error().
void loop()
  {
  static int loopCount = 0;

  // Allow the terminal console to be brought up.
  for(int i = 3; i > 0; --i)
    {
    Serial.print(F("Tests starting... "));
    Serial.print(i);
    Serial.println();
    delay(1000);
    }
  Serial.println();


  // Run the tests, fastest / newest / most-fragile / most-interesting first...
  testLibVersion();
  testLibVersions();

  testFHT8VPercentage();
  testCSVMDC();
  testCurrentSenseValveMotorDirect();
  testMRVSExtremes();
  testMRVSOpenFastFromCold593();


  // Announce successful loop completion and count.
  ++loopCount;
  Serial.println();
  Serial.print(F("%%% All tests completed OK, round "));
  Serial.print(loopCount);
  Serial.println();
  Serial.println();
  Serial.println();
  delay(2000);
  }
