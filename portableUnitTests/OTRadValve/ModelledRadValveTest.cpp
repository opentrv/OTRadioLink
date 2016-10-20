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
 * OTRadValve ModelledRadValve tests.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <OTV0P2BASE_QuickPRNG.h>

#include "OTRadValve_AbstractRadValve.h"
#include "OTRadValve_ModelledRadValve.h"


// Test for general sanity of computation of desired valve position.
// In particular test the logic in ModelledRadValveState for starting from extreme positions.
//
// Adapted 2016/10/16 from test_VALVEMODEL.ino testMRVSExtremes().
TEST(ModelledRadValve,MRVSExtremes)
{
    // If true then be more verbose.
    const static bool verbose = false;

    // Test that if the real temperature is zero
    // and the initial valve position is anything less than 100%
    // that after one tick (with mainly defaults)
    // that the valve is being opened (and more than glacially),
    // ie that when below any possible legal target FROST/WARM/BAKE temperature the valve will open monotonically,
    // and also test that the fully-open state is reached in a bounded number of ticks ie bounded time.
    static const int maxFullTravelMins = 25;
    if(verbose) { fprintf(stderr, "open...\n"); }
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
        ASSERT_TRUE(newValvePos > 0);
        ASSERT_TRUE(newValvePos <= 100);
        ASSERT_TRUE(newValvePos > oldValvePos);
        if(oldValvePos < is0.minPCOpen) { ASSERT_TRUE(is0.minPCOpen <= newValvePos); } // Should open to at least minimum-really-open-% on first step.
        ASSERT_TRUE(rs0.valveMoved == (oldValvePos != newValvePos));
        if(100 == newValvePos) { break; }
        }
    ASSERT_EQ(100, valvePCOpen);
    ASSERT_EQ(100 - valvePCOpenInitial0, rs0.cumulativeMovementPC);
    // Equally test that if the temperature is much higher than any legit target
    // the valve will monotonically close to 0% in bounded time.
    // Check for superficially correct linger behaviour:
    //   * minPCOpen-1 % must be hit (lingering close) if starting anywhere above that.
    //   * Once in linger all reductions should be by 1% until possible final jump to 0.
    //   * Check that linger was long enough (if linger threshold is higher enough to allow it).
    // Also check for some correct initialisation and 'velocity'/smoothing behaviour.
    if(verbose) { fprintf(stderr, "close...\n"); }
    OTRadValve::ModelledRadValveInputState is1(100<<4);
    is1.targetTempC = OTV0P2BASE::randRNG8NextBoolean() ? 5 : 25;
    OTRadValve::ModelledRadValveState rs1;
    ASSERT_TRUE(!rs1.initialised); // Initialisation not yet complete.
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
        ASSERT_TRUE(valvePCOpen >= 0); // Nominally never false because unsigned type.
        rs1.tick(valvePCOpen, is1);
        const uint8_t newValvePos = valvePCOpen;
        ASSERT_TRUE(valvePCOpen >= 0); // Nominally never false because unsigned type.
        ASSERT_TRUE(rs1.initialised); // Initialisation must have completed.
        ASSERT_TRUE(newValvePos < 100);
        ASSERT_TRUE(newValvePos < oldValvePos);
        if(hitLinger) { ++lingerMins; }
        if(hitLinger && (0 != newValvePos)) { ASSERT_EQ(oldValvePos - 1, newValvePos); }
        if(newValvePos == is1.minPCOpen-1) { hitLinger = true; }
        ASSERT_TRUE(rs1.valveMoved == (oldValvePos != newValvePos));
        if(0 == newValvePos) { break; }
        }
    ASSERT_EQ(0, valvePCOpen);
    ASSERT_EQ(valvePCOpenInitial1, rs1.cumulativeMovementPC);
    ASSERT_TRUE(hitLinger == lookForLinger);
    if(lookForLinger) { ASSERT_TRUE(lingerMins >= OTV0P2BASE::fnmin(is1.minPCOpen, OTRadValve::DEFAULT_MAX_RUN_ON_TIME_M)); }
    // Filtering should not have been engaged and velocity should be zero (temperature is flat).
    for(int i = OTRadValve::ModelledRadValveState::filterLength; --i >= 0; ) { ASSERT_EQ(100<<4, rs1.prevRawTempC16[i]); }
    ASSERT_EQ(100<<4, rs1.getSmoothedRecent());
    //  AssertIsEqual(0, rs1.getVelocityC16PerTick());
    ASSERT_TRUE(!rs1.isFiltering);

    // Test that soft setback works as expected to support dark-based quick setback.
    // ENERGY SAVING RULE TEST (TODO-442 2a: "Setback in WARM mode must happen in dark (quick response) or long vacant room.")
    // Try a range of (whole-degree) offsets...
    for(int offset = -2; offset <= +2; ++offset)
        {
        // Try soft setback off and on.
        for(int s = 0; s < 2; ++s)
            {
            OTRadValve::ModelledRadValveInputState is3(100<<4);
            is3.targetTempC = 25;
            is3.widenDeadband = (s == 1);
            // Other than in the proportional range, valve should unconditionally be driven off/on by gross temperature error.
            if(0 != offset)
                {
                is3.setReferenceTemperatures((is3.targetTempC + offset) << 4);
                // Where adjusted reference temperature is (well) below target, valve should be driven on.
                OTRadValve::ModelledRadValveState rs3a;
                valvePCOpen = 0;
                rs3a.tick(valvePCOpen, is3);
if(verbose) { fprintf(stderr, "@ %d %d\n", offset, valvePCOpen); }
                EXPECT_TRUE((offset < 0) ? (valvePCOpen > 0) : (0 == valvePCOpen)) << (int)valvePCOpen;
                // Where adjusted reference temperature is (well) above target, valve should be driven off.
                OTRadValve::ModelledRadValveState rs3b;
                valvePCOpen = 100;
                rs3b.tick(valvePCOpen, is3);
                EXPECT_TRUE((offset < 0) ? (100 == valvePCOpen) : (valvePCOpen < 100)) << (int)valvePCOpen;
                }
            else
                {
                // In proportional range, ie fairly close to target.

                // (Even well) below the half way mark the valve should only be closed
                // with temperature moving in wrong direction and without soft setback.
                is3.setReferenceTemperatures((is3.targetTempC << 4) + 0x1);
                OTRadValve::ModelledRadValveState rs3c;
                valvePCOpen = 100;
                rs3c.tick(valvePCOpen, is3);
                if(is3.widenDeadband) { EXPECT_EQ(100, valvePCOpen); } else { EXPECT_GT(100, valvePCOpen); }
                --is3.refTempC16;
                rs3c.tick(valvePCOpen, is3);
                if(is3.widenDeadband) { EXPECT_EQ(100, valvePCOpen); } else { EXPECT_GT(100, valvePCOpen); }

                // (Even well) above the half way mark the valve should only be opened
                // with temperature moving in wrong direction and without soft setback.
                is3.setReferenceTemperatures((is3.targetTempC << 4) + 0xe);
                OTRadValve::ModelledRadValveState rs3d;
                valvePCOpen = 0;
                rs3d.tick(valvePCOpen, is3);
                EXPECT_EQ(0, valvePCOpen);
// FIXME
//                ++is3.refTempC16;
//                rs3c.tick(valvePCOpen, is3);
//                if(is3.widenDeadband) { EXPECT_EQ(0, valvePCOpen); } else { EXPECT_LT(0, valvePCOpen); }
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
//
// Adapted 2016/10/16 from test_VALVEMODEL.ino testMRVSOpenFastFromCold593().
TEST(ModelledRadValve,MRVSOpenFastFromCold593)
{
    // Test that if the real temperature is at least 2 degrees below the target
    // and the initial valve position is 0/closed
    // (or any below OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN)
    // and a widened deadband has not been requested
    // (and filtering is not switched on)
    // after one tick
    // that the valve is open to at least DEFAULT_VALVE_PC_MODERATELY_OPEN.
    // Starting temp >2C below target, even with 0.5 offset.
    OTRadValve::ModelledRadValveInputState is0(OTV0P2BASE::randRNG8() & 0xf8);
    is0.targetTempC = 18; // Modest target temperature.
    OTRadValve::ModelledRadValveState rs0;
    is0.widenDeadband = false;
    volatile uint8_t valvePCOpen = OTV0P2BASE::randRNG8() % OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN;
    // Futz some input parameters that should not matter.
    rs0.isFiltering = OTV0P2BASE::randRNG8NextBoolean();
    is0.hasEcoBias = OTV0P2BASE::randRNG8NextBoolean();
    // Run the algorithm one tick.
    rs0.tick(valvePCOpen, is0);
    const uint8_t newValvePos = valvePCOpen;
    ASSERT_TRUE(newValvePos >= OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN);
    ASSERT_TRUE(newValvePos <= 100);
    ASSERT_TRUE(rs0.valveMoved);
    ASSERT_EQ(OTRadValve::ModelledRadValveState::MRVE_OPENFAST, rs0.lastEvent);
}


// Test that the cold draught detector works, with simple synthetic case.
// Check that a sufficiently sharp drop in temperature
// (when already below target temperature)
// inhibits further heating at least partly for a while.
TEST(ModelledRadValve,DraughtDetectorSimple)
{
    // If true then be more verbose.
    const static bool verbose = false;

    // Run the test a few times to help ensure no dependency on state of random generator, etc.
    for(int i = 8; --i >= 0; )
        {
        // Test that if the real temperature is moderately-to-much below the target
        // (allowing for any internal offsetting)
        // and the initial valve position is anywhere [0,100]
        // but the final temperature measurement shows a large drop
        // (and ECO mode is enabled, and no fast response)
        // after one tick
        // the valve is open to less than DEFAULT_VALVE_PC_SAFER_OPEN
        // to try to ensure no call for heat from the boiler.
        //
        // Starting temp as a little below target.
        const uint8_t targetC = OTRadValve::SAFE_ROOM_TEMPERATURE;
        const int_fast16_t roomTemp = (targetC << 4) - 15- (OTV0P2BASE::randRNG8() % 32);
if(verbose) { fprintf(stderr, "Start\n"); }
        OTRadValve::ModelledRadValveInputState is0(roomTemp);
        is0.targetTempC = targetC;
        OTRadValve::ModelledRadValveState rs0(is0);
        volatile uint8_t valvePCOpen = OTV0P2BASE::randRNG8() % 100;
if(verbose) { fprintf(stderr, "Valve %d%%.\n", valvePCOpen); }
        // Set necessary conditions to allow draught-detector.
        // (Not allowed to activate in comfort mode, or when user has just adjusted the controls.)
        is0.hasEcoBias = true;
        is0.fastResponseRequired = false;
        // Futz some input parameters that should not matter.
        is0.widenDeadband = OTV0P2BASE::randRNG8NextBoolean();
        rs0.isFiltering = OTV0P2BASE::randRNG8NextBoolean();
        // Set a new significantly lower room temp (drop >=0.5C), as from a draught.
        const int_fast16_t droppedRoomTemp = roomTemp - 8 - (OTV0P2BASE::randRNG8() % 32);
        is0.setReferenceTemperatures(droppedRoomTemp);
        // Run the algorithm one tick.
        rs0.tick(valvePCOpen, is0);
if(verbose) { fprintf(stderr, "Valve %d%%.\n", valvePCOpen); }
        const uint8_t newValvePos = valvePCOpen;
        EXPECT_LT(newValvePos, OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN);
        ASSERT_EQ(OTRadValve::ModelledRadValveState::MRVE_DRAUGHT, rs0.lastEvent);
        }
}

// C16 (Celsius*16) room Temperature and target data samples, along with optional expected event from ModelledRadValve.
// Can be directly created from OpenTRV log files.
class C16DataSample
    {
    public:
        const uint8_t d, H, M, tC;
        const int16_t C16;
        const OTRadValve::ModelledRadValveState::event_t expected;

        // Day/hour/minute and light level and expected result.
        // An expected result of 0 means no particular event expected from this (anything is acceptable).
        C16DataSample(uint8_t dayOfMonth, uint8_t hour24, uint8_t minute,
                      uint8_t tTempC, int16_t tempC16,
                      OTRadValve::ModelledRadValveState::event_t expectedResult = OTRadValve::ModelledRadValveState::MRVE_NONE)
            : d(dayOfMonth), H(hour24), M(minute), tC(tTempC), C16(tempC16), expected(expectedResult)
            { }

        // Create/mark a terminating entry; all input values invalid.
        C16DataSample() : d(255), H(255), M(255), tC(255), C16(~0), expected(OTRadValve::ModelledRadValveState::MRVE_NONE) { }

        // Compute current minute for this record.
        long currentMinute() const { return((((d * 24L) + H) * 60L) + M); }

        // True for empty/termination data record.
        bool isEnd() const { return(d > 31); }
    };

// TODO: tests based on real data samples, for multiple aspects of functionality.
/* eg some or all of:
TODO-442:
1a) No prewarm (eg 'smart' extra heating in FROST mode) in a long-vacant room.
1b) Never a higher pre-warm/FROST-mode target temperature than WARM-mode target.
1c) Prewarm temperature must be set back from normal WARM target.

2a) Setback in WARM mode must happen in dark (quick response) or long vacant room.
2b) FULL setbacks (4C as at 20161016) must be possible in full eco mode.
2c) Setbacks are at most 2C in comfort mode (but there is a setback).
2d) Bigger setbacks are possible after a room has been vacant longer (eg for weekends).
2e) Setbacks should be targeted at times of expected low occupancy.
2f) Some setbacks should be possible in office environments with lights mainly or always on.
*/

// Nominally target up 0.25C--1C drop over a few minutes (limited by the filter length).
// TODO-621: in case of very sharp drop in temperature,
// assume that a window or door has been opened,
// by accident or to ventilate the room,
// so suppress heating to reduce waste.
//
// See one sample 'airing' data set:
//     http://www.earth.org.uk/img/20160930-16WWmultisensortempL.README.txt
//     http://www.earth.org.uk/img/20160930-16WWmultisensortempL.png
//     http://www.earth.org.uk/img/20160930-16WWmultisensortempL.json.xz
//
// 7h (hall, A9B2F7C089EECD89) saw a sharp fall and recovery, possibly from an external door being opened:
// 1C over 10 minutes then recovery by nearly 0.5C over next half hour.
// Note that there is a potential 'sensitising' occupancy signal available,
// ie sudden occupancy may allow triggering with a lower temperature drop.
//[ "2016-09-30T06:45:18Z", "", {"@":"A9B2F7C089EECD89","+":15,"T|C16":319,"H|%":65,"O":1} ]
//[ "2016-09-30T06:57:10Z", "", {"@":"A9B2F7C089EECD89","+":2,"L":101,"T|C16":302,"H|%":60} ]
//[ "2016-09-30T07:05:10Z", "", {"@":"A9B2F7C089EECD89","+":4,"T|C16":303,"v|%":0} ]
//[ "2016-09-30T07:09:08Z", "", {"@":"A9B2F7C089EECD89","+":5,"tT|C":16,"T|C16":305} ]
//[ "2016-09-30T07:21:08Z", "", {"@":"A9B2F7C089EECD89","+":8,"O":2,"T|C16":308,"H|%":64} ]
//[ "2016-09-30T07:33:12Z", "", {"@":"A9B2F7C089EECD89","+":11,"tS|C":0,"T|C16":310} ]
//
// Using an artificially high target temp in the test data to allow draught-mode detection.
static const C16DataSample sample7h[] =
    {
{ 0, 6, 45, 20, 319 },
{ 0, 6, 57, 20, 302, OTRadValve::ModelledRadValveState::MRVE_DRAUGHT },
{ 0, 7, 5, 20, 303 },
{ 0, 7, 9, 20, 305 },
{ 0, 7, 21, 20, 308 },
{ 0, 7, 33, 20, 310 },
{ }
    };
//
// 1g (bedroom, FEDA88A08188E083) saw a slower fall, assumed from airing:
// initially of .25C in 12m, 0.75C over 1h, bottoming out ~2h later down ~2C.
// Note that there is a potential 'sensitising' occupancy signal available,
// ie sudden occupancy may allow triggering with a lower temperature drop.
//
// Using an artificially high target temp in the test data to allow draught-mode detection.
//[ "2016-09-30T06:27:30Z", "", {"@":"FEDA88A08188E083","+":8,"tT|C":17,"tS|C":0} ]
//[ "2016-09-30T06:31:38Z", "", {"@":"FEDA88A08188E083","+":9,"gE":0,"T|C16":331,"H|%":67} ]
//[ "2016-09-30T06:35:30Z", "", {"@":"FEDA88A08188E083","+":10,"T|C16":330,"O":2,"L":2} ]
//[ "2016-09-30T06:43:30Z", "", {"@":"FEDA88A08188E083","+":12,"H|%":65,"T|C16":327,"O":2} ]
//[ "2016-09-30T06:59:34Z", "", {"@":"FEDA88A08188E083","+":0,"T|C16":325,"H|%":64,"O":1} ]
//[ "2016-09-30T07:07:34Z", "", {"@":"FEDA88A08188E083","+":2,"H|%":63,"T|C16":324,"O":1} ]
//[ "2016-09-30T07:15:36Z", "", {"@":"FEDA88A08188E083","+":4,"L":95,"tT|C":13,"tS|C":4} ]
//[ "2016-09-30T07:19:30Z", "", {"@":"FEDA88A08188E083","+":5,"vC|%":0,"gE":0,"T|C16":321} ]
//[ "2016-09-30T07:23:29Z", "", {"@":"FEDA88A08188E083","+":6,"T|C16":320,"H|%":63,"O":1} ]
//[ "2016-09-30T07:31:27Z", "", {"@":"FEDA88A08188E083","+":8,"L":102,"T|C16":319,"H|%":63} ]
// ...
//[ "2016-09-30T08:15:27Z", "", {"@":"FEDA88A08188E083","+":4,"T|C16":309,"H|%":61,"O":1} ]
//[ "2016-09-30T08:27:41Z", "", {"@":"FEDA88A08188E083","+":7,"vC|%":0,"T|C16":307} ]
//[ "2016-09-30T08:39:33Z", "", {"@":"FEDA88A08188E083","+":10,"T|C16":305,"H|%":61,"O":1} ]
//[ "2016-09-30T08:55:29Z", "", {"@":"FEDA88A08188E083","+":14,"T|C16":303,"H|%":61,"O":1} ]
//[ "2016-09-30T09:07:37Z", "", {"@":"FEDA88A08188E083","+":1,"gE":0,"T|C16":302,"H|%":61} ]
//[ "2016-09-30T09:11:29Z", "", {"@":"FEDA88A08188E083","+":2,"T|C16":301,"O":1,"L":175} ]
//[ "2016-09-30T09:19:41Z", "", {"@":"FEDA88A08188E083","+":4,"T|C16":301,"H|%":61,"O":1} ]
static const C16DataSample sample1g[] =
    {
{ 0, 6, 31, 20, 331 },
{ 0, 6, 35, 20, 330 },
{ 0, 6, 43, 20, 327, OTRadValve::ModelledRadValveState::MRVE_DRAUGHT },
{ 0, 6, 59, 20, 325 },
{ 0, 7, 7, 20, 324 },
{ 0, 7, 19, 20, 321, OTRadValve::ModelledRadValveState::MRVE_DRAUGHT },
{ 0, 7, 23, 20, 320 },
{ 0, 7, 31, 20, 319 },
//...
{ 0, 8, 15, 20, 309 },
{ 0, 8, 27, 20, 307 },
{ 0, 8, 39, 20, 305 },
{ 0, 8, 55, 20, 303 },
{ 0, 9, 7, 20, 302 },
{ 0, 9, 11, 20, 301 },
{ 0, 9, 19, 20, 301 },
{ }
    };

// TODO: standard driver and test cases from data above!
