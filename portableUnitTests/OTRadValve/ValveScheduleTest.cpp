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

Author(s) / Copyright (s): Damon Hart-Davis 2017
*/

/*
 * OTRadValve simple valve schedule / programme tests.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <OTRadValve.h>

#include "OTRadValve_SimpleValveSchedule.h"


// Test for general sanity of SimpleValveSchedule.
TEST(SimpleValveSchedule,basics)
{
    srandom((unsigned)::testing::UnitTest::GetInstance()->random_seed()); // Seed random() for use in tests; --gtest_shuffle will force it to change.

    // Test reversibility of expansion of time from single programme byte.
    for(uint8_t i = 0; i <= OTRadValve::SimpleValveScheduleParams::MAX_COMPRESSED_MINS_AFTER_MIDNIGHT; ++i)
        {
        const uint16_t mins = OTRadValve::SimpleValveScheduleParams::computeTimeFromPrgrammeByte(i);
        EXPECT_GT(OTV0P2BASE::MINS_PER_DAY, mins);
        const uint8_t cmins = OTRadValve::SimpleValveScheduleParams::computeProgrammeByteFromTime(mins);
        EXPECT_EQ(i, cmins);
        }

    // Test schedule setting several times
    // to futz with multiple random values.
    for(int r = 0; r < 10; ++r)
        {
        OTRadValve::SimpleValveScheduleMock<> svsm0;
        ASSERT_LT(0, svsm0.maxSchedules()) << "expect >0 capacity by default";
        EXPECT_FALSE(svsm0.isAnySimpleScheduleSet());

        // Set and retrieve schedule times for each schedule.
        for(uint8_t i = 0; i < svsm0.maxSchedules(); ++i)
            {
            // Should be no schedule set in this slot.
            EXPECT_EQ(0xffff, svsm0.getSimpleScheduleOn(i));
            EXPECT_EQ(0xffff, svsm0.getSimpleScheduleOff(i));

            // Choose random schedule time.
            const uint16_t time = ((unsigned) random()) % OTV0P2BASE::MINS_PER_DAY;

            // At least for first schedule to be set, should not be active yet.
            if(0 == i) { EXPECT_FALSE(svsm0.isAnyScheduleOnWARMNow(time)); }

            // Set the schedule.
            svsm0.setSimpleSchedule(time, i);

            // Some sort of schedule should now be set.
            EXPECT_NE(0xffffU, svsm0.getSimpleScheduleOn(i));
            EXPECT_NE(0xffffU, svsm0.getSimpleScheduleOff(i));
            EXPECT_TRUE(svsm0.isAnySimpleScheduleSet());
            EXPECT_TRUE(svsm0.isAnyScheduleOnWARMNow(time)) << time;

            // The schedule should be clearly not be set 12h shifted,
            // at least for the first schedule to be set.
            // This should apply to warm 'now' and warm 'soon'.
            if(0 == i)
                {
                const uint16_t timeShift12h =
                    (time + OTV0P2BASE::MINS_PER_DAY/2) % OTV0P2BASE::MINS_PER_DAY;
                EXPECT_FALSE(svsm0.isAnyScheduleOnWARMNow(timeShift12h)) << timeShift12h;
                EXPECT_FALSE(svsm0.isAnyScheduleOnWARMSoon(timeShift12h)) << timeShift12h;
                }
            }
        }
}

// Test isAnyScheduleOnWARMNow().
// Check for all possible minutes that
// isAnyScheduleOnWARMNow() is true for the time that the schedule was set.
TEST(SimpleValveSchedule,isAnyScheduleOnWARMNow0)
{
    OTRadValve::SimpleValveScheduleMock<1> svsm0;
    ASSERT_EQ(1, svsm0.maxSchedules()) << "expect >0 capacity by default";
    EXPECT_FALSE(svsm0.isAnySimpleScheduleSet());
    EXPECT_EQ(0xffff, svsm0.getSimpleScheduleOn(0));
    EXPECT_EQ(0xffff, svsm0.getSimpleScheduleOff(0));
    // Check that no minute is seen as 'warm now' on freshly-minted scheduler.
    for(uint16_t m = 0; m < OTV0P2BASE::MINS_PER_DAY; ++m)
        {
        EXPECT_FALSE(svsm0.isAnyScheduleOnWARMNow(m));
        }

    // For every minute set the schedule and see that it is a 'warm' minute.
    for(uint16_t m = 0; m < OTV0P2BASE::MINS_PER_DAY; ++m)
        {
        // Clear the schedule and make sure that it is cleared.
        svsm0.clearSimpleSchedule(0);
        EXPECT_EQ(0xffff, svsm0.getSimpleScheduleOn(0));
        EXPECT_EQ(0xffff, svsm0.getSimpleScheduleOff(0));
        EXPECT_FALSE(svsm0.isAnySimpleScheduleSet());
        EXPECT_FALSE(svsm0.isAnyScheduleOnWARMNow(m));
        // Set the schedule for the given minute.
        svsm0.setSimpleSchedule(m, 0);
        // Make sure that it now set.
        EXPECT_NE(0xffffU, svsm0.getSimpleScheduleOn(0));
        EXPECT_NE(0xffffU, svsm0.getSimpleScheduleOff(0));
        EXPECT_TRUE(svsm0.isAnySimpleScheduleSet());
        // Make sure that the current minute is seen as warm.
        EXPECT_TRUE(svsm0.isAnyScheduleOnWARMNow(m)) << m;
        }
}


// TODO: needs more thorough checking, eg through a whole simulated day.
