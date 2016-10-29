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
 * Driver for OTV0p2Base JSON stats output tests.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>


// Test handling of JSON stats.
//
// Imported 2016/10/29 from Unit_Tests.cpp testJSONStats().
TEST(JSONStats,JSONStats)
{
    OTV0P2BASE::SimpleStatsRotation<2> ss1;
    ss1.setID("1234");
    EXPECT_EQ(0, ss1.size());
    EXPECT_EQ(0, ss1.writeJSON(NULL, OTV0P2BASE::randRNG8(), OTV0P2BASE::randRNG8(), OTV0P2BASE::randRNG8NextBoolean()));

    char buf[OTV0P2BASE::MSG_JSON_MAX_LENGTH + 2]; // Allow for trailing '\0' and spare byte.
    // Create minimal JSON message with no data content. just the (supplied) ID.
    const uint8_t l1 = ss1.writeJSON((uint8_t*)buf, sizeof(buf), OTV0P2BASE::randRNG8(), OTV0P2BASE::randRNG8NextBoolean());
    EXPECT_EQ(12, l1) << buf;
    EXPECT_STREQ(buf, "{\"@\":\"1234\"}") << buf;
    ss1.enableCount(false);
    EXPECT_EQ(12, ss1.writeJSON((uint8_t*)buf, sizeof(buf), OTV0P2BASE::randRNG8(), OTV0P2BASE::randRNG8NextBoolean()));
    EXPECT_STREQ(buf, "{\"@\":\"1234\"}") << buf;
    // Check that count works.
    ss1.enableCount(true);
    EXPECT_EQ(0, ss1.size());
    EXPECT_EQ(18, ss1.writeJSON((uint8_t*)buf, sizeof(buf), OTV0P2BASE::randRNG8(), OTV0P2BASE::randRNG8NextBoolean()));
    //OTV0P2BASE::serialPrintAndFlush(buf); OTV0P2BASE::serialPrintlnAndFlush();
    EXPECT_STREQ(buf, "{\"@\":\"1234\",\"+\":2}");
    // Turn count off for rest of tests.
    ss1.enableCount(false);
    EXPECT_EQ(12, ss1.writeJSON((uint8_t*)buf, sizeof(buf), OTV0P2BASE::randRNG8(), OTV0P2BASE::randRNG8NextBoolean()));
    // Check that removal of absent entry does nothing.
    EXPECT_TRUE(!ss1.remove("bogus"));
    EXPECT_EQ(0, ss1.size());
    // Check that new item can be added/put (with no/default properties).
    ss1.put("f1", 0);
    EXPECT_EQ(1, ss1.size());
    EXPECT_EQ(19, ss1.writeJSON((uint8_t*)buf, sizeof(buf), 0, OTV0P2BASE::randRNG8NextBoolean()));
    EXPECT_STREQ(buf, "{\"@\":\"1234\",\"f1\":0}");
    ss1.put("f1", 42);
    EXPECT_EQ(1, ss1.size());
    EXPECT_EQ(20, ss1.writeJSON((uint8_t*)buf, sizeof(buf), 0, OTV0P2BASE::randRNG8NextBoolean()));
    EXPECT_STREQ(buf, "{\"@\":\"1234\",\"f1\":42}");
    ss1.put("f1", -111);
    EXPECT_EQ(1, ss1.size());
    EXPECT_EQ(22, ss1.writeJSON((uint8_t*)buf, sizeof(buf), 0, OTV0P2BASE::randRNG8NextBoolean()));
    EXPECT_STREQ(buf, "{\"@\":\"1234\",\"f1\":-111}");

    EXPECT_TRUE(OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
}

// Test handling of JSON messages for transmission and reception.
// Includes bit-twiddling, CRC computation, and other error checking.
//
// Imported 2016/10/29 from Unit_Tests.cpp testJSONForTX().
TEST(JSONStats,JSONForTX)
  {
  char buf[OTV0P2BASE::MSG_JSON_MAX_LENGTH + 2]; // Allow for trailing '\0' or CRC + 0xff terminator.
  // Clear the buffer.
  memset(buf, 0, sizeof(buf));
  // Fail sanity check on a completely empty buffer (zero-length string).
  EXPECT_TRUE(!OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  // Fail sanity check on a few initially-plausible length-1 values.
  buf[0] = '{';
  EXPECT_TRUE(!OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  buf[0] = '}';
  EXPECT_TRUE(!OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  buf[0] = '[';
  EXPECT_TRUE(!OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  buf[0] = ']';
  EXPECT_TRUE(!OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  buf[0] = ' ';
  EXPECT_TRUE(!OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  // Fail sanity check with already-adjusted (minimal) nessage.
  buf[0] = '{';
  buf[1] = ('}' | 0x80);
  EXPECT_TRUE(!OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  // Minimal correct messaage should pass.
  buf[0] = '{';
  buf[1] = '}';
  EXPECT_TRUE(OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  // Try a longer valid trivial message.
  strcpy(buf, "{  }");
  EXPECT_TRUE(OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  // Invalidate it with a non-printable char and check that it is rejected.
  buf[2] = '\1';
  EXPECT_TRUE(!OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  // Try a longer valid non-trivial message.
  const char * longJSONMsg1 = "{\"@\":\"cdfb\",\"T|C16\":299,\"H|%\":83,\"L\":255,\"B|cV\":256}";
  memset(buf, 0, sizeof(buf));
  strcpy(buf, longJSONMsg1);
  EXPECT_TRUE(OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  // Invalidate it with a high-bit set and check that it is rejected.
  buf[5] |= 0x80;
  EXPECT_TRUE(!OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));
  // CRC fun!
  memset(buf, 0, sizeof(buf));
  buf[0] = '{';
  buf[1] = '}';
  const uint8_t crc1 = OTV0P2BASE::adjustJSONMsgForTXAndComputeCRC(buf);
  // Check that top bit is not set (ie CRC was computed OK).
  EXPECT_TRUE(!(crc1 & 0x80));
  // Check for expected CRC value.
  EXPECT_TRUE(0x38 == crc1);
  // Check that initial part unaltered.
  EXPECT_TRUE('{' == buf[0]);
  // Check that top bit has been set in trailing brace.
  EXPECT_TRUE((char)('}' | 0x80) == buf[1]);
  // Check that trailing '\0' still present.
  EXPECT_TRUE(0 == buf[2]);
  // Check that TX-format can be converted for RX.
  buf[2] = crc1;
  buf[3] = 0xff; // As for normal TX...
// FIXME
//  const int8_t l1 = adjustJSONMsgForRXAndCheckCRC(buf, sizeof(buf));
//  AssertIsTrueWithErr(2 == l1, l1);
//  AssertIsTrueWithErr(2 == strlen(buf), strlen(buf));
//  AssertIsTrue(quickValidateRawSimpleJSONMessage(buf));
  // Now a longer message...
  memset(buf, 0, sizeof(buf));
  strcpy(buf, longJSONMsg1);
  const uint8_t crc2 = OTV0P2BASE::adjustJSONMsgForTXAndComputeCRC(buf);
  // Check that top bit is not set (ie CRC was computed OK).
  EXPECT_TRUE(!(crc2 & 0x80));
  // Check for expected CRC value.
  EXPECT_TRUE(0x77 == crc2);
// FIXME
//  // Check that TX-format can be converted for RX.
//  const int l2o = strlen(buf);
//  buf[l2o] = crc2;
//  buf[l2o+1] = 0xff;
// FIXME
//  const int8_t l2 = adjustJSONMsgForRXAndCheckCRC(buf, sizeof(buf));
//  AssertIsTrueWithErr(l2o == l2, l2);
//  AssertIsTrue(quickValidateRawSimpleJSONMessage(buf));
  }
