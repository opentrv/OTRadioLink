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
// Must be called with 1 or more data rows in ascending time with a terminating (empty) entry.
// Repeated rows with the same light value and expected result can be omitted
// as they will be synthesised by this routine for each virtual minute until the next supplied item.
// Ensures that any required predictions/detections in either direction are met.
// Can be supplied with nominal long-term rolling min and max
// or they can be computed from the data supplied (0xff implies no data).
// Can be supplied with nominal long-term rolling mean levels by hour,
// or they can be computed from the data supplied (NULL means none supplied, 0xff entry means none for given hour).
// Uses the update() call for the main simulation.
// Uses the setTypMinMax() call as the hour rolls; leaves 'sensitive' off by default.
void simpleDataSampleRun(const ALDataSample *const data, OTV0P2BASE::SensorAmbientLightOccupancyDetectorInterface *const detector,
                         const uint8_t minLevel = 0xff, const uint8_t maxLevel = 0xff,
                         const uint8_t meanByHour[24] = NULL)
    {
    ASSERT_TRUE(NULL != data);
    ASSERT_TRUE(NULL != detector);
    ASSERT_FALSE(data->isEnd()) << "do not pass in empty data set";
    // Compute own values for min, max, etc.
    int minI = 256;
    int maxI = -1;
    uint8_t byHourMeanI[24];
    int byHourMeanSumI[24]; memset(byHourMeanSumI, 0, sizeof(byHourMeanSumI));
    int byHourMeanCountI[24]; memset(byHourMeanCountI, 0, sizeof(byHourMeanCountI));
    for(const ALDataSample *dp = data; !dp->isEnd(); ++dp)
        {
    	long currentMinute = dp->currentMinute();
    	do  {
            const uint8_t level = dp->L;
            if((int)level < minI) { minI = level; }
            if((int)level > maxI) { maxI = level; }
    		const uint8_t H = (currentMinute % 1440) / 60;
            ASSERT_TRUE(H < 24) << "bad hour";
            byHourMeanSumI[H] += level;
            ++byHourMeanCountI[H];
            ++currentMinute;
    	    } while((!(dp+1)->isEnd()) && (currentMinute < (dp+1)->currentMinute()));
        }
//    fprintf(stderr, "minI: %d, maxI %d\n", minI, maxI);
    for(int i = 24; --i >= 0; )
        {
        if(0 != byHourMeanCountI[i])
            { byHourMeanI[i] = (uint8_t)((byHourMeanSumI[i] + (byHourMeanCountI[i]>>1)) / byHourMeanCountI[i]); }
        else { byHourMeanI[i] = 0xff; }
        }
//    fprintf(stderr, "mean by hour:");
//    for(int i = 0; i < 24; ++i)
//        {
//        fputc(' ', stderr);
//        if(0xff == byHourMeanI[i]) { fputc('-', stderr); }
//        else { fprintf(stderr, "%d", (int)byHourMeanI[i]); }
//        }
//    fprintf(stderr, "\n");
    // Select which params to use.
    const uint8_t minToUse = (0xff != minLevel) ? minLevel :
    		((minI < 255) ? (uint8_t)minI : 0xff);
    const uint8_t maxToUse = (0xff != maxLevel) ? maxLevel :
    		((maxI >= 0) ? (uint8_t)maxI : 0xff);
    // Run simulation.
    uint8_t oldH = 0xff;
    for(const ALDataSample *dp = data; !dp->isEnd(); ++dp)
        {
    	long currentMinute = dp->currentMinute();
    	do  {
    		const uint8_t H = (currentMinute % 1440) / 60;
    		if(H != oldH)
    		    {
    		    // When the hour rolls, set new stats for the detector.
    		    // Note that implementations be use the end of the hour/period
    		    // and other times.
    		    // The detector and caller should aim not to be hugely sensitive to the exact timing,
    		    // eg by blending prev/current/next periods linearly.
//fprintf(stderr, "mean = %d\n", byHourMeanI[H]);
                const bool sensitive = false;
    		    detector->setTypMinMax(byHourMeanI[H], minToUse, maxToUse, sensitive);
    		    ASSERT_EQ(sensitive, detector->isSensitive());
    		    oldH = H;
    		    }
            const bool prediction = detector->update(dp->L);
if(prediction) { fprintf(stderr, "@ %d:%d L = %d\n", H, (int)(currentMinute % 60), dp->L); }
            // Note that for all synthetic ticks the expectation is removed (since there is no level change).
            const uint8_t expected = (currentMinute != dp->currentMinute()) ? 0 : dp->expected;
            if(0 != expected)
                {
                // If a particular outcome was expected, test against it.
                const bool expectedOccupancy = (expected > 1);
        		const uint8_t M = (currentMinute % 60);
                EXPECT_EQ(expectedOccupancy, prediction) << " @ " << ((int)H) << ":" << ((int)M);
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

// "3l" 2016/10/08+09 test set with tough occupancy to detect in the evening up to 21:00Z and in the morning from 07:09Z then  06:37Z.
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
{8,7,9,14},  // OCCUPIED: curtains drawn?
{8,7,17,35},
{8,7,21,38},
{8,7,33,84},
{8,7,37,95},
{8,7,49,97, 1}, // Not enough rise to be occupation.
{8,7,57,93, 1}, // Fall is not indicative of occupation.
{8,8,5,98, 1}, // Sun coming up: not enough rise to indicate occupation.
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
{8,17,37,44, 2}, // OCCUPIED (light on?).
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
{9,6,37,24, 2}, // Curtains drawn: OCCUPIED.
{9,6,45,32},
{9,6,53,31},
{9,7,5,30},
{9,7,17,41},
{9,7,25,54},
{9,7,33,63, 1}, // Sun coming up; not a sign of occupancy.
{9,7,41,73, 1}, // Sun coming up; not a sign of occupancy.
{9,7,45,77, 1}, // Sun coming up: not enough rise to indicate occupation.
{ }
    };

// Test with real data set.
TEST(AmbientLightOccupancyDetection,sample3lHard)
{
	OTV0P2BASE::SensorAmbientLightOccupancyDetectorSimple ds1;
	simpleDataSampleRun(sample3lHard, &ds1);
}

// "5s" 2016/10/08+09 test set with tough occupancy to detect in the evening 21:00Z.
static const ALDataSample sample5sHard[] =
    {
{8,0,3,2, 1}, // Not occupied actively.
{8,0,19,2, 1}, // Not occupied actively.
// ...
{8,5,19,2, 1}, // Not occupied actively.
{8,5,31,1, 1}, // Not occupied actively.
{8,5,43,2, 1}, // Not occupied actively.
// ...
{8,6,23,4},
{8,6,35,6},
{8,6,39,5},
{8,6,51,6},
{8,7,3,9},
{8,7,11,12},
{8,7,15,13},
{8,7,19,17},
{8,7,27,42},
{8,7,31,68},
{8,7,43,38},
{8,7,51,55},
{8,7,55,63},
{8,7,59,69},
{8,8,11,68},
{8,8,15,74},
{8,8,27,72},
{8,8,43,59},
{8,8,51,38},
{8,8,55,37},
{8,8,59,34},
{8,9,3,43},
{8,9,19,79},
{8,9,23,84},
{8,9,35,92},
{8,9,39,64},
{8,9,43,78},
{8,9,55,68},
{8,9,59,60},
{8,10,3,62},
{8,10,11,41},
{8,10,15,40},
{8,10,16,42},
{8,10,23,40},
{8,10,27,45},
{8,10,39,99},
{8,10,46,146},
{8,10,51,79},
{8,10,56,46},
{8,11,3,54},
{8,11,7,63},
{8,11,23,132},
{8,11,27,125},
{8,11,39,78},
{8,11,55,136},
{8,11,59,132},
{8,12,7,132},
{8,12,19,147},
{8,12,23,114},
{8,12,35,91},
{8,12,47,89},
{8,12,55,85},
{8,13,3,98},
{8,13,11,105},
{8,13,19,106},
{8,13,31,32},
{8,13,43,29},
{8,13,51,45},
{8,13,55,37},
{8,13,59,31},
{8,14,7,42},
{8,14,27,69},
{8,14,31,70},
{8,14,35,63},
{8,14,55,40},
{8,15,7,47},
{8,15,11,48},
{8,15,19,66},
{8,15,27,48},
{8,15,35,46},
{8,15,43,40},
{8,15,51,33},
{8,16,3,24},
{8,16,11,26},
{8,16,27,20},
{8,16,39,14},
{8,16,54,8},
{8,16,59,6},
{8,17,3,5},
{8,17,19,3},
{8,17,31,2},
{8,17,47,2},
{8,17,59,2},
{8,18,19,2},
{8,18,35,2},
{8,18,47,2},
{8,18,55,2},
{8,19,7,2},
{8,19,19,2},
{8,19,31,2},
{8,19,43,2},
{8,19,55,2},
{8,20,11,2},
{8,20,23,2},
{8,20,35,16, 2}, // Light turned on, OCCUPANCY.
{8,20,46,16},
{8,20,55,13},
{8,20,58,14},
{8,21,7,3, 1}, // Light turned off, no occupancy.
{8,21,23,2},
{8,21,39,2},
{8,21,55,2},
{8,22,11,2},
{8,22,19,2},
{8,22,31,2},
{8,22,43,2},
{8,22,59,2},
{8,23,15,2},
{8,23,27,2},
{8,23,43,2},
{8,23,59,2},
{9,0,15,2},
{9,0,23,2},
{9,0,39,2},
{9,0,55,2},
{9,1,7,2},
{9,1,15,1},
{9,1,19,1},
{9,1,35,1},
{9,1,51,1},
{9,2,3,1},
{9,2,11,1},
{9,2,23,1},
{9,2,35,1},
{9,2,47,1},
{9,2,59,1},
{9,3,7,1},
{9,3,15,1},
{9,3,31,1},
{9,3,47,1},
{9,3,55,1},
{9,4,11,1},
{9,4,23,1},
{9,4,35,1},
{9,4,43,1},
{9,4,53,1},
{9,5,7,1},
{9,5,19,1},
{9,5,31,1},
{9,5,36,1},
{9,5,47,2},
{9,5,51,2},
{9,6,3,3},
{9,6,15,5},
{9,6,27,10},
{9,6,31,12},
{9,6,35,15},
{9,6,39,19},
{9,6,43,26},
{9,6,59,24},
{9,7,7,28},
{9,7,15,66, 1}, // Not yet up and about.
{9,7,27,181, 2}, // Curtains drawn: OCCUPANCY.
{9,7,43,181},
{9,7,51,181},
{9,7,59,181},
    { }
    };

// Test with real data set.
TEST(AmbientLightOccupancyDetection,sample5sHard)
{
    OTV0P2BASE::SensorAmbientLightOccupancyDetectorSimple ds1;
    simpleDataSampleRun(sample5sHard, &ds1);
}

//// "2b" 2016/10/08+09 test set with tough occupancy to detect in the evening ~19:00Z to 20:00Z.
//static const ALDataSample sample2bHard[] =
//    {
//    { }
//    };
//
//// Test with real data set.
//TEST(AmbientLightOccupancyDetection,sample2bHard)
//{
//    OTV0P2BASE::SensorAmbientLightOccupancyDetectorSimple ds1;
//    simpleDataSampleRun(sample2bHard, &ds1);
//}
