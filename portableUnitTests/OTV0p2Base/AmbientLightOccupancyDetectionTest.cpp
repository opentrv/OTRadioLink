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

// Ambient light data samples, along with optional expected result of occupancy detector.
// Can be directly created from OpenTRV log files.
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
// Uses the setTypMinMax() call as the hour rolls or in more complex blended-stats modes;
// runs with 'sensitive' in both states to verify algorithm's robustness.
// Will fail if an excessive amount of the time occupancy is predicted (more than ~25%).
void simpleDataSampleRun(const ALDataSample *const data, OTV0P2BASE::SensorAmbientLightOccupancyDetectorInterface *const detector,
                         const uint8_t minLevel = 0xff, const uint8_t maxLevel = 0xff,
                         const uint8_t meanByHour[24] = NULL)
    {
    static const bool verbose = false; // Set true for more verbose reporting.
    ASSERT_TRUE(NULL != data);
    ASSERT_TRUE(NULL != detector);
    ASSERT_FALSE(data->isEnd()) << "do not pass in empty data set";
    // Count of number of records.
    int nRecords = 0;
    // COunt number of records with explicit expected response assertion.
    int nExpectation = 0;
    // Compute own values for min, max, etc.
    int minI = 256;
    int maxI = -1;
    uint8_t byHourMeanI[24];
    int byHourMeanSumI[24]; memset(byHourMeanSumI, 0, sizeof(byHourMeanSumI));
    int byHourMeanCountI[24]; memset(byHourMeanCountI, 0, sizeof(byHourMeanCountI));
    for(const ALDataSample *dp = data; !dp->isEnd(); ++dp)
        {
        ++nRecords;
        if(0 != dp->expected) { ++nExpectation; }
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
    ASSERT_LT(0, nExpectation) << "must assert some expected predictions";
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
    // Run simulation with different stats blending types
    // to ensure that occupancy detection is robust.
    enum blending_t { NONE, HALFHOURMIN, HALFHOUR, BYMINUTE, END };
    for(int blending = NONE; blending < END; ++blending)
        {
if(verbose) { fprintf(stderr, "blending = %d\n", blending); }
        // Run simulation at both sensitivities.
        int nOccupancyReportsSensitive = 0;
        int nOccupancyReportsNotSensitive = 0;
        for(int s = 0; s <= 1; ++s)
            {
            const bool sensitive = (0 != s);
if(verbose) { fputs(sensitive ? "sensitive\n" : "not sensitive\n", stderr); }
            // Count of number of occupancy signals.
            int nOccupancyReports = 0;
            uint8_t oldH = 0xff; // Used to detect hour rollover.
            for(const ALDataSample *dp = data; !dp->isEnd(); ++dp)
                {
                long currentMinute = dp->currentMinute();
                do  {
                    const uint8_t H = (currentMinute % 1440) / 60;
                    const uint8_t M = (currentMinute % 60);
                    switch(blending)
                        {
                        case NONE: // Use unblended mean for this hour.
                            {
                            if(H != oldH)
                                {
                                // When the hour rolls, set new stats for the detector.
                                // Note that implementations be use the end of the hour/period
                                // and other times.
                                // The detector and caller should aim not to be hugely sensitive to the exact timing,
                                // eg by blending prev/current/next periods linearly.
//fprintf(stderr, "mean = %d\n", byHourMeanI[H]);
                                detector->setTypMinMax(byHourMeanI[H], minToUse, maxToUse, sensitive);
                                ASSERT_EQ(sensitive, detector->isSensitive());
                                }
                            break;
                            }
                        case HALFHOURMIN: // Use blended (min) mean for final half hour hour.
                            {
                            const uint8_t thm = byHourMeanI[H];
                            const uint8_t nhm = byHourMeanI[(H+1)%24];
                            int8_t m = thm; // Default to this hour's mean.
                            if(M >= 30)
                                {
                                // In last half hour of each hour...
                                if(0xff == thm) { m = nhm; } // Use next hour mean if none available for this hour.
                                else if(0xff != nhm) { m = OTV0P2BASE::fnmin(nhm, thm); } // Take min when both hours' means available.
                                }
                            detector->setTypMinMax(m, minToUse, maxToUse, sensitive);
                            break;
                            }
                        case HALFHOUR: // Use blended mean for final half hour hour.
                            {
                            const uint8_t thm = byHourMeanI[H];
                            const uint8_t nhm = byHourMeanI[(H+1)%24];
                            int8_t m = thm; // Default to this hour's mean.
                            if(M >= 30)
                                {
                                // In last half hour of each hour...
                                if(0xff == thm) { m = nhm; } // Use next hour mean if none available for this hour.
                                else if(0xff != nhm) { m = (thm + (int)nhm + 1) / 2; } // Take mean when both hours' means available.
                                }
                            detector->setTypMinMax(m, minToUse, maxToUse, sensitive);
                            break;
                            }
                        case BYMINUTE: // Adjust blend by minute.
                            {
                            const uint8_t thm = byHourMeanI[H];
                            const uint8_t nhm = byHourMeanI[(H+1)%24];
                            int8_t m = thm; // Default to this hour's mean.
                            if(0xff == thm) { m = nhm; } // Use next hour's mean always if this one's not available.
                            else
                                {
                                // Continuous blend.
                                m = ((((int)thm) * (60-M)) + (((int)nhm) * M) + 30) / 60;
                                }
                            detector->setTypMinMax(m, minToUse, maxToUse, sensitive);
                            break;
                            }
                        }
                    oldH = H;
                    const bool prediction = detector->update(dp->L);
                    if(prediction) { ++nOccupancyReports; }
                    // Note that for all synthetic ticks the expectation is removed (since there is no level change).
                    const uint8_t expected = (currentMinute != dp->currentMinute()) ? 0 : dp->expected;
if(verbose && prediction) { fprintf(stderr, "@ %d:%d L = %d e = %d\n", H, M, dp->L, expected); }
                    if(0 != expected)
                        {
                        // If a particular outcome was expected, test against it.
                        const bool expectedOccupancy = (expected > 1);
                        EXPECT_EQ(expectedOccupancy, prediction) << " @ " << ((int)H) << ":" << ((int)M);
                        }
                    ++currentMinute;
                    } while((!(dp+1)->isEnd()) && (currentMinute < (dp+1)->currentMinute()));
                }
            // Check that there are not huge numbers of (false) positives.
            ASSERT_TRUE(nOccupancyReports <= (nRecords/4)) << "far too many occupancy indications";
            if(sensitive) { nOccupancyReportsSensitive = nOccupancyReports; }
            else { nOccupancyReportsNotSensitive = nOccupancyReports; }
            detector->update(254); // Force detector to 'initial'-like state ready for re-run.
            }
        EXPECT_LE(nOccupancyReportsNotSensitive, nOccupancyReportsSensitive) << "expect sensitive never to generate fewer reports";
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
{8,7,33,84, 2}, // Lights on or more curtains drawn?  Possibly occupied.
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
{8,7,27,42}, // ? Curtains drawn?
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
{8,21,23,2, 1}, // Light turned off, no occupancy.
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
{9,7,7,28, 1}, // Not yet up and about.
{9,7,15,66},
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

// "2b" 2016/10/08+09 test set with tough occupancy to detect in the evening ~19:00Z to 20:00Z.
static const ALDataSample sample2bHard[] =
    {
{8,0,12,3},
{8,0,24,3},
// ...
{8,7,28,3},
{8,7,40,180, 2}, // Curtains drawn, OCCUPANCY.
{8,7,44,179},
{8,7,52,180},
{8,8,0,182},
{8,8,8,183},
{8,8,20,182},
{8,8,28,182},
{8,8,36,183},
{8,8,48,183},
{8,8,52,182},
{8,9,0,182},
{8,9,4,182},
{8,9,20,184},
{8,9,24,183},
{8,9,32,183},
{8,9,36,183},
{8,9,48,183},
{8,10,4,183},
{8,10,16,183},
{8,10,28,182},
{8,10,32,183},
{8,10,44,185},
{8,10,48,186},
{8,11,0,184},
{8,11,4,183},
{8,11,20,184},
{8,11,24,185},
{8,11,29,186},
{8,11,36,185},
{8,11,44,186},
{8,11,48,186},
{8,12,4,186},
{8,12,16,187},
{8,12,20,187},
{8,12,32,184},
{8,12,36,186},
{8,12,48,185},
{8,12,56,185},
{8,13,4,186},
{8,13,8,187},
{8,13,24,186},
{8,13,28,183},
{8,13,32,186},
{8,13,40,120},
{8,13,44,173},
{8,13,48,176},
{8,13,52,178},
{8,13,56,179},
{8,14,4,180},
{8,14,8,182},
{8,14,12,183},
{8,14,18,183},
{8,14,28,185},
{8,14,32,186},
{8,14,40,186},
{8,14,48,185},
{8,14,52,186},
{8,15,0,182},
{8,15,4,181},
{8,15,12,184},
{8,15,19,186},
{8,15,24,182},
{8,15,32,181},
{8,15,40,182},
{8,15,52,182},
{8,16,0,178},
{8,16,4,176},
{8,16,16,181},
{8,16,20,182},
{8,16,32,178},
{8,16,40,176},
{8,16,48,168},
{8,16,52,176},
{8,16,56,154},
{8,17,5,68},
{8,17,8,37},
{8,17,16,30},
{8,17,20,20},
{8,17,32,12},
{8,17,40,5},
{8,17,44,4},
{8,17,52,3},
{8,18,0,3},
{8,18,12,3},
{8,18,24,3},
{8,18,40,3},
{8,18,52,3},
{8,19,4,3},
{8,19,20,3},
{8,19,32,4},
{8,19,39,4},
{8,19,52,4},
{8,20,0,7},
{8,20,16,6},
{8,20,20,10, 2}, // Light on, OCCUPANCY.
{8,20,28,6},
{8,20,36,3},
{8,20,42,3},
// ...
{9,7,40,3},
{9,7,48,3},
{9,7,52,4},
{9,8,8,176, 2}, // Curtains drawn, OCCUPANCY.
{9,8,20,177},
{9,8,32,177},
{9,8,44,178},
{9,8,56,178},
{9,9,8,179},
{9,9,16,179},
{9,9,20,180},
{9,9,36,180},
{9,9,48,180},
{9,9,52,181},
{9,10,0,181},
{9,10,4,179},
{9,10,8,181},
{9,10,20,182},
{9,10,24,185},
{9,10,40,185},
{9,10,44,184},
{9,10,52,184},
{9,11,0,184},
{9,11,8,185},
{9,11,12,186},
{9,11,16,185},
{9,11,24,183},
{9,11,28,183},
{9,11,40,186},
{9,11,44,186},
{9,12,4,184},
{9,12,16,184},
{9,12,24,186},
{9,12,32,187},
{9,12,40,186},
{9,12,44,187},
{9,12,56,187},
{9,13,8,186},
{9,13,12,185},
{9,13,13,185},
{9,13,8,186},
{9,13,12,185},
{9,13,13,185},
{9,13,24,187},
{9,13,36,188},
{9,13,48,184},
{9,13,52,186},
{9,13,56,185},
{9,14,4,185},
{9,14,12,184},
{9,14,16,186},
{9,14,28,185},
{9,14,36,187},
{9,14,40,186},
{9,14,52,184},
{9,15,0,183},
{9,15,4,185},
{9,15,8,183},
{9,15,16,176},
{9,15,24,164},
{9,15,28,178},
{9,15,32,181},
{9,15,40,177},
{9,15,44,128},
{9,15,48,107},
{9,15,56,98},
{9,16,0,96},
{9,16,4,68},
{9,16,12,63},
{9,16,20,81},
{9,16,33,95},
{9,16,44,97},
{9,16,52,73},
{9,16,56,56},
{9,17,0,46},
{9,17,4,40},
{9,17,12,32},
{9,17,16,25},
{9,17,32,7},
{9,17,36,5},
{9,17,41,4},
{9,17,48,3},
{9,18,0,3},
{9,18,12,3},
{9,18,28,3},
{9,18,40,3},
{9,18,56,3},
{9,19,8,10, 2}, // Light on, OCCUPANCY.
{9,19,16,9},
{9,19,28,10},
{9,19,44,6},
{9,19,48,11, 2}, // Small light on?  Possible occupancy.
{9,19,56,8},
{9,20,4,8},
{9,20,8,3, 1}, // Light off, no qctive occupancy.
{9,20,20,3},
{9,20,36,3},
    { }
    };

// Test with real data set.
TEST(AmbientLightOccupancyDetection,sample2bHard)
{
    OTV0P2BASE::SensorAmbientLightOccupancyDetectorSimple ds1;
    simpleDataSampleRun(sample2bHard, &ds1);
}

// "6k" 2016/10/08+09 test set relatively easy to detect daytime occupancy in busy room.
static const ALDataSample sample6k[] =
    {
{8,0,7,1, 1}, // Not occupied.
{8,0,19,1},
{8,0,35,1},
{8,0,47,1},
{8,1,3,1},
{8,1,19,2, 1}, // Not occupied.
{8,1,35,2},
{8,1,39,2},
// ...
{8,6,11,2},
{8,6,23,3},
{8,6,35,5},
{8,6,39,4},
{8,6,42,4},
{8,6,47,4},
{8,6,55,5},
{8,7,7,20},
{8,7,15,25},
{8,7,19,33},
{8,7,31,121, 2}, // Light on: OCCUPIED.
{8,7,40,35},
{8,7,52,62},
{8,8,7,168},
{8,8,19,173},
{8,8,23,146},
{8,8,35,96},
{8,8,43,57},
{8,8,47,61},
{8,9,3,44},
{8,9,7,48},
{8,9,19,93},
{8,9,23,107},
{8,9,31,174},
{8,9,43,146},
{8,9,47,128},
{8,9,55,145},
{8,10,7,121},
{8,10,11,110},
{8,10,19,118},
{8,10,27,119},
{8,10,35,137},
{8,10,39,166},
{8,10,43,177},
{8,10,47,180},
{8,10,55,127},
{8,10,59,131},
{8,11,11,152},
{8,11,15,166},
{8,11,31,153},
{8,11,35,147},
{8,11,43,143},
{8,11,51,162},
{8,11,55,178},
{8,12,7,155},
{8,12,15,179},
{8,12,17,172},
{8,12,19,84},
{8,12,27,55},
{8,12,35,85},
{8,12,43,90},
{8,12,55,89},
{8,12,59,100},
{8,13,11,106},
{8,13,15,102},
{8,13,23,101},
{8,13,35,14},
{8,13,47,38},
{8,13,55,34},
{8,13,59,25},
{8,14,3,27},
{8,14,11,41},
{8,14,15,50},
{8,14,19,53},
{8,14,27,58},
{8,14,31,59},
{8,14,35,52},
{8,14,47,63},
{8,14,59,29},
{8,15,3,24},
{8,15,11,38},
{8,15,15,45},
{8,15,19,61},
{8,15,27,44},
{8,15,39,44},
{8,15,43,40},
{8,15,51,33},
{8,15,55,29},
{8,15,59,28},
{8,16,3,23},
{8,16,19,27},
{8,16,27,18},
{8,16,35,164, 2}, // Light on: OCCUPIED.
{8,16,39,151},
{8,16,51,153},
{8,17,3,151},
{8,17,11,122},
{8,17,15,131},
{8,17,31,138},
{8,17,35,1, 1}, // Light off: not occupied.
{8,17,43,1},
{8,17,55,1},
{8,18,3,1},
{8,18,15,1},
{8,18,23,1},
{8,18,35,1},
{8,18,47,1},
{8,18,59,1},
{8,19,11,1},
{8,19,23,1},
{8,19,31,7},
{8,19,35,6},
{8,19,47,6},
{8,19,59,6},
{8,20,11,6},
{8,20,19,1},
{8,20,23,1},
{8,20,35,1},
{8,20,51,1},
{8,20,59,1},
{8,21,11,1},
{8,21,27,90, 2}, // Light on: OCCUPIED.
{8,21,43,82},
{8,21,47,80},
{8,21,51,79},
{8,22,7,1, 1}, // Light off: not occupied.
{8,22,19,1},
// ...
{9,5,59,1},
{9,6,7,2},
{9,6,11,2},
{9,6,15,3},
{9,6,23,4},
{9,6,31,6},
{9,6,35,8},
{9,6,47,50, 2}, // Light on or blinds open: OCCUPIED.
{9,6,51,53},
{9,7,7,48},
{9,7,11,57},
{9,7,23,108},
{9,7,39,185},
{9,7,43,184},
{9,7,51,184},
    { }
    };

// Test with real data set.
TEST(AmbientLightOccupancyDetection,sample6k)
{
    OTV0P2BASE::SensorAmbientLightOccupancyDetectorSimple ds1;
    simpleDataSampleRun(sample6k, &ds1);
}
