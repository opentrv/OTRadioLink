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
{ 0, 0, 0, 254, 1 }, // Should NOT predict occupancy on first tick.
{ 0, 0, 1, 0, 1 }, // Should NOT predict occupancy on falling level.
{ 0, 0, 5, 0 }, // Should NOT predict occupancy on falling level.
{ 0, 0, 9, 254, 2 }, // Should predict occupancy on level rising to (near) max.
{ }
    };

// Do a simple run over the supplied data, one call per simulated minute until the terminating record is found.
// Must be supplied in ascending order with a terminating (empty) entry.
// Repeated rows with the same light value and expected result can be omitted
// as they will be synthesised by this routine for each virtual minute until the next supplied item.
// Ensures that any required predictions/detections in either direction are met.
// Uses only the update() call.
void simpleDataSampleRun(const ALDataSample *const data, OTV0P2BASE::SensorAmbientLightOccupancyDetectorInterface *const detector)
    {
    ASSERT_TRUE(NULL != data);
    ASSERT_TRUE(NULL != detector);
    ASSERT_FALSE(data->isEnd()) << "do not pass in empty data set";
    for(const ALDataSample *dp = data; !dp->isEnd(); ++dp)
        {
    	long currentMinute = dp->currentMinute();
    	do  {
//            fprintf(stderr, "Mins: %ld\n", currentMinute);
            const bool prediction = detector->update(dp->L);
            const uint8_t expected = dp->expected;
            if(0 != expected)
                {
                // If a particular outcome was expected, test against it.
                const bool expectedOccupancy = (expected > 1);
                EXPECT_EQ(expectedOccupancy, prediction) << " @ " << int(dp->H) << ":" << int(dp->M) << " + " << (currentMinute - dp->currentMinute());
                }
            ++currentMinute;
    	    } while((!(dp+1)->isEnd()) && (currentMinute < (dp+1)->currentMinute()));
        }
	}

// Basic test of update() behaviour.
TEST(AmbientLightOccupancyDetection,simpleDataSampleRun)
{
	OTV0P2BASE::SensorAmbientLightOccupancyDetectorSimple ds1;
	simpleDataSampleRun(trivialSample1, &ds1);
}

// "3l" 2016/10/08 test set with tough occupancy to detect in the evening up to 21:00Z and in the morning from 07:09Z then  06:37Z.
static const ALDataSample sample3lHard[] =
    {
{8,0,1,1, 1}, // Definitely not occupied.
{8,0,17,1, 1}, // Definitely not occupied.
//...
{8,6,21,1},
{8,6,29,2, 1}, // Not enough rise to indicate occupation.
{8,6,33,2},
{8,6,45,2},
{8,6,57,2},
{8,7,9,14},  // Occupied: curtains drawn?
{8,7,17,35},
{8,7,21,38},
{8,7,33,84},
{8,7,37,95},
{8,7,49,97, 1}, // Not enough rise to be occupation.
{8,7,57,93, 1}, // Fall is not indicative of occupation.
{8,8,5,98},
{8,8,13,98},
{8,8,17,93},
{8,8,25,79},
{8,8,33,103},
{8,8,41,118},
{8,8,49,106},
{8,8,53,92},
{8,8,57,103},
{8,9,5,104},
{8,9,21,138},
{8,9,29,132},
{8,9,33,134},
{8,9,45,121},
{8,9,53,125},
{8,10,5,140},
{8,10,9,114},
{8,10,17,121},
{8,10,21,126},
{8,10,25,114},
{8,10,29,107},
{8,10,41,169},
{8,10,49,177},
{8,10,57,126},
{8,11,1,117},
{8,11,5,114},
{8,11,13,111},
{8,11,17,132},
{8,11,21,157},
{8,11,29,177},
{8,11,33,176},
{8,11,45,174},
{8,11,49,181},
{8,11,57,182},
{8,12,9,181},
{8,12,13,182},
{8,12,29,175},
{8,12,45,161},
{8,12,53,169},
{8,13,1,176},
{8,13,5,177},
{8,13,9,178},
{8,13,25,158},
{8,13,29,135},
{8,13,37,30},
{8,13,45,37},
{8,13,49,45},
{8,14,5,61},
{8,14,17,117},
{8,14,29,175},
{8,14,33,171},
{8,14,37,148},
{8,14,45,141},
{8,14,53,173},
{8,15,5,125},
{8,15,13,119},
{8,15,21,107},
{8,15,29,58},
{8,15,37,62},
{8,15,45,54},
{8,15,53,47},
{8,16,1,35},
{8,16,9,48},
{8,16,25,50},
{8,16,37,39},
{8,16,41,34},
{8,16,49,34},
{8,16,57,28},
{8,17,5,20},
{8,17,13,7},
{8,17,25,4},
{8,17,37,44, 2}, // Occupied (light on?).
    {8,17,38,44},
{8,17,49,42},
{8,18,1,42},
{8,18,9,40},
{8,18,13,42, 1}, // Not enough rise to be occupation.
{8,18,25,40},
{8,18,37,40},
{8,18,41,42},
{8,18,49,42},
{8,18,57,41},
{8,19,1,40},
{8,19,13,41},
{8,19,21,39},
{8,19,25,41},
{8,19,41,41},
{8,19,52,42},
{8,19,57,40},
{8,20,5,40},
{8,20,9,42},
{8,20,17,42},
{8,20,23,40},
{8,20,29,40},
{8,20,33,40},
{8,20,37,41},
{8,20,41,42},
{8,20,49,40},
{8,21,5,1, 1}, // Definitely not occupied.
{8,21,13,1, 1}, // Definitely not occupied.
// ...
{9,5,57,1, 1}, // Definitely not occupied.
{9,6,13,1, 1}, // Definitely not occupied.
{9,6,21,2, 1}, // Not enough rise to indicate occupation.
{9,6,33,2},
{9,6,37,24, 2}, // Curtains drawn: occupied.
    {9,6,38,24},
{9,6,45,32},
{9,6,53,31},
{9,7,5,30},
{9,7,17,41},
{9,7,25,54},
{9,7,33,63},
{9,7,41,73},
{9,7,45,77},
{ }
    };

// Test with real data set.
TEST(AmbientLightOccupancyDetection,sample3lHard)
{
	OTV0P2BASE::SensorAmbientLightOccupancyDetectorSimple ds1;
	simpleDataSampleRun(sample3lHard, &ds1);
}
