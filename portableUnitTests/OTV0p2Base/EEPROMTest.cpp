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
 * Driver for OTV0p2Base EEPROM support routines.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>

#include "OTV0P2BASE_EEPROM.h"


// Test low-wear unary encoding.
//
// DHD20161107: imported from test_SECFRAME.ino testUnaryEncoding().
TEST(EEPROM, UnaryEncoding)
  {
  // Check for conversion back and forth of all allowed represented values.
  for(uint8_t i = 0; i <= 8; ++i) { EXPECT_EQ(i, OTV0P2BASE::eeprom_unary_1byte_decode(OTV0P2BASE::eeprom_unary_1byte_encode(i))); }
  for(uint8_t i = 0; i <= 16; ++i) { EXPECT_EQ(i, OTV0P2BASE::eeprom_unary_2byte_decode(OTV0P2BASE::eeprom_unary_2byte_encode(i))); }
  // Ensure that all-1s (erased) EEPROM values equate to 0.
  EXPECT_EQ(0, OTV0P2BASE::eeprom_unary_1byte_decode(0xff));
//  EXPECT_EQ(0, OTV0P2BASE::eeprom_unary_1byte_decode(0xffffU));
  EXPECT_EQ(0, OTV0P2BASE::eeprom_unary_2byte_decode(0xffffU));
  // Check rejection of at least one bad value.
  EXPECT_EQ(-1, OTV0P2BASE::eeprom_unary_1byte_decode(0xef));
  EXPECT_EQ(-1, OTV0P2BASE::eeprom_unary_2byte_decode(0xccccU));
  }

