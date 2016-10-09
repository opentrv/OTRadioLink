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
 * Driver for OTV0P2BASE_SensorAmbientLightOccupancy tests.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>
#include "OTV0P2BASE_SensorAmbientLightOccupancy.h"


// Sanity test.
TEST(AmbientLightOccupancyDetection,SanityTest)
{
    EXPECT_EQ(42, 42);
}


// Basic test of update() behaviour.
TEST(AmbientLightOccupancyDetection,updateBasics)
{
    // Check that initial update never indicates occupancy.
	OTV0P2BASE::SensorAmbientLightOccupancyDetectorSimple ds1;
    EXPECT_FALSE(ds1.update(0)) << "no initial update should imply occupancy";
	OTV0P2BASE::SensorAmbientLightOccupancyDetectorSimple ds2;
    EXPECT_FALSE(ds2.update(255)) << "no initial update should imply occupancy";
    // Check that update from 0 to max does force occupancy indication (but steady does not).
    EXPECT_TRUE(ds1.update(255)) << "update from 0 to 255 (max) illumination should signal occupancy";
    EXPECT_FALSE(ds2.update(255)) << "unchanged 255 (max) light level should not imply occupancy";
}

class ALDataSample
    {
    public:
        const uint8_t d, H, M, L, expected;

		// Day/hour/minute and light level and expected result.
		// An expected result of 0 means no particular result expected from this (anything is acceptable).
		// An expected result of 1 means occupancy should NOT be reported for this sample.
		// An expected result of 2+ means occupancy should be reported for this sample.
	    ALDataSample(uint8_t dayOfMonth, uint8_t hour24, uint8_t minute, uint8_t lightLevel, uint8_t expectedResult = 0)
			: d(dayOfMonth), H(hour24), M(minute), L(lightLevel), expected(expectedResult)
			{ }

        // Create/mark a terminating entry; all input values invalid.
	    ALDataSample() : d(255), H(255), M(255), L(255), expected(0) { }

        // Compute current minute for this record.
        long currentMinute() const { return((((d * 24L) + H) * 60L) + M); }

	    // True for empty/termination data record.
	    bool isEnd() const { return(d > 31); }
    };

// Trivial sample, testing initial reaction to start transient.
static const ALDataSample trivialSample1[] =
    {
{ 0, 0, 0, 255, 1 },
{ 0, 0, 1, 0, 1 },
{ 0, 0, 5, 0 },
{ 0, 0, 9, 255, 2 },
{ }
    };

// Do a simple run over the supplied data, one call per simulated minute until the terminating record is found.
// Ensures that any required predictions/detections in either direction are met.
// Uses only the update() call.
void simpleDataSampleRun(const ALDataSample *const data, OTV0P2BASE::SensorAmbientLightOccupancyDetectorInterface *const detector)
    {
    ASSERT_TRUE(NULL != data);
    ASSERT_TRUE(NULL != detector);
    ASSERT_FALSE(data->isEnd()) << "do not pass in empty data set";
    for(const ALDataSample *dp = data; !dp->isEnd(); ++dp)
        {
        const long mins = dp->currentMinute();
        fprintf(stderr, "Mins: %ld\n", mins);
        }
	}

// Basic test of update() behaviour.
TEST(AmbientLightOccupancyDetection,simpleDataSampleRun)
{
	OTV0P2BASE::SensorAmbientLightOccupancyDetectorSimple ds1;
	simpleDataSampleRun(trivialSample1, &ds1);
}
