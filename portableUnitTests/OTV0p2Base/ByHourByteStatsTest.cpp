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

    // New emoty container.
    OTV0P2BASE::NVByHourByteStatsMock ms;

    // Pick a random hour to treat as 'now'.
    const uint8_t hourNow = ((unsigned) random()) % 24;
    ms._setHour(hourNow);

    const uint8_t unset = OTV0P2BASE::NVByHourByteStatsBase::UNSET_BYTE;

    // On a fresh/empty stats container, all support functions should give 'not-set' / error results.
    for(uint8_t statsSet = 0; statsSet < OTV0P2BASE::NVByHourByteStatsBase::STATS_SETS_COUNT; ++statsSet)
        {
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

    // Pick a stats set to work on at random.
    const uint8_t statsSet = ((unsigned) random()) % OTV0P2BASE::NVByHourByteStatsBase::STATS_SETS_COUNT;
    // Check that when a single value is set,
    // it is seen as expected by the simple accessors.
    ms.setByHourStatSimple(statsSet, hourNow, 0);
    EXPECT_EQ(0, ms.getByHourStatSimple(statsSet, hourNow));
    EXPECT_EQ(0, ms.getByHourStatRTC(statsSet, hourNow));
    EXPECT_EQ(0, ms.getByHourStatRTC(statsSet, OTV0P2BASE::NVByHourByteStatsBase::SPECIAL_HOUR_CURRENT_HOUR));
    EXPECT_EQ(unset, ms.getByHourStatRTC(statsSet, OTV0P2BASE::NVByHourByteStatsBase::SPECIAL_HOUR_NEXT_HOUR));
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

// Test that stats updater can be constructed and defaults as expected.
namespace BHSSUBasics
    {
    HByHourByteStats hs;
    OTV0P2BASE::PseudoSensorOccupancyTracker occupancy;
    OTV0P2BASE::SensorAmbientLightAdaptiveMock ambLight;
    OTV0P2BASE::TemperatureC16Mock tempC16;
    OTV0P2BASE::HumiditySensorMock rh;
    OTV0P2BASE::ByHourSimpleStatsUpdaterSampleStats <
      decltype(hs), &hs,
      decltype(occupancy), &occupancy,
      decltype(ambLight), &ambLight,
      decltype(tempC16), &tempC16,
      decltype(rh), &rh,
      1
      > su;
    }
TEST(Stats, ByHourSimpleStatsUpdaterBasics)
{
      static_assert(1 == BHSSUBasics::su.maxSamplesPerHour, "constant must propagate correctly");
      const uint8_t msph = BHSSUBasics::su.maxSamplesPerHour;
      ASSERT_EQ(1, msph);
      BHSSUBasics::su.sampleStats(false, 0);
      BHSSUBasics::su.sampleStats(true, 0);
}

// Test that stats updater can be constructed and can be updated.
namespace BHSSU
    {
    OTV0P2BASE::NVByHourByteStatsMock ms;
    OTV0P2BASE::PseudoSensorOccupancyTracker occupancy;
    OTV0P2BASE::SensorAmbientLightAdaptiveMock ambLight;
    OTV0P2BASE::TemperatureC16Mock tempC16;
    OTV0P2BASE::HumiditySensorMock rh;
    OTV0P2BASE::ByHourSimpleStatsUpdaterSampleStats <
      decltype(ms), &ms,
      decltype(occupancy), &occupancy,
      decltype(ambLight), &ambLight,
      decltype(tempC16), &tempC16,
      decltype(rh), &rh,
      2
      > su;
    }
TEST(Stats, ByHourSimpleStatsUpdater)
{
    // Seed random() for use in simulator; --gtest_shuffle will force it to change.
    srandom((unsigned) ::testing::UnitTest::GetInstance()->random_seed());

    static_assert(2 == BHSSU::su.maxSamplesPerHour, "constant must propagate correctly");
    const uint8_t msph = BHSSU::su.maxSamplesPerHour;
    ASSERT_EQ(2, msph);

    // Reset static state to make tests re-runnable.
//    BHSSU::su.sampleStats(true, 0);
    BHSSU::ms.zapStats();
    BHSSU::occupancy.reset();
    BHSSU::ambLight.set(0, 0, false);
    BHSSU::tempC16.set(OTV0P2BASE::TemperatureC16Mock::DEFAULT_INVALID_TEMP);
    BHSSU::rh.set(0, false);

    const uint8_t unset = OTV0P2BASE::NVByHourByteStatsBase::UNSET_BYTE;

    // Set (arbitrary) initial time.
    uint8_t hourNow = ((unsigned) random()) % 24;
    BHSSU::ms._setHour(hourNow);
    // Set initial sensor values.
    const uint8_t al0 = 254;
    BHSSU::ambLight.set(al0);
    const int16_t t0 = 18 << 4;
    const uint8_t t0c = OTV0P2BASE::compressTempC16(t0);
    ASSERT_NEAR(t0, OTV0P2BASE::expandTempC16(t0c), 1);
    BHSSU::tempC16.set(t0);
    ASSERT_EQ(18<<4, BHSSU::tempC16.get());
    const uint8_t rh0 = ((unsigned) random()) % 101;
    BHSSU::rh.set(rh0);
    BHSSU::su.sampleStats(true, hourNow);
    const uint8_t o0 = 0;
    ASSERT_EQ(o0, BHSSU::occupancy.get());
    // Verify that after first full update sensor values set to specified values.
    EXPECT_EQ(al0, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, hourNow));
    EXPECT_EQ(al0, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, hourNow));
    EXPECT_EQ(al0, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, hourNow));
    EXPECT_EQ(al0, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, hourNow));
    EXPECT_EQ(al0, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, BHSSU::ms.SPECIAL_HOUR_CURRENT_HOUR));
    EXPECT_EQ(al0, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, BHSSU::ms.SPECIAL_HOUR_CURRENT_HOUR));
    EXPECT_EQ(unset, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, BHSSU::ms.SPECIAL_HOUR_NEXT_HOUR));
    EXPECT_EQ(unset, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, BHSSU::ms.SPECIAL_HOUR_NEXT_HOUR));
    EXPECT_EQ(t0c, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_TEMP_BY_HOUR, hourNow));
    EXPECT_EQ(t0c, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_TEMP_BY_HOUR_SMOOTHED, hourNow));
    EXPECT_EQ(rh0, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_RHPC_BY_HOUR, hourNow));
    EXPECT_EQ(rh0, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_RHPC_BY_HOUR_SMOOTHED, hourNow));
    EXPECT_EQ(o0, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_OCCPC_BY_HOUR, hourNow));
    EXPECT_EQ(o0, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_OCCPC_BY_HOUR_SMOOTHED, hourNow));

    // Nominally roll round a day and update the same slot for new sensor values.
    const uint8_t al1 = 0;
    BHSSU::ambLight.set(al1);
    const uint8_t o1 = 100;
    BHSSU::occupancy.markAsOccupied();
    ASSERT_EQ(o1, BHSSU::occupancy.get());
    const uint8_t rh1 = ((unsigned) random()) % 101;
    BHSSU::rh.set(rh1);
    // Compute expected (approximate) smoothed values.
    const uint8_t sm_al1 = OTV0P2BASE::NVByHourByteStatsBase::smoothStatsValue(al0, al1);
    EXPECT_LT(al1, sm_al1);
    EXPECT_GT(al0, sm_al1);
    const uint8_t sm_o1 = OTV0P2BASE::NVByHourByteStatsBase::smoothStatsValue(o0, o1);
    const uint8_t sm_rh1 = OTV0P2BASE::NVByHourByteStatsBase::smoothStatsValue(rh0, rh1);
    // Take single/final/full sample.
    BHSSU::su.sampleStats(true, hourNow);
    EXPECT_EQ(al1, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, hourNow));
    EXPECT_NEAR(sm_al1, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, hourNow), 1);
    EXPECT_EQ(al1, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, hourNow));
    EXPECT_NEAR(sm_al1, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, hourNow), 1);
    EXPECT_EQ(al1, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, BHSSU::ms.SPECIAL_HOUR_CURRENT_HOUR));
    EXPECT_NEAR(sm_al1, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, BHSSU::ms.SPECIAL_HOUR_CURRENT_HOUR), 1);
    EXPECT_EQ(unset, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, BHSSU::ms.SPECIAL_HOUR_NEXT_HOUR));
    EXPECT_EQ(unset, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, BHSSU::ms.SPECIAL_HOUR_NEXT_HOUR));
    EXPECT_EQ(t0c, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_TEMP_BY_HOUR, hourNow));
    EXPECT_EQ(t0c, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_TEMP_BY_HOUR_SMOOTHED, hourNow));
    EXPECT_EQ(rh1, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_RHPC_BY_HOUR, hourNow));
    EXPECT_NEAR(sm_rh1, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_RHPC_BY_HOUR_SMOOTHED, hourNow), 1);
    EXPECT_EQ(o1, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_OCCPC_BY_HOUR, hourNow));
    EXPECT_NEAR(sm_o1, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_OCCPC_BY_HOUR_SMOOTHED, hourNow), 1);

    // Move to next hour.
    const uint8_t nextHour = (hourNow + 1) % 24;
    BHSSU::ms._setHour(nextHour);
    // Take a couple of samples for this hour.
    BHSSU::ambLight.set(al0);
    BHSSU::rh.set(rh0);
    BHSSU::su.sampleStats(false, nextHour);
    BHSSU::ambLight.set(al1);
    BHSSU::rh.set(rh1);
    BHSSU::su.sampleStats(true, nextHour);
    // Expect to see the mean of the first and second sample as the stored value.
    // Note that this exercises sensor data that is handled differently (eg full-range AmbLight vs RH%).
    const uint8_t al01 = (al0 + al1 + 1) / 2;
    const uint8_t rh01 = (rh0 + rh1 + 1) / 2;
    EXPECT_EQ(al01, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, nextHour));
    EXPECT_EQ(al01, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, nextHour));
    EXPECT_EQ(al01, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, nextHour));
    EXPECT_EQ(al01, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, nextHour));
    EXPECT_EQ(al01, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, BHSSU::ms.SPECIAL_HOUR_CURRENT_HOUR));
    EXPECT_EQ(al01, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, BHSSU::ms.SPECIAL_HOUR_CURRENT_HOUR));
    EXPECT_EQ(unset, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, BHSSU::ms.SPECIAL_HOUR_NEXT_HOUR));
    EXPECT_EQ(unset, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, BHSSU::ms.SPECIAL_HOUR_NEXT_HOUR));
    EXPECT_EQ(t0c, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_TEMP_BY_HOUR, nextHour));
    EXPECT_EQ(t0c, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_TEMP_BY_HOUR_SMOOTHED, nextHour));
    EXPECT_EQ(rh01, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_RHPC_BY_HOUR, nextHour));
    EXPECT_EQ(rh01, BHSSU::ms.getByHourStatSimple(BHSSU::ms.STATS_SET_RHPC_BY_HOUR_SMOOTHED, nextHour));
    EXPECT_EQ(rh01, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_RHPC_BY_HOUR, nextHour));
    EXPECT_EQ(rh01, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_RHPC_BY_HOUR_SMOOTHED, nextHour));
    EXPECT_EQ(rh01, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_RHPC_BY_HOUR, BHSSU::ms.SPECIAL_HOUR_CURRENT_HOUR));
    EXPECT_EQ(rh01, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_RHPC_BY_HOUR_SMOOTHED, BHSSU::ms.SPECIAL_HOUR_CURRENT_HOUR));

    // Nominally roll round a day and check the first slot's values via the RTC view.
    BHSSU::ms._setHour(hourNow);
    EXPECT_EQ(al1, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, BHSSU::ms.SPECIAL_HOUR_CURRENT_HOUR));
    EXPECT_NEAR(sm_al1, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, BHSSU::ms.SPECIAL_HOUR_CURRENT_HOUR), 2);
    EXPECT_EQ(al01, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR, BHSSU::ms.SPECIAL_HOUR_NEXT_HOUR));
    EXPECT_EQ(al01, BHSSU::ms.getByHourStatRTC(BHSSU::ms.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, BHSSU::ms.SPECIAL_HOUR_NEXT_HOUR));
}
