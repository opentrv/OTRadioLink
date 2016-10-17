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

  testCSVMDC();
  testCurrentSenseValveMotorDirect();

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
