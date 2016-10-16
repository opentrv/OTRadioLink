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

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>
#include "OTRadValve_ModelledRadValve.h"


// Test for general sanity of computation of desired valve position.
// In particular test the logic in ModelledRadValveState for starting from extreme positions.
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
        rs1.tick(valvePCOpen, is1);
        const uint8_t newValvePos = valvePCOpen;
        ASSERT_TRUE(rs1.initialised); // Initialisation must have completed.
        ASSERT_TRUE(newValvePos < 100);
        //    AssertIsTrue(newValvePos >= 0);
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
                ASSERT_TRUE((offset < 0) ? (valvePCOpen > 0) : (0 == valvePCOpen));
                // Where adjusted reference temperature is (well) above target, valve should be driven off.
                OTRadValve::ModelledRadValveState rs3b;
                valvePCOpen = 100;
                rs3b.tick(valvePCOpen, is3);
                ASSERT_TRUE((offset < 0) ? (100 == valvePCOpen) : (valvePCOpen < 100));
                }
            else
                {
                // Below the half way mark the valve should always be opened (from off), soft setback or not.
                is3.refTempC16 = (is3.targetTempC << 4) + 0x4;
                OTRadValve::ModelledRadValveState rs3c;
                valvePCOpen = 0;
                rs3c.tick(valvePCOpen, is3);
                ASSERT_TRUE(valvePCOpen > 0);
                // Above the half way mark the valve should only be opened without soft setback.
                is3.refTempC16 = (is3.targetTempC << 4) + 0xc;
                OTRadValve::ModelledRadValveState rs3d;
                valvePCOpen = 0;
                rs3d.tick(valvePCOpen, is3);
                ASSERT_TRUE(0 == valvePCOpen);
                }
            }
        }
}
