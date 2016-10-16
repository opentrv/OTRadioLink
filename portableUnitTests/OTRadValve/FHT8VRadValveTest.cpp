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
 * OTRadValve FHTRadValve tests.
 *
 * Partial, since interactions with hardware (eg radio) hard to portably test.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <OTV0P2BASE_QuickPRNG.h>

#include "OTRadValve_AbstractRadValve.h"
#include "OTRadValve_FHT8VRadValve.h"


// Test (fast) mappings back and forth between [0,100] valve open percentage and [0,255] FS20 representation.
//
// Adapted 2016/10/16 from test_VALVEMODEL.ino testFHT8VPercentage().
TEST(FHT8VRadValve,FHT8VPercentage)
{
    // Test that end-points are mapped correctly from % -> FS20 scale.
    ASSERT_EQ(0, OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(0));
    ASSERT_EQ(255, OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(100));
    // Test that illegal/over values handled sensibly.
    ASSERT_EQ(255, OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(101));
    ASSERT_EQ(255, OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(255));
    // Test that end-points are mapped correctly from FS20 scale to %.
    ASSERT_EQ(0, OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(0));
    ASSERT_EQ(100, OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(255));
    // Test that some critical thresholds are mapped back and forth (round-trip) correctly.
    ASSERT_EQ(OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN, OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN)));
    ASSERT_EQ(OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN, OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN)));
    // Check that all mappings back and forth (round-trip) are reasonably close to target.
    const uint8_t eps = 2; // Tolerance in %
    for(uint8_t i = 0; i <= 100; ++i)
      {
      ASSERT_NEAR(i, OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i)), eps);
      }
    // Check for monotonicity of mapping % -> FS20.
    for(uint8_t i = 0; i < 100; ++i)
      {
      ASSERT_TRUE(
        OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i) <=
        OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i+1));
      }
    // Check for monotonicity of mapping FS20 => %.
    for(uint8_t i = 0; i < 100; ++i)
      {
      ASSERT_TRUE(
        OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i)) <=
        OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i+1)));
      }
    // Check for monotonicity of round-trip.
    for(uint8_t i = 0; i < 100; ++i)
      {
      ASSERT_TRUE(
        OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i) <=
        OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i+1));
      }

}
