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
 * Driver for OTV0p2Base by-hour and related stats tests.
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

// Test some basic behaviour of the support/calc routines on emoty stats container.
// In particular exercises failure paths as there are no valid stats sets.
TEST(Stats, empty)
{
    // Seed random() for use in simulator; --gtest_shuffle will force it to change.
    srandom((unsigned) ::testing::UnitTest::GetInstance()->random_seed());

    // On a dummy (no-stats) impl, all support functions should give 'not-set' / error results.
    OTV0P2BASE::NULLByHourByteStats ns;
    const uint8_t statsSet = 0; // Should be arbitrary.
    const uint8_t unset = OTV0P2BASE::NVByHourByteStatsBase::UNSET_BYTE;
    EXPECT_FALSE(ns.inBottomQuartile(statsSet, 0));
    EXPECT_FALSE(ns.inBottomQuartile(statsSet, 0xff & random()));
    EXPECT_FALSE(ns.inBottomQuartile(statsSet, unset));
    EXPECT_FALSE(ns.inTopQuartile(statsSet, 0));
    EXPECT_FALSE(ns.inTopQuartile(statsSet, 0xff & random()));
    EXPECT_FALSE(ns.inTopQuartile(statsSet, unset));
    EXPECT_FALSE(ns.inOutlierQuartile(true, statsSet));
    EXPECT_EQ(unset, ns.getMinByHourStat(statsSet));
    EXPECT_EQ(unset, ns.getMaxByHourStat(statsSet));
    EXPECT_EQ(0, ns.countStatSamplesBelow(statsSet, 0));
    EXPECT_EQ(0, ns.countStatSamplesBelow(statsSet, 0xff & random()));
    EXPECT_EQ(0, ns.countStatSamplesBelow(statsSet, unset));

    // By-hour values.
    for(uint8_t hh = 0; hh < 24; ++hh)
        {
        EXPECT_EQ(unset, ns.getByHourStatSimple(statsSet, hh));
        EXPECT_EQ(unset, ns.getByHourStatRTC(statsSet, hh));
        EXPECT_FALSE(ns.inOutlierQuartile(true, statsSet, hh));
        EXPECT_FALSE(ns.inOutlierQuartile(false, statsSet, hh));
        }
}

// Test some basic behaviour of the support/calc routines mock r/w stats containet.
TEST(Stats, mockRW)
{
    // Seed random() for use in simulator; --gtest_shuffle will force it to change.
    srandom((unsigned) ::testing::UnitTest::GetInstance()->random_seed());

    // On a dummy (no-stats) impl, all support functions should give 'not-set' / error results.
    OTV0P2BASE::NVByHourByteStatsMock ms;
    const uint8_t statsSet = 0; // Should be arbitrary.
    const uint8_t unset = OTV0P2BASE::NVByHourByteStatsBase::UNSET_BYTE;
    EXPECT_FALSE(ms.inBottomQuartile(statsSet, 0));
    EXPECT_FALSE(ms.inBottomQuartile(statsSet, 0xff & random()));
    EXPECT_FALSE(ms.inBottomQuartile(statsSet, unset));
    EXPECT_FALSE(ms.inTopQuartile(statsSet, 0));
    EXPECT_FALSE(ms.inTopQuartile(statsSet, 0xff & random()));
    EXPECT_FALSE(ms.inTopQuartile(statsSet, unset));
    EXPECT_FALSE(ms.inOutlierQuartile(true, statsSet));
    EXPECT_EQ(unset, ms.getMinByHourStat(statsSet));
    EXPECT_EQ(unset, ms.getMaxByHourStat(statsSet));
    EXPECT_EQ(0, ms.countStatSamplesBelow(statsSet, 0));
    EXPECT_EQ(0, ms.countStatSamplesBelow(statsSet, 0xff & random()));
    EXPECT_EQ(0, ms.countStatSamplesBelow(statsSet, unset));

    // By-hour values.
    for(uint8_t hh = 0; hh < 24; ++hh)
        {
        EXPECT_EQ(unset, ms.getByHourStatSimple(statsSet, hh));
        EXPECT_EQ(unset, ms.getByHourStatRTC(statsSet, hh));
        EXPECT_FALSE(ms.inOutlierQuartile(true, statsSet, hh));
        EXPECT_FALSE(ms.inOutlierQuartile(false, statsSet, hh));
        }
}

// Trivial read-only implementation that returns hour value in each slot with getByHourStatSimple().
// Enough to test some stats against.
class HByHourByteStats final : public OTV0P2BASE::NVByHourByteStatsBase
  {
  public:
    virtual bool zapStats(uint16_t = 0) override { return(true); } // No stats to erase, so all done.
    virtual uint8_t getByHourStatSimple(uint8_t, uint8_t h) const override { return(h); }
    virtual void setByHourStatSimple(uint8_t, const uint8_t, uint8_t = UNSET_BYTE) override { }
    virtual uint8_t getByHourStatRTC(uint8_t, uint8_t = 0xff) const override { return(UNSET_BYTE); }
  };

// Test some basic behaviour of the support/calc routines on simple data sets
TEST(Stats, moreCalcs)
{
    // Seed random() for use in simulator; --gtest_shuffle will force it to change.
    srandom((unsigned) ::testing::UnitTest::GetInstance()->random_seed());

    // On a dummy (no-stats) impl, all support functions should give 'not-set' / error results.
    HByHourByteStats hs;
    const uint8_t statsSet = 0; // Should be arbitrary.
    const uint8_t unset = OTV0P2BASE::NVByHourByteStatsBase::UNSET_BYTE;
    EXPECT_TRUE(hs.inBottomQuartile(statsSet, 0));
    EXPECT_FALSE(hs.inBottomQuartile(statsSet, 23));
    EXPECT_FALSE(hs.inBottomQuartile(statsSet, unset));
    EXPECT_FALSE(hs.inTopQuartile(statsSet, 0));
    EXPECT_TRUE(hs.inTopQuartile(statsSet, 23));
    EXPECT_FALSE(hs.inTopQuartile(statsSet, unset));
    EXPECT_FALSE(hs.inOutlierQuartile(true, statsSet));
    EXPECT_EQ(0, hs.getMinByHourStat(statsSet));
    EXPECT_EQ(23, hs.getMaxByHourStat(statsSet));
    EXPECT_EQ(0, hs.countStatSamplesBelow(statsSet, 0));
    EXPECT_EQ(24, hs.countStatSamplesBelow(statsSet, 24));
    EXPECT_EQ(24, hs.countStatSamplesBelow(statsSet, unset));

    // By-hour values.
    for(uint8_t hh = 0; hh < 24; ++hh)
        {
        EXPECT_EQ(hh, hs.getByHourStatSimple(statsSet, hh));
//EXPECT_EQ(unset, hs.getByHourStatRTC(statsSet, hh)); // Not usefully defined.
        EXPECT_EQ(hh > 17, hs.inTopQuartile(statsSet, hh));
        EXPECT_EQ(hh < 6, hs.inBottomQuartile(statsSet, hh));
        EXPECT_EQ(hh > 17, hs.inOutlierQuartile(true, statsSet, hh));
        EXPECT_EQ(hh < 6, hs.inOutlierQuartile(false, statsSet, hh));
        }
}

// Test stats updater can be constructed and defaults as expected
namespace BHSSU
    {
    HByHourByteStats hs;
    OTV0P2BASE::SensorAmbientLightMock ambLight;
    OTV0P2BASE::ByHourSimpleStatsUpdaterSampleStats <
      decltype(BHSSU::hs), &BHSSU::hs,
      decltype(BHSSU::ambLight), &BHSSU::ambLight
      > su;
    }
TEST(Stats, ByHourSimpleStatsUpdater)
{
      static_assert(2 == BHSSU::su.maxSamplesPerHour, "constant must propagate correctly");
      const uint8_t msph = BHSSU::su.maxSamplesPerHour;
      ASSERT_EQ(2, msph);
      BHSSU::su.sampleStats(false, 0);
      BHSSU::su.sampleStats(true, 0);
}




// Test temperature companding for non-volatile storage.
//
// 20161016 moved from OpenTRV-Arduino-V0p2 Unit_Tests.cpp testTempCompand().
TEST(Stats,TempCompand)
{
  // Ensure that all (whole) temperatures from 0C to 100C are correctly compressed and expanded.
  for(int16_t i = 0; i <= 100; ++i)
    {
    //DEBUG_SERIAL_PRINT(i<<4); DEBUG_SERIAL_PRINT(" => "); DEBUG_SERIAL_PRINT(compressTempC16(i<<4)); DEBUG_SERIAL_PRINT(" => "); DEBUG_SERIAL_PRINT(expandTempC16(compressTempC16(i<<4))); DEBUG_SERIAL_PRINTLN();
    ASSERT_EQ(i<<4, OTV0P2BASE::expandTempC16(OTV0P2BASE::compressTempC16(int16_t(i<<4))));
    }
  // Ensure that out-of-range inputs are coerced to the limits.
  ASSERT_EQ(0, OTV0P2BASE::expandTempC16(OTV0P2BASE::compressTempC16(-1)));
  ASSERT_EQ((100<<4), OTV0P2BASE::expandTempC16(OTV0P2BASE::compressTempC16(101<<4)));
  ASSERT_EQ(OTV0P2BASE::COMPRESSION_C16_CEIL_VAL_AFTER, OTV0P2BASE::compressTempC16(102<<4)); // Verify ceiling.
  ASSERT_LT(OTV0P2BASE::COMPRESSION_C16_CEIL_VAL_AFTER, 0xff);
  // Ensure that 'unset' compressed value expands to 'unset' uncompressed value.
  const int16_t ui = OTV0P2BASE::NVByHourByteStatsBase::UNSET_INT;
  const uint8_t ub = OTV0P2BASE::NVByHourByteStatsBase::UNSET_BYTE;
  ASSERT_EQ(ui, OTV0P2BASE::expandTempC16(ub));
}
