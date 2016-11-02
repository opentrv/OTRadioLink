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
 * Driver for OTV0p2Base by-hour stats tests.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>

// Test handling of ByHourByteStats stats.
//
// Verify that the simple smoothing function never generates an out of range value.
// In particular, with a legitimate value range of [0,254]
// smoothStatsValue() must never generate 255 (0xff) which looks like an uninitialised EEPROM value,
// nor wrap around in either direction.
//
// Imported 2016/10/29 from Unit_Tests.cpp testSmoothStatsValue().
TEST(ByHourByteStats,SmoothStatsValue)
{
    // Covers the key cases 0 and 254 in particular.
    for(int i = 256; --i >= 0; )
        { EXPECT_EQ(i, OTV0P2BASE::NVByHourByteStatsBase::smoothStatsValue((uint8_t)i, (uint8_t)i)); }

}
