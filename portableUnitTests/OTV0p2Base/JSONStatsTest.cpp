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
    ss1.setID(V0p2_SENSOR_TAG_F("1234"));
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
    EXPECT_FALSE(ss1.remove(V0p2_SENSOR_TAG_F("bogus")));
    EXPECT_EQ(0, ss1.size());
    // Check that new item can be added/put (with no/default properties).
    ss1.put(V0p2_SENSOR_TAG_F("f1"), 0);
    EXPECT_EQ(1, ss1.size());
    EXPECT_EQ(19, ss1.writeJSON((uint8_t*)buf, sizeof(buf), 0, OTV0P2BASE::randRNG8NextBoolean()));
    EXPECT_STREQ(buf, "{\"@\":\"1234\",\"f1\":0}");
    ss1.put(V0p2_SENSOR_TAG_F("f1"), 42);
    EXPECT_EQ(1, ss1.size());
    EXPECT_EQ(20, ss1.writeJSON((uint8_t*)buf, sizeof(buf), 0, OTV0P2BASE::randRNG8NextBoolean()));
    EXPECT_STREQ(buf, "{\"@\":\"1234\",\"f1\":42}");
    ss1.put(V0p2_SENSOR_TAG_F("f1"), -111);
    EXPECT_EQ(1, ss1.size());
    EXPECT_EQ(22, ss1.writeJSON((uint8_t*)buf, sizeof(buf), 0, OTV0P2BASE::randRNG8NextBoolean()));
    EXPECT_STREQ(buf, "{\"@\":\"1234\",\"f1\":-111}");
    EXPECT_TRUE(OTV0P2BASE::quickValidateRawSimpleJSONMessage(buf));

    // Check that removal of absent entry does nothing.
    EXPECT_TRUE(ss1.remove(V0p2_SENSOR_TAG_F("f1")));
    EXPECT_EQ(0, ss1.size());

    // Check setting direct with Sensor.
    OTV0P2BASE::SensorAmbientLightMock alm;
    ss1.put(alm);
    EXPECT_EQ(1, ss1.size());
    EXPECT_EQ(18, ss1.writeJSON((uint8_t*)buf, sizeof(buf), 0, OTV0P2BASE::randRNG8NextBoolean()));
    EXPECT_STREQ(buf, "{\"@\":\"1234\",\"L\":0}");

    // Check ID suppression.
    ss1.setID(V0p2_SENSOR_TAG_F(""));
    EXPECT_EQ(7, ss1.writeJSON((uint8_t*)buf, sizeof(buf), 0, OTV0P2BASE::randRNG8NextBoolean()));
    EXPECT_STREQ(buf, "{\"L\":0}");
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
  buf[1] = char('}' | 0x80);
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
  buf[2] = char(crc1);
  buf[3] = char(0xff); // As for normal TX...
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


// This is used to size the stats generator and consistently extract values for it.
// At least one sensor must be provided.
template<typename T1, typename ... Ts>
class JSONStatsHolder final
  {
  private:
    std::tuple<T1, Ts...> args;

  public:
    // Number of arguments/stats.
    static constexpr size_t argCount = 1 + sizeof...(Ts);

    // JSON generator.
    typedef OTV0P2BASE::SimpleStatsRotation<argCount> ss_t;
    ss_t ss;

    // Construct an instance; though the template helper function is usually easier.
    template <typename... Args>
    constexpr JSONStatsHolder(Args&&... args) : args(std::forward<Args>(args)...)
      { static_assert(sizeof...(Args) > 0, "must take at least one arg"); }

  private:
    // Many thanks for template wrangling examples to David Godfrey and others:
    //     http://stackoverflow.com/questions/16868129/how-to-store-variadic-template-arguments
    //     http://stackoverflow.com/questions/8992853/terminating-function-template-recursion
    template<std::size_t> struct Int2Type { };
    // Put...
    template<typename ... Args> bool putOrRemove(Int2Type<0>, std::tuple<Args...>& tup)
        { return(ss.putOrRemove(std::get<0>(tup))); }
    template<size_t I, typename ... Args> bool putOrRemove(Int2Type<I>, std::tuple<Args...>& tup)
        { return(putOrRemove(Int2Type<I-1>(), tup) && ss.putOrRemove(std::get<I>(tup))); }
    // Read...
    template<typename ... Args> void read(Int2Type<0>, std::tuple<Args...>& tup)
        { return((std::get<0>(tup)).read()); }
    template<size_t I, typename ... Args> void read(Int2Type<I>, std::tuple<Args...>& tup)
        { read(Int2Type<I-1>(), tup); (std::get<I>(tup)).read(); }

  public:
    // Call read() on all sensors; usually done once, at initialisation.
    void readAll() { read(args); }
    // Put all the attached isAvailable() sensor values into the stats object; remove those !isAvailable().
    bool putOrRemoveAll() { return(putOrRemove(Int2Type<argCount-1>(), args)); }
  };

// Helper to avoid having to spell out the types explicitly.
template <typename... Args>
constexpr JSONStatsHolder<Args...> makeJSONStatsHolder(Args&&... args)
    { return(JSONStatsHolder<Args...>(std::forward<Args>(args)...)); }

// Testing simplified argument passing and stats object sizing.
TEST(JSONStats,VariadicJSON1)
{
    static OTV0P2BASE::HumiditySensorMock RelHumidity;
    static auto ssh1 = makeJSONStatsHolder(RelHumidity);
    auto &ss1 = ssh1.ss;
    const uint8_t c1 = ss1.getCapacity();
    EXPECT_EQ(1, c1);
    // Suppression the ID.
    ss1.setID(V0p2_SENSOR_TAG_F(""));
    // Disable the counter.
    ss1.enableCount(false);
    // Set the sensor to a known value.
    RelHumidity.set(0);
    char buf[OTV0P2BASE::MSG_JSON_MAX_LENGTH + 2]; // Allow for trailing '\0' and spare byte.
    // No sensor data so stats should be empty.
    const uint8_t l0 = ss1.writeJSON((uint8_t*)buf, sizeof(buf), OTV0P2BASE::randRNG8());
    EXPECT_EQ(2, l0) << buf;
    EXPECT_STREQ(buf, "{}") << buf;
    EXPECT_TRUE(ss1.isEmpty());
    // Write sensor values to the stats.
    EXPECT_EQ(0, ss1.size());
    ASSERT_TRUE(ssh1.putOrRemoveAll()) << "all operations must succeed";
    EXPECT_EQ(1, ss1.size());
    // Create minimal JSON message with just the sensor data.
    const uint8_t l1 = ss1.writeJSON((uint8_t*)buf, sizeof(buf), OTV0P2BASE::randRNG8());
    EXPECT_EQ(9, l1) << buf;
    EXPECT_STREQ(buf, "{\"H|%\":0}") << buf;
}

// Testing simplified argument passing and stats object sizing.
TEST(JSONStats,VariadicJSON2)
{
    static OTV0P2BASE::HumiditySensorMock RelHumidity;
    static OTV0P2BASE::SensorAmbientLightMock AmbLight;
    static auto ssh2 = makeJSONStatsHolder(AmbLight, RelHumidity);
    auto &ss2 = ssh2.ss;
    const uint8_t c1 = ss2.getCapacity();
    EXPECT_EQ(2, c1);
    // Suppression the ID.
    ss2.setID(V0p2_SENSOR_TAG_F(""));
    // Disable the counter.
    ss2.enableCount(false);
    // Set the sensor to a known value.
    RelHumidity.set(0);
    AmbLight.set(0);
    char buf[OTV0P2BASE::MSG_JSON_MAX_LENGTH + 2]; // Allow for trailing '\0' and spare byte.
    // No sensor data so stats should be empty.
    const uint8_t l0 = ss2.writeJSON((uint8_t*)buf, sizeof(buf), OTV0P2BASE::randRNG8());
    EXPECT_EQ(2, l0) << buf;
    EXPECT_STREQ(buf, "{}") << buf;
    // Write sensor values to the stats.
    EXPECT_TRUE(ss2.isEmpty());
    EXPECT_EQ(0, ss2.size());
    ASSERT_TRUE(ssh2.putOrRemoveAll()) << "all operations must succeed";
    EXPECT_EQ(2, ss2.size());
    // Create minimal JSON message with just the sensor data.
    const uint8_t l1 = ss2.writeJSON((uint8_t*)buf, sizeof(buf), OTV0P2BASE::randRNG8(), true);
    EXPECT_EQ(15, l1) << buf;
    EXPECT_TRUE((0 == strcmp(buf, "{\"H|%\":0,\"L\":0}")) || (0 == strcmp(buf, "{\"L\":0,\"H|%\":0}")));
}
