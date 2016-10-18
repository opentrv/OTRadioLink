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
 * Driver for minor OTV0p2Base tests.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>

#include "OTV0P2BASE_EEPROM.h"


// Sanity test.
// Minimally test a real library function.
// Tests some simple parsing functions.
TEST(OTV0p2Base,parseHex)
{
    const char s[] = "0a";
    // This works. It's inline and only in the header.
    EXPECT_EQ(10, OTV0P2BASE::parseHexDigit(s[1]));
    // The compiler can't find this for some reason (function def in source file).
    EXPECT_EQ(10, OTV0P2BASE::parseHexByte(s));
}


// Test temperature companding for non-volatile storage.
//
// 20161016 moved from OpenTRV-Arduino-V0p2 Unit_Tests.cpp testTempCompand().
TEST(OTV0p2Base,TempCompand)
{
  // Ensure that all (whole) temperatures from 0C to 100C are correctly compressed and expanded.
  for(int i = 0; i <= 100; ++i)
    {
    //DEBUG_SERIAL_PRINT(i<<4); DEBUG_SERIAL_PRINT(" => "); DEBUG_SERIAL_PRINT(compressTempC16(i<<4)); DEBUG_SERIAL_PRINT(" => "); DEBUG_SERIAL_PRINT(expandTempC16(compressTempC16(i<<4))); DEBUG_SERIAL_PRINTLN();
    ASSERT_EQ(i<<4, OTV0P2BASE::expandTempC16(OTV0P2BASE::compressTempC16(i<<4)));
    }
  // Ensure that out-of-range inputs are coerced to the limits.
  ASSERT_EQ(0, OTV0P2BASE::expandTempC16(OTV0P2BASE::compressTempC16(-1)));
  ASSERT_EQ((100<<4), OTV0P2BASE::expandTempC16(OTV0P2BASE::compressTempC16(101<<4)));
  ASSERT_EQ(OTV0P2BASE::COMPRESSION_C16_CEIL_VAL_AFTER, OTV0P2BASE::compressTempC16(102<<4)); // Verify ceiling.
  ASSERT_LT(OTV0P2BASE::COMPRESSION_C16_CEIL_VAL_AFTER, 0xff);
  // Ensure that 'unset' compressed value expands to 'unset' uncompressed value.
  ASSERT_EQ(OTV0P2BASE::STATS_UNSET_INT, OTV0P2BASE::expandTempC16(OTV0P2BASE::STATS_UNSET_BYTE));
}
