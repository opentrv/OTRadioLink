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
 *
 * These data sets can also be used to test key related and derived behaviours
 * such as basic ambient light level sensing and temperature setback levels.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>
#include <OTRadValve.h>
#include "OTV0P2BASE_SensorAmbientLightOccupancy.h"


// Set true for verbose reporting.
static constexpr bool verbose = true;
// Lots of extra detail, generally should not be needed.
static constexpr bool veryVerbose = false && verbose;

// Import occType enum values.
typedef OTV0P2BASE::SensorAmbientLightOccupancyDetectorInterface::occType occType;

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
// Can be directly created from OpenTRV log files into day hour minute value columns,
// eg from log lines of such as "2016-11-24T22:07:39Z 96F0CED3B4E690E8 47" with:
//    awk '{ print "{"(0+substr($1,9,2))","(0+substr($1, 12, 2))","(0+substr($1, 15, 2))","$3"},"; }'
class ALDataSample final
    {
    public:
        // Time/data values.
        const uint8_t d, H, M, L;

        // -1 implies no occupancy prediction, distinct from all occType values.
        static constexpr int8_t NO_OCC_EXPECTATION = -1;
        // Occupancy prediction OCC_XXX; -1 for no prediction.
        const int8_t expectedOcc = NO_OCC_EXPECTATION;

        // -1 implies no room-dark prediction, distinct from bool values.
        static constexpr int8_t NO_RD_EXPECTATION = -1;
        // Room-dark flag; -1 for no prediction.
        const int8_t expectedRd = NO_RD_EXPECTATION;

        // -1 implies no actual occupancy known, distinct from bool values.
        static constexpr int8_t UNKNOWN_ACT_OCC = -1;
        // Actual occupancy flag; -1 for no occupancy known.
        // Meant to be evaluated against output of occupancy tracker.
        // Errors in known vs predicted will be counted against a threshold.
        const int8_t actOcc = UNKNOWN_ACT_OCC;

        // Scale mid-point setback prediction (C); -1 for no prediction.
        // This is for a conventional/default set of valve parameters,
        // mainly intended to ensure sane behaviour under normal circumstances.
        // Useful setback predictions will probably need at least 24h of data.
        enum expectedSb_t : int8_t
            {
            NO_SB_EXPECTATION = -1, // -1 indicates no setback prediction.
            SB_NONE, // Setback of zero, ie no setback.
            SB_NONEMIN, // Some mixture of NONE and MIN.
            SB_MIN, // MIN setback.
              SB_NONEECO, // Some mixture of NONE, MIN and ECO.
            SB_MINECO, // Some mixture of MIN and ECO.
            SB_ECO, // ECO/medium setback.
            SB_ECOMAX, // Some mixture of ECO and MAX.
            SB_MAX, // Maximum setback.
            };
        const expectedSb_t expectedSb = NO_SB_EXPECTATION;

        // Day/hour/minute and light level and expected results.
        // An expected result of -1 means no particular result expected from this (anything is acceptable).
        // Else the given occType value is expected.
        constexpr ALDataSample(uint8_t dayOfMonth, uint8_t hour24, uint8_t minute,
                               uint8_t lightLevel,
                               int8_t expectedOcc_ = NO_OCC_EXPECTATION,
                               int8_t expectedRd_ = NO_RD_EXPECTATION,
                               int8_t actOcc_ = UNKNOWN_ACT_OCC,
                               expectedSb_t expectedSb_ = NO_SB_EXPECTATION)
            :
            d(dayOfMonth), H(hour24), M(minute),
            L(lightLevel),
            expectedOcc(expectedOcc_),
            expectedRd(expectedRd_),
            actOcc(actOcc_),
            expectedSb(expectedSb_)
            { }

//        // Alternative constructor with expected setback only.
//        // This cuts to the chase and the final desired outcome.
//        constexpr ALDataSample(uint8_t dayOfMonth, uint8_t hour24, uint8_t minute,
//                               uint8_t lightLevel,
//                               expectedSb_t expectedSb_ = NO_SB_EXPECTATION)
//            :
//            d(dayOfMonth), H(hour24), M(minute),
//            L(lightLevel),
//            expectedSb(expectedSb_)
//            { }

        // Create/mark a terminating entry; all input values invalid.
        constexpr ALDataSample() : d(255), H(255), M(255), L(255) { }

        // Compute current minute for this record.
        long currentMinute() const { return((((d * 24L) + H) * 60L) + M); }

        // True for empty/termination data record.
        bool isEnd() const { return(d > 31); }
    };

// Sample of count of 'flavoured' events/samples.
class SimpleFlavourStats final
    {
    private:
        unsigned n = 0;
        unsigned flavoured = 0;
    public:
        void zero() { n = 0; flavoured = 0; }
        void takeSample(bool isFlavoured) { ++n; if(isFlavoured) { ++flavoured; } }
        unsigned getSampleCount() const { return(n); }
        unsigned getFlavouredCount() const { return(flavoured); }
        float getFractionFlavoured() const { return(flavoured / (float) OTV0P2BASE::fnmax(1U, n)); }
    };

// Collection of 'flavoured' events in one run.
class SimpleFlavourStatCollection final
    {
    public:
        constexpr SimpleFlavourStatCollection(bool sensitive_, uint8_t blending_)
            : sensitive(sensitive_), blending(blending_) { }
        const bool sensitive;
        const uint8_t blending;

        // Count of number of samples counted as dark.
        // Checking for gross under- or over- reporting.
        SimpleFlavourStats roomDarkSamples;

        // Counting failures to meet specific room dark/light expectations.
        SimpleFlavourStats roomDarkPredictionErrors;

        // Count of ambient light occupancy callbaks.
        // Checking for gross over- reporting.
        SimpleFlavourStats ambLightOccupancyCallbacks;

        // Counting failures to meet specific occupancy callback expectations.
        SimpleFlavourStats ambLightOccupancyCallbackPredictionErrors;
//        // Checking ambient light level actual vs reported.
//        SimpleFlavourStats AmbientLightLevelFalsePositives;
//        SimpleFlavourStats AmbientLightLevelFalseNegatives;

        // Checking occupancy tracking accuracy vs actual occupation/vacancy.
        SimpleFlavourStats occupancyTrackingFalsePositives;
        SimpleFlavourStats occupancyTrackingFalseNegatives;

        // Checking setback accuracy vs actual occupation/vacancy.
        SimpleFlavourStats setbackTooFar; // Excessive setback.
        SimpleFlavourStats setbackInsufficient; // Insufficient setback.
    };

// Trivial sample, testing initial occupancy detector reaction to start transient.
static const ALDataSample trivialSample1[] =
    {
{ 0, 0, 0, 254, occType::OCC_NONE, false, false }, // Should NOT predict occupancy on first tick.
{ 0, 0, 1, 0, occType::OCC_NONE, true }, // Should NOT predict occupancy on falling level.
{ 0, 0, 5, 0, ALDataSample::NO_OCC_EXPECTATION, true }, // Should NOT predict occupancy on steady (dark) level, but have no expectation.
{ 0, 0, 6, 0, occType::OCC_NONE, true }, // Should NOT predict occupancy on steady (dark) level.
{ 0, 0, 9, 254, occType::OCC_PROBABLE }, // Should predict occupancy on level rising to (near) max.
{ }
    };

// Trivial sample, testing level response alongside some occupancy detection.
static const ALDataSample trivialSample2[] =
    {
{ 0, 0, 0, 254, ALDataSample::NO_OCC_EXPECTATION, false, false }, // Light.
{ 0, 0, 1, 0, occType::OCC_NONE, true }, // Dark.
{ 0, 0, 5, 0, ALDataSample::NO_OCC_EXPECTATION, true }, // Dark.
{ 0, 0, 6, 0, occType::OCC_NONE, true }, // Dark.
{ 0, 0, 9, 254, occType::OCC_PROBABLE }, // Light but no prediction made.
{ }
    };

// Trivial sample, testing level only.
static const ALDataSample trivialSample3[] =
    {
{ 0, 0, 0, 254, ALDataSample::NO_OCC_EXPECTATION, false, false }, // Light.
{ 0, 0, 1, 0, ALDataSample::NO_OCC_EXPECTATION, true }, // Dark.
{ 0, 0, 5, 0, ALDataSample::NO_OCC_EXPECTATION, true }, // Dark.
{ 0, 0, 6, 0, ALDataSample::NO_OCC_EXPECTATION, true }, // Dark.
{ 0, 0, 9, 254, ALDataSample::NO_OCC_EXPECTATION }, // Light but no prediction made.
{ }
    };

// Support state for simpleDataSampleRun().
namespace SDSR
    {
    static OTV0P2BASE::PseudoSensorOccupancyTracker occupancy;
    static OTV0P2BASE::SensorAmbientLightAdaptiveMock ambLight;
    // In-memory stats set.
    static OTV0P2BASE::NVByHourByteStatsMock hs;
    // Dummy (non-functioning) temperature and relative humidity sensors.
    static OTV0P2BASE::TemperatureC16Mock tempC16;
    static OTV0P2BASE::DummyHumiditySensor rh;
    // Two-subsamples per hour stats sampling.
    static OTV0P2BASE::ByHourSimpleStatsUpdaterSampleStats <
      decltype(hs), &hs,
      decltype(occupancy), &occupancy,
      decltype(ambLight), &ambLight,
      decltype(tempC16), &tempC16,
      decltype(rh), &rh,
      2
      > su;
    // Support for cttb instance.
    static OTRadValve::ValveMode valveMode;
    typedef OTRadValve::DEFAULT_ValveControlParameters parameters;
    static OTRadValve::TempControlSimpleVCP<parameters> tempControl;
    static OTRadValve::NULLActuatorPhysicalUI physicalUI;
    static OTV0P2BASE::NULLValveSchedule schedule;
    // Simple-as-possible instance.
    static OTRadValve::ModelledRadValveComputeTargetTempBasic<
       parameters,
        &valveMode,
        decltype(tempC16),                            &tempC16,
        decltype(tempControl),                        &tempControl,
        decltype(occupancy),                          &occupancy,
        decltype(ambLight),                           &ambLight,
        decltype(physicalUI),                         &physicalUI,
        decltype(schedule),                           &schedule,
        decltype(hs),                                 &hs
        > cttb;
    // Occupancy callback.
    static int8_t cbProbable;
    static void (*const callback)(bool) = [](bool p)
        {
        cbProbable = p;
        if(p) { occupancy.markAsPossiblyOccupied(); } else { occupancy.markAsJustPossiblyOccupied(); }
if(veryVerbose) { fprintf(stderr, " *Callback: %d\n", p); }
        };
    // Reset all these static entities but does not clear stats.
    static void resetAll()
        {
        // Set up room to be dark and vacant.
        ambLight.resetAdaptive();
        occupancy.reset();
        // Flush any partial samples.
        su.reset();
        // Reset valve-level controls.
        valveMode.setWarmModeDebounced(true);
        physicalUI.read();
        // Install the occupancy tracker callback from the ambient light sensor.
        ambLight.setOccCallbackOpt(callback);
        }
    }
// Score actual setback against expected setback.
// This is arguably the key metric, ie closest to the desired outcone,
// of energy savings and comfort being achieved.
// Sets 'failed' to true if at least one matric was a fail for this point.
template<class Valve_parameters>
static void scoreSetback(
    const uint8_t setback, const ALDataSample::expectedSb_t expectedSb,
    SimpleFlavourStats &setbackInsufficient, SimpleFlavourStats &setbackTooFar,
    bool &failed)
    {
    bool tooFar = false;
    bool insufficient = false;

    switch(expectedSb)
        {
        // No scoring to do if no expectation.
        // This does not even get a tick for the counts.
        case ALDataSample::NO_SB_EXPECTATION: { return; }

        // Setback of zero, ie no setback.
        // Any setback is too much; zero setback is good.
        case ALDataSample::SB_NONE:
           {
           tooFar = (0 != setback);
           break;
           }

       // NINE/minimum setback micture.
       // Up to MIN setback is acceptable.
       case ALDataSample::SB_NONEMIN:
           {
           tooFar = (setback > Valve_parameters::SETBACK_DEFAULT);
           break;
           }

       // Minimum setback.
       // Exactly ECO setback is acceptable.
       case ALDataSample::SB_MIN:
           {
           insufficient = (setback < Valve_parameters::SETBACK_DEFAULT);
           tooFar = (setback > Valve_parameters::SETBACK_DEFAULT);
           break;
           }

       // Some mixture of NONE (and MIN) and ECO.
       // A setback up to ECO inclusive is OK.
       case ALDataSample::SB_NONEECO:
           {
           tooFar = (setback > Valve_parameters::SETBACK_ECO);
           break;
           }

       // Some mixture of MIN and ECO.
       // A setback from MIN up to ECO inclusive is OK.
       case ALDataSample::SB_MINECO:
           {
           insufficient = (setback < Valve_parameters::SETBACK_DEFAULT);
           tooFar = (setback > Valve_parameters::SETBACK_ECO);
           break;
           }

        // ECO/medium setback.
        // Exactly ECO setback is acceptable.
        case ALDataSample::SB_ECO:
            {
            insufficient = (setback < Valve_parameters::SETBACK_ECO);
            tooFar = (setback > Valve_parameters::SETBACK_ECO);
            break;
            }

        // Some mixture of ECO and MAX.
        // A setback of at least ECO is good; there is no 'too much'.
        case ALDataSample::SB_ECOMAX:
            {
            insufficient = (setback < Valve_parameters::SETBACK_ECO);
            break;
            }

        // Maximum setback.
        // A setback less than FULL is insufficient; there is no 'too much'.
        case ALDataSample::SB_MAX:
            {
            insufficient = (setback < Valve_parameters::SETBACK_FULL);
            break;
            }

        default: { FAIL(); return; } // Unexpected expectation value.
        }

    setbackTooFar.takeSample(tooFar);
    setbackInsufficient.takeSample(insufficient);

    if(tooFar || insufficient) { failed = true; }
    }

// Compute and when appropriate set stats parameters to the ambient light sensor.
// Assumes that it is called in strictly monotonic increasing time
// incrementing one minute each time, wrapping as 23:59.
// Before the first call on one run of data oldH should be set to 0xff.
enum blending_t : uint8_t { BL_FROMSTATS, BL_NONE, BL_HALFHOURMIN, BL_HALFHOUR, BL_BYMINUTE, BL_END };
void setTypeMinMax(OTV0P2BASE::SensorAmbientLightAdaptiveMock &ala,
        const blending_t blending,
        const uint8_t H, const uint8_t M,
        const uint8_t minToUse, const uint8_t maxToUse, const bool sensitive,
        const uint8_t byHourMeanI[24],
        const OTV0P2BASE::NVByHourByteStatsMock &hs,
        uint8_t &oldH,
        uint8_t &meanUsed)
    {
    meanUsed = 0xff;
    switch(blending)
        {
        case BL_NONE: // Use unblended mean for this hour.
            {
            meanUsed = byHourMeanI[H];
            if(H != oldH)
                {
                // When the hour rolls, set new stats for the detector.
                // Note that implementations be use the end of the hour/period
                // and other times.
                // The detector and caller should aim not to be hugely sensitive to the exact timing,
                // eg by blending prev/current/next periods linearly.
                ala.setTypMinMax(byHourMeanI[H], minToUse, maxToUse, sensitive);
                }
            break;
            }
        case BL_HALFHOURMIN: // Use blended (min) mean for final half hour hour.
            {
            const uint8_t thm = byHourMeanI[H];
            const uint8_t nhm = byHourMeanI[(H+1)%24];
            uint8_t m = thm; // Default to this hour's mean.
            if(M >= 30)
                {
                // In last half hour of each hour...
                if(0xff == thm) { m = nhm; } // Use next hour mean if none available for this hour.
                else if(0xff != nhm) { m = OTV0P2BASE::fnmin(nhm, thm); } // Take min when both hours' means available.
                }
            meanUsed = m;
            ala.setTypMinMax(m, minToUse, maxToUse, sensitive);
            break;
            }
        case BL_HALFHOUR: // Use blended mean for final half hour hour.
            {
            const uint8_t thm = byHourMeanI[H];
            const uint8_t nhm = byHourMeanI[(H+1)%24];
            uint8_t m = thm; // Default to this hour's mean.
            if(M >= 30)
                {
                // In last half hour of each hour...
                if(0xff == thm) { m = nhm; } // Use next hour mean if none available for this hour.
                else if(0xff != nhm) { m = uint8_t((thm + (uint_fast16_t)nhm + 1) / 2); } // Take mean when both hours' means available.
                }
            meanUsed = m;
            ala.setTypMinMax(m, minToUse, maxToUse, sensitive);
            break;
            }
        case BL_BYMINUTE: // Adjust blend by minute.
            {
            const uint8_t thm = byHourMeanI[H];
            const uint8_t nhm = byHourMeanI[(H+1)%24];
            uint8_t m = thm; // Default to this hour's mean.
            if(0xff == thm) { m = nhm; } // Use next hour's mean always if this one's not available.
            else
                {
                // Continuous blend.
                m = uint8_t(((((uint_fast16_t)thm) * (60-M)) + (((uint_fast16_t)nhm) * M) + 30) / 60);
                }
            meanUsed = m;
            ala.setTypMinMax(m, minToUse, maxToUse, sensitive);
            break;
            }
        case BL_FROMSTATS: // From the smoothed rolling stats.
            {
            const uint8_t m = hs.getByHourStatSimple(OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, H);
            meanUsed = m;
            if(H != oldH)
                {
                // When the hour rolls, set new stats for the detector.
                // Note that implementations be use the end of the hour/period
                // and other times.
                // The detector and caller should aim not to be hugely sensitive to the exact timing,
                // eg by blending prev/current/next periods linearly.
                ala.setTypMinMax(m, minToUse, maxToUse, sensitive);
                }
            break;
            }
        default: FAIL();
        }
    oldH = H;

    }
// Check that the occupancy/setback/etc results are acceptable for the data.
// Makes the test fail via EXPECT_XX() etc if not.
static void checkAccuracyAcceptableAgainstData(
        const SimpleFlavourStatCollection &flavourStats)
    {
    const bool sensitive = flavourStats.sensitive;
    const bool oddBlend = (flavourStats.blending != BL_FROMSTATS);
    const bool normalOperation = !sensitive && !oddBlend;

    // Check that at least some expectations have been set.
//            ASSERT_NE(0U, flavourStats.AmbLightOccupancyCallbackPredictionErrors.getSampleCount()) << "some expected occupancy callbacks should be provided";
    ASSERT_NE(0U, flavourStats.roomDarkPredictionErrors.getSampleCount()) << "some known room dark values should be provided";
    ASSERT_NE(0U, flavourStats.occupancyTrackingFalseNegatives.getSampleCount()) << "some known occupancy values should be provided";

    // Check that there are not huge numbers of (false) positive occupancy reports.
    EXPECT_GE(0.24f, flavourStats.ambLightOccupancyCallbacks.getFractionFlavoured());

    // Check that there are not huge numbers of failed callback expectations.
    // We could allow more errors with an odd (non-deployment) blending.
    EXPECT_GE(0.067f, flavourStats.ambLightOccupancyCallbackPredictionErrors.getFractionFlavoured());

    // Check that there are not huge numbers of failed dark expectations.
    EXPECT_GE(0.15f, flavourStats.roomDarkPredictionErrors.getFractionFlavoured()) << flavourStats.roomDarkPredictionErrors.getSampleCount();

    // Check that there is a reasonable balance between room dark/light.
    const float rdFraction = flavourStats.roomDarkSamples.getFractionFlavoured();
    EXPECT_LE(0.4f, rdFraction);
    EXPECT_GE(0.8f, rdFraction);

    // Check that number of false positives and negatives
    // from occupancy tracked fed from ambient light reports is OK.
    // When 'sensitive', eg in comfort mode,
    // more false positives and fewer false negatives are OK.
    // But accept more errors generally with non-preferred blending.
    // Excess false positives likely inhibit energy saving.
    // The FIRST (tighter) limit is the more critical one for normal operation.
    EXPECT_GE((normalOperation ? 0.1f : 0.122f), flavourStats.occupancyTrackingFalsePositives.getFractionFlavoured());
    // Excess false negatives may cause discomfort.
    EXPECT_GE((normalOperation ? 0.1f : 0.23f), flavourStats.occupancyTrackingFalseNegatives.getFractionFlavoured());

    // Check that setback accuracy is OK.
    // Aim for a low error rate in either direction.
    EXPECT_GE((flavourStats.sensitive ? 0.12f : 0.1f), flavourStats.setbackInsufficient.getFractionFlavoured());
    EXPECT_GE((oddBlend ? 0.145f : 0.1f), flavourStats.setbackTooFar.getFractionFlavoured());
    }
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
void simpleDataSampleRun(const ALDataSample *const data)
    {
    ASSERT_TRUE(NULL != data);
    ASSERT_FALSE(data->isEnd()) << "do not pass in empty data set";

    // Clear stats backing store.
    SDSR::hs.zapStats();

    // First count records and set up testing state.
    // The ambient light sensor is not being fed back stats
    // with setTypMinMax() and so is using its default parameters.

    // Clear all state in static instances.
    SDSR::resetAll();
    // Ambient light sensor instance under test.
    OTV0P2BASE::SensorAmbientLightAdaptiveMock &ala = SDSR::ambLight;
    // Occupancy tracker instance under test, to check system behaviour.
    OTV0P2BASE::PseudoSensorOccupancyTracker &tracker = SDSR::occupancy;

    // Some basic sense-checking of the set-up state.
    ASSERT_EQ(0, tracker.get());
    ASSERT_FALSE(tracker.isLikelyOccupied());
    // As room starts dark and vacant, expect a setback initially.
    static constexpr uint8_t WARM = SDSR::parameters::WARM;
    static constexpr uint8_t FROST = SDSR::parameters::FROST;
    ASSERT_GE(WARM, SDSR::cttb.computeTargetTemp());
    ASSERT_LE(FROST, SDSR::cttb.computeTargetTemp());

    // Count of number of records.
    int nRecords = 0;
    // Count number of records with explicit expected occupancy response assertion.
    int nOccExpectation = 0;
    // Count number of records with explicit expected room-dark response assertion.
    int nRdExpectation = 0;
    // Compute own values for min, max, etc.
    int minI = 256;
    int maxI = -1;
    uint8_t byHourMeanI[24];
    int byHourMeanSumI[24]; memset(byHourMeanSumI, 0, sizeof(byHourMeanSumI));
    int byHourMeanCountI[24]; memset(byHourMeanCountI, 0, sizeof(byHourMeanCountI));
    for(const ALDataSample *dp = data; !dp->isEnd(); ++dp)
        {
        ++nRecords;
        const int8_t neo = ALDataSample::NO_OCC_EXPECTATION;
        if(neo != dp->expectedOcc) { ++nOccExpectation; }
        const int8_t ner = ALDataSample::NO_RD_EXPECTATION;
        if(ner != dp->expectedRd) { ++nRdExpectation; }
        long currentMinute = dp->currentMinute();
        do  {
            const uint8_t level = dp->L;
            if((int)level < minI) { minI = level; }
            if((int)level > maxI) { maxI = level; }
            const uint8_t H = (currentMinute % 1440) / 60;
            const uint8_t M = (currentMinute % 60);
            if(29 == M) { SDSR::su.sampleStats(false, H); }
            if(59 == M) { SDSR::su.sampleStats(true, H); }
            byHourMeanSumI[H] += level;
            ++byHourMeanCountI[H];
            ++currentMinute;
            ala.set(dp->L); ala.read(); tracker.read();
            } while((!(dp+1)->isEnd()) && (currentMinute < (dp+1)->currentMinute()));
        }
    ASSERT_TRUE((nOccExpectation > 0) || (nRdExpectation > 0)) << "must assert some expected predictions";
    for(int i = 24; --i >= 0; )
        {
        if(0 != byHourMeanCountI[i])
            { byHourMeanI[i] = (uint8_t)((byHourMeanSumI[i] + (byHourMeanCountI[i]>>1)) / byHourMeanCountI[i]); }
        else { byHourMeanI[i] = 0xff; }
        }

    // Take an initial copy of the stats.
    const OTV0P2BASE::NVByHourByteStatsMock hsInitCopy = SDSR::hs;

    const uint8_t minToUse = hsInitCopy.getMinByHourStat(hsInitCopy.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED);
    const uint8_t maxToUse = hsInitCopy.getMaxByHourStat(hsInitCopy.STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED);

    // Dump some of the data collected.
    if(verbose)
        {
        fprintf(stderr, "STATS:\n");

        fprintf(stderr, "  min: %d\n", minI);
        fprintf(stderr, "  max: %d\n", maxI);

        fprintf(stderr, "  min from stats: %d\n", minToUse);
        fprintf(stderr, "  max from stats: %d\n", maxToUse);

        fprintf(stderr, "  mean ambient light level by hour:");
        for(int i = 0; i < 24; ++i)
            {
            fputc(' ', stderr);
            const uint8_t v = byHourMeanI[i];
            if(0xff == v) { fputc('-', stderr); }
            else { fprintf(stderr, "%d", (int)v); }
            }
        fprintf(stderr, "\n");

        fprintf(stderr, " smoothed ambient light level: ");
        for(int i = 0; i < 24; ++i)
            {
            fputc(' ', stderr);
            const uint8_t v = hsInitCopy.getByHourStatSimple(OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED, i);
            if(0xff == v) { fputc('-', stderr); }
            else { fprintf(stderr, "%d", v); }
            }
        fprintf(stderr, "\n");

        fprintf(stderr, " smoothed occupancy: ");
        for(int i = 0; i < 24; ++i)
            {
            fputc(' ', stderr);
            const uint8_t v = hsInitCopy.getByHourStatSimple(OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_OCCPC_BY_HOUR_SMOOTHED, i);
            if(0xff == v) { fputc('-', stderr); }
            else { fprintf(stderr, "%d", v); }
            }
        fprintf(stderr, "\n");
        }

    // Now run through all the data checking responses.
    // Run simulation with different stats blending types
    // to ensure that occupancy detection is robust.
    // The BL_FROMSTATS case is most like the real embedded code.
    for(uint8_t blending = 0; blending < BL_END; ++blending)
        {
if(verbose) { fprintf(stderr, "blending = %d\n", blending); }
        SCOPED_TRACE(testing::Message() << "blending " << (int)blending);
        // The preferred blend (most like a real deployment) is FROMSTATS.
        const bool oddBlend = (BL_FROMSTATS != blending);

        // Run simulation at both sensitivities.
        int nOccupancyReportsSensitive = 0;
        int nOccupancyReportsNotSensitive = 0;
        for(int s = 0; s <= 1; ++s)
            {
            const bool sensitive = (0 != s);
if(verbose) { fputs(sensitive ? "sensitive\n" : "not sensitive\n", stderr); }
            SCOPED_TRACE(testing::Message() << "sensitive " << sensitive);

            // Reset stats to end of main warm-up run.
            SDSR::hs = hsInitCopy;

            // Now run a warmup to get stats into correct state.
            // Stats are rolled over from the warmpup to the final run.
            // Results will be ignored during this warmup.

            for(int w = 0; w < 2; ++w)
                {
                const bool warmup = (0 == w);

                // Suppress most reporting for odd blends and in warmup.
const bool verboseOutput = !warmup && (veryVerbose || (verbose && !oddBlend));

                SimpleFlavourStatCollection flavourStats(sensitive, blending);

                // Clear all state in static instances (except stats).
                SDSR::resetAll();
                // Ambient light sensor instance under test.
                OTV0P2BASE::SensorAmbientLightAdaptiveMock &ala = SDSR::ambLight;
                // Occupancy tracker instance under test, to check system behaviour.
                OTV0P2BASE::PseudoSensorOccupancyTracker &tracker = SDSR::occupancy;
                ASSERT_EQ(0, tracker.get());
                ASSERT_FALSE(tracker.isLikelyOccupied());

                uint8_t oldH = 0xff; // Used to detect hour rollover.
                for(const ALDataSample *dp = data; !dp->isEnd(); ++dp)
                    {
                    long currentMinute = dp->currentMinute();
                    do  {
                        const uint8_t D = (currentMinute / 1440);
                        const uint8_t H = (currentMinute % 1440) / 60;
                        const uint8_t M = (currentMinute % 60);
                        uint8_t meanUsed = 0xff;
                        setTypeMinMax(ala,
                                (blending_t)blending,
                                H, M,
                                minToUse, maxToUse, sensitive,
                                byHourMeanI,
                                hsInitCopy,
                                oldH,
                                meanUsed);

                        // Capture some 'before' values for failure analysis.
                        const uint8_t beforeSteadyTicks = ala._occDet._getSteadyTicks();

//fprintf(stderr, "L=%d @ %dT%d:%.2d\n", dp->L, D, H, M);
                        // About to perform another virtual minute 'tick' update.
                        SDSR::cbProbable = -1; // Collect occupancy prediction (if any) from call-back.
                        ala.set(dp->L);
                        ala.read();
                        tracker.read();

                        // Get hourly stats sampled and updated.
                        if(29 == M) { SDSR::su.sampleStats(false, H); }
                        if(59 == M) { SDSR::su.sampleStats(true, H); }

//if(veryVerbose && tracker.isLikelyOccupied()) { fprintf(stderr, "O=%d @ %dT%d:%.2d\n", (int)tracker.get(), D, H, M); }

                        // Check predictions/calculations against explicit expectations.
                        // True if real non-interpolated record.
                        const bool isRealRecord = (currentMinute == dp->currentMinute());
                        const bool predictedRoomDark = ala.isRoomDark();
                        flavourStats.roomDarkSamples.takeSample(predictedRoomDark);
                        const int8_t expectedRoomDark = (!isRealRecord) ? ALDataSample::NO_RD_EXPECTATION : dp->expectedRd;
                        // Collect occupancy prediction (if any) from call-back.
                        const OTV0P2BASE::SensorAmbientLightOccupancyDetectorInterface::occType predictionOcc =
                            (-1 == SDSR::cbProbable) ? occType::OCC_NONE :
                                ((0 == SDSR::cbProbable) ? occType::OCC_WEAK : occType::OCC_PROBABLE);
//if(veryVerbose && (-1 != cbProbable)) { fprintf(stderr, "  occupancy callback=%d @ %dT%d:%.2d\n", cbProbable, D, H, M); }
                        if(isRealRecord) { flavourStats.ambLightOccupancyCallbacks.takeSample((-1 != SDSR::cbProbable)); }
                        // Collect occupancy tracker prediction and error.
                        if(isRealRecord && (ALDataSample::UNKNOWN_ACT_OCC != dp->actOcc))
                            {
                            const bool trackedLikelyOccupancy = tracker.isLikelyOccupied();
                            const bool actOcc = bool(dp->actOcc);
if(verbose && !warmup && (trackedLikelyOccupancy != actOcc)) { fprintf(stderr, "!!!actual occupancy=%d @ %dT%d:%.2d L=%d mean=%d tracker=%d\n", dp->actOcc, D, H, M, dp->L, meanUsed, (int)tracker.get()); }
                            flavourStats.occupancyTrackingFalseNegatives.takeSample(actOcc && !trackedLikelyOccupancy);
                            flavourStats.occupancyTrackingFalsePositives.takeSample(!actOcc && trackedLikelyOccupancy);
                            }

if(veryVerbose && verboseOutput && isRealRecord) { fprintf(stderr, "  tS=%d @ %dT%d:%.2d\n", SDSR::tempControl.getWARMTargetC() - SDSR::cttb.computeTargetTemp(), D, H, M); }
                        if(isRealRecord && (ALDataSample::NO_SB_EXPECTATION != dp->expectedSb))
                            {
                            const int8_t setback = SDSR::tempControl.getWARMTargetC() - SDSR::cttb.computeTargetTemp();
                            bool failed = false;
                            scoreSetback<SDSR::parameters>(setback, dp->expectedSb,
                                flavourStats.setbackInsufficient, flavourStats.setbackTooFar,
                                failed);
if(verbose && !warmup && failed) { fprintf(stderr, "!!!tS=%d @ %dT%d:%.2d expectation=%d\n", setback, D, H, M, dp->expectedSb); }
                            }

                        // Note that for all synthetic ticks the expectation is removed (since there is no level change).
                        const int8_t expectedOcc = (!isRealRecord) ? ALDataSample::NO_OCC_EXPECTATION : dp->expectedOcc;
if(veryVerbose && verboseOutput && isRealRecord && (occType::OCC_NONE != predictionOcc)) { fprintf(stderr, "  predictionOcc=%d @ %dT%d:%.2d L=%d mean=%d\n", predictionOcc, D, H, M, dp->L, meanUsed); }
                        if(ALDataSample::NO_OCC_EXPECTATION != expectedOcc)
                            {
                            flavourStats.ambLightOccupancyCallbackPredictionErrors.takeSample(expectedOcc != predictionOcc);
if(verbose && !warmup && (expectedOcc != predictionOcc)) { fprintf(stderr, "!!!expectedOcc=%d @ %dT%d:%.2d L=%d mean=%d beforeSteadyTicks=%d\n", expectedOcc, D, H, M, dp->L, meanUsed, beforeSteadyTicks); }
                            }
                        if(ALDataSample::NO_RD_EXPECTATION != expectedRoomDark)
                            {
                            flavourStats.roomDarkPredictionErrors.takeSample((bool)expectedRoomDark != predictedRoomDark);
if(verbose && !warmup && ((bool)expectedRoomDark != predictedRoomDark)) { fprintf(stderr, "!!!expectedDark=%d @ %dT%d:%.2d L=%d mean=%d\n", expectedRoomDark, D, H, M, dp->L, meanUsed); }
                            }

                        ++currentMinute;
                        } while((!(dp+1)->isEnd()) && (currentMinute < (dp+1)->currentMinute()));
                    }

                // Don't test results in wormup run.
                if(!warmup)
                    {
                    checkAccuracyAcceptableAgainstData(flavourStats);
                    // Allow check in outer loop that sensitive mode generates
                    // at least as many reports as non-sensitive mode.
                    if(sensitive) { nOccupancyReportsSensitive = flavourStats.ambLightOccupancyCallbacks.getFlavouredCount(); }
                    else { nOccupancyReportsNotSensitive = flavourStats.ambLightOccupancyCallbacks.getFlavouredCount(); }
                    }
                }
            }
        // Check that sensitive mode generates at least as many reports as non.
        EXPECT_LE(nOccupancyReportsNotSensitive, nOccupancyReportsSensitive) << "expect sensitive never to generate fewer reports";
        }
    }

// Basic test of update() behaviour.
TEST(AmbientLightOccupancyDetection,simpleDataSampleRun)
{
    simpleDataSampleRun(trivialSample1);
    simpleDataSampleRun(trivialSample2);
    simpleDataSampleRun(trivialSample3);
}

// "3l" 2016/10/08+09 test set with tough occupancy to detect in the evening up to 21:00Z and in the morning from 07:09Z then  06:37Z.
static const ALDataSample sample3lHard[] =
    {
{8,0,1,1, occType::OCC_NONE, true, false, ALDataSample::SB_MINECO}, // Definitely not occupied; should be at least somewhat setback immediately.
{8,0,17,1, occType::OCC_NONE, true, false, ALDataSample::SB_MINECO}, // Definitely not occupied; should be at least somewhat setback immediately.
//...
{8,6,21,1},
{8,6,29,2, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not enough rise to indicate occupation, still dark, running long enough for max setback.
{8,6,33,2},
{8,6,45,2},
{8,6,57,2, occType::OCC_NONE, true, false}, // Not enough light to indicate occupation, dark.
{8,7,9,14, ALDataSample::NO_OCC_EXPECTATION, ALDataSample::NO_RD_EXPECTATION},  // Temporarily occupied: curtains drawn?  Borderline dark?
{8,7,17,35},
{8,7,21,38},
{8,7,33,84, occType::OCC_PROBABLE, false, true}, // Lights on or more curtains drawn?  Possibly occupied.
{8,7,37,95},
{8,7,49,97}, // Was: "occType::OCC_NONE, not enough rise to be occupation" but in this case after likely recent OCC_PROBABLE not materially important.
{8,7,57,93, occType::OCC_NONE, false}, // Fall is not indicative of occupation.
{8,8,5,98, occType::OCC_NONE, false}, // Sun coming up: not enough rise to indicate occupation.
{8,8,13,98},
{8,8,17,93},
{8,8,25,79},
{8,8,33,103},
{8,8,41,118},
{8,8,49,106},
{8,8,53,92},
{8,8,57,103},
{8,9,5,104, occType::OCC_NONE, false, false}, // Light, unoccupied.
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
{8,12,9,181, ALDataSample::NO_OCC_EXPECTATION, false}, // Light.
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
{8,17,37,44, occType::OCC_PROBABLE, false, true}, // OCCUPIED (light on?).
{8,17,49,42},
{8,18,1,42, occType::OCC_WEAK, false, true}, // Light on, watching TV?
{8,18,9,40},
{8,18,13,42, occType::OCC_WEAK, false, true}, // Light on, watching TV?
{8,18,25,40},
{8,18,37,40, occType::OCC_WEAK, false, true}, // Light on, watching TV?
{8,18,41,42, occType::OCC_WEAK, false, true}, // Light on, watching TV?
{8,18,49,42, occType::OCC_WEAK, false, true}, // Light on, watching TV?
{8,18,57,41},
{8,19,1,40},
{8,19,13,41, occType::OCC_WEAK, false, true}, // Light on, watching TV?
{8,19,21,39},
{8,19,25,41}, // ... more WEAK signals should follow...
{8,19,41,41},
{8,19,52,42},
{8,19,57,40},
{8,20,5,40},
{8,20,9,42, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Ideally, on bigger data set... occType::OCC_WEAK}, // Light on, watching TV?
{8,20,17,42},
{8,20,23,40},
{8,20,29,40, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Ideally, on bigger data set... occType::OCC_WEAK}, // Light on, watching TV?
{8,20,33,40},
{8,20,37,41},
{8,20,41,42, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Ideally, on bigger data set... occType::OCC_WEAK}, // Light on, watching TV?
{8,20,49,40},
{8,21,5,1, occType::OCC_NONE, true}, // Just vacated, dark.
{8,21,13,1, occType::OCC_NONE, true, false}, // Definitely not occupied.
// ...
{9,5,57,1, occType::OCC_NONE, true, false}, // Definitely not occupied.
{9,6,13,1, occType::OCC_NONE, true, false}, // Definitely not occupied.
{9,6,21,2, occType::OCC_NONE, true, false}, // Not enough rise to indicate occupation, dark.
{9,6,33,2, occType::OCC_NONE, true, false}, // Not enough light to indicate occupation, dark.
{9,6,37,24, occType::OCC_PROBABLE, false, true}, // Curtains drawn: OCCUPIED. Should appear light.
{9,6,45,32},
{9,6,53,31},
{9,7,5,30},
{9,7,17,41},
{9,7,25,54},
{9,7,33,63, occType::OCC_NONE, false}, // Sun coming up; not a sign of occupancy.
{9,7,41,73, occType::OCC_NONE, false}, // Sun coming up; not a sign of occupancy.
{9,7,45,77, occType::OCC_NONE, false}, // Sun coming up: not enough rise to indicate occupation.
{ }
    };
// Test with real data set.
TEST(AmbientLightOccupancyDetection,sample3lHard)
{
    simpleDataSampleRun(sample3lHard);
}

// "3l" 2016/12/01+02 test for dark/light detection overnight.
// (Full setback was not achieved; verify that night sensed as dark.)
static const ALDataSample sample3lLevels[] =
    {
{1,0,7,2, ALDataSample::NO_OCC_EXPECTATION, true, false}, // Dark.
{1,0,19,2},
// ...
{1,5,39,2, ALDataSample::NO_OCC_EXPECTATION, true, false}, // Dark.
{1,5,55,2},
{1,6,11,3},
{1,6,24,2},
{1,6,39,2},
{1,6,55,2},
{1,7,11,3, ALDataSample::NO_OCC_EXPECTATION, true, false}, // Dark.
{1,7,31,5},
{1,7,47,13},
{1,7,55,19},
{1,8,3,26},
{1,8,19,35},
{1,8,27,39},
{1,8,35,46},
{1,8,51,58},
{1,9,7,73},
{1,9,18,51},
{1,9,20,49},
{1,9,24,43},
{1,9,29,116},
{1,9,45,129},
{1,9,48,130},
{1,9,57,133},
{1,10,9,138},
{1,10,17,142},
{1,10,29,147},
{1,10,45,163},
{1,10,49,167},
{1,11,5,167},
{1,11,21,168},
{1,11,41,173},
{1,11,48,174},
{1,11,53,175},
{1,12,9,176},
{1,12,13,176},
{1,12,29,177},
{1,12,45,178, ALDataSample::NO_OCC_EXPECTATION, ALDataSample::NO_RD_EXPECTATION, false},
{1,13,5,179},
{1,13,21,179, ALDataSample::NO_OCC_EXPECTATION, ALDataSample::NO_RD_EXPECTATION, false},
{1,13,35,181},
{1,13,45,182},
{1,13,49,182},
{1,14,1,182},
{1,14,13,183},
{1,14,17,180},
{1,14,28,154},
{1,14,41,142},
{1,14,45,138},
{1,15,1,125, ALDataSample::NO_OCC_EXPECTATION, ALDataSample::NO_RD_EXPECTATION, false},
{1,15,17,95},
{1,15,21,87},
{1,15,33,67},
{1,15,45,44},
{1,15,49,32},
{1,16,1,25},
{1,16,13,43},
{1,16,25,52},
{1,16,28,51},
{1,16,45,41},
{1,16,53,41},
{1,17,5,41},
{1,17,17,39},
{1,17,29,40},
{1,17,33,38},
{1,17,45,12},
{1,17,57,42},
{1,18,1,3},
{1,18,9,41, occType::OCC_PROBABLE, false, true}, // TV watching
{1,18,29,40},
{1,18,49,39},
{1,18,57,39},
{1,19,5,39},
{1,19,21,37},
{1,19,33,40, occType::OCC_WEAK, false, true},
{1,19,53,39},
{1,19,57,38},
{1,20,9,38},
{1,20,21,40},
{1,20,23,40},
{1,20,41,39},
{1,20,45,39, occType::OCC_WEAK, false, true},
{1,21,1,38},
{1,21,21,40},
{1,21,25,39},
{1,21,41,39},
{1,21,45,40},
{1,21,53,39},
{1,22,9,2, ALDataSample::NO_OCC_EXPECTATION, true, false}, // Dark.
{1,22,29,2},
{1,22,49,2},
{1,23,5,2},
{1,23,18,2},
{1,23,27,2},
{1,23,48,2},
{2,0,1,2, ALDataSample::NO_OCC_EXPECTATION, true, false}, // Dark.
{2,0,17,2},
{2,0,33,2},
{2,0,49,2},
{2,1,1,2},
{2,1,17,2},
{2,1,33,2},
{2,1,57,2},
{2,2,9,2},
{2,2,29,2, ALDataSample::NO_OCC_EXPECTATION, true, false}, // Dark.
{2,2,49,2},
{2,3,5,2},
{2,3,25,2},
{2,3,41,2},
{2,3,57,2},
{2,4,9,2},
{2,4,25,2},
{2,4,41,2},
{2,4,57,2, ALDataSample::NO_OCC_EXPECTATION, true, false}, // Dark.
{2,5,13,2},
{2,5,33,2},
{2,5,49,2},
{2,6,1,2},
{2,6,17,2},
{2,6,33,2},
{2,6,49,2},
{2,7,5,2},
{2,7,17,3},
{2,7,21,3},
{2,7,29,3, ALDataSample::NO_OCC_EXPECTATION, true, false}, // Dark.
{2,7,37,4},
{2,7,45,6},
{2,8,1,13},
{2,8,2,14},
{2,8,21,25},
{2,8,33,28},
{2,8,49,24},
{2,8,53,29},
{2,9,4,35},
{2,9,13,49},
{2,9,17,51},
{2,9,33,70},
{2,9,37,73},
{2,9,45,184},
{2,9,45,183},
{2,9,49,45},
{2,9,55,85},
{2,10,11,95},
{2,10,15,96},
{2,10,24,103},
{2,10,39,113},
{2,10,43,114},
{ }
    };
// Test with real data set.
TEST(AmbientLightOccupancyDetection,sample3lLevels)
{
    simpleDataSampleRun(sample3lLevels);
}

// "5s" 2016/10/08+09 test set with tough occupancy to detect in the evening 21:00Z.
static const ALDataSample sample5sHard[] =
    {
{8,0,3,2, occType::OCC_NONE, true}, // Not occupied actively.
{8,0,19,2, occType::OCC_NONE, true, false, ALDataSample::SB_ECOMAX}, // Not occupied actively, sleeping, good setback (may be too soon after data set start to hit max).
// ...
{8,5,19,2, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{8,5,31,1, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{8,5,43,2, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
// ...
{8,6,23,4, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{8,6,35,6, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{8,6,39,5, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{8,6,51,6, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{8,7,3,9, occType::OCC_NONE, ALDataSample::NO_RD_EXPECTATION, false}, // Not occupied actively.
{8,7,11,12},
{8,7,15,13},
{8,7,19,17},
{8,7,27,42, occType::OCC_PROBABLE, false, ALDataSample::UNKNOWN_ACT_OCC, ALDataSample::SB_NONEECO}, // FIXME: should detect curtains drawn?  Temporary occupancy.  Should at least be anticipating occupancy.
{8,7,31,68, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::UNKNOWN_ACT_OCC, ALDataSample::SB_NONEECO}, // Should at least be anticipating occupancy.
{8,7,43,38},
{8,7,51,55},
{8,7,55,63},
{8,7,59,69},
{8,8,11,68, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::UNKNOWN_ACT_OCC, ALDataSample::SB_NONEECO}, // Daylight, setback should be limited.
{8,8,15,74},
{8,8,27,72},
{8,8,43,59},
{8,8,51,38},
{8,8,55,37},
{8,8,59,34},
{8,9,3,43, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::UNKNOWN_ACT_OCC, ALDataSample::SB_NONEECO}, // Daylight, setback should be limited.
{8,9,19,79},
{8,9,23,84},
{8,9,35,92},
{8,9,39,64},
{8,9,43,78},
{8,9,55,68},
{8,9,59,60},
{8,10,3,62, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::UNKNOWN_ACT_OCC, ALDataSample::SB_NONEECO}, // Daylight, setback should be limited.
{8,10,11,41},
{8,10,15,40},
{8,10,16,42},
{8,10,23,40},
{8,10,27,45},
{8,10,39,99},
{8,10,46,146},
{8,10,51,79},
{8,10,56,46},
{8,11,3,54, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Broad daylight, vacant, some setback should be in place.
{8,11,7,63},
{8,11,23,132},
{8,11,27,125},
{8,11,39,78}, // Cloud passing over.
{8,11,55,136},
{8,11,59,132},
{8,12,7,132, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Broad daylight, vacant, some setback should be in place.
{8,12,19,147},
{8,12,23,114, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Broad daylight, vacant, some setback should be in place.
{8,12,35,91}, // Cloud passing over.
{8,12,47,89},
{8,12,55,85},
{8,13,3,98, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Broad daylight, vacant, some setback should be in place.
{8,13,11,105},
{8,13,19,106},
{8,13,31,32},
{8,13,43,29},
{8,13,51,45},
{8,13,55,37},
{8,13,59,31},
{8,14,7,42, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Broad daylight, vacant, some setback should be in place.
{8,14,27,69},
{8,14,31,70},
{8,14,35,63},
{8,14,55,40},
{8,15,7,47, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Daylight, vacant, some setback should be in place.
{8,15,11,48},
{8,15,19,66},
{8,15,27,48},
{8,15,35,46},
{8,15,43,40},
{8,15,51,33},
{8,16,3,24, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Daylight, vacant, some setback should be in place.
{8,16,11,26},
{8,16,27,20},
{8,16,39,14},
{8,16,54,8},
{8,16,59,6},
{8,17,3,5, ALDataSample::NO_OCC_EXPECTATION, true, false, ALDataSample::SB_MINECO}, // Dark, vacant, some setback should be in place.
{8,17,19,3},
{8,17,31,2},
{8,17,47,2, occType::OCC_NONE, true, false}, // Light turned off, no active occupancy.
// ...
{8,20,11,2},
{8,20,23,2},
{8,20,35,16, occType::OCC_PROBABLE, false, true}, // Light turned on, OCCUPANCY.
{8,20,46,16, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{8,20,55,13, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{8,20,58,14, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{8,21,7,3, occType::OCC_NONE, true}, // Light turned off, no active occupancy.
{8,21,23,2, occType::OCC_NONE, true, false}, // Light turned off, no active occupancy.
{8,21,39,2, occType::OCC_NONE, true, false}, // Light turned off, no active occupancy.
{8,21,55,2},
// ...
{9,0,55,2, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{9,1,7,2, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{9,1,15,1, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{9,1,19,1, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
// ...
{9,5,31,1, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{9,5,36,1, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{9,5,47,2, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{9,5,51,2, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{9,6,3,3},
{9,6,15,5, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Not occupied actively, sleeping, max setback.
{9,6,27,10, ALDataSample::NO_OCC_EXPECTATION, ALDataSample::NO_RD_EXPECTATION, ALDataSample::UNKNOWN_ACT_OCC, ALDataSample::SB_NONEECO}, // Should be anticipating occupancy; at most small setback.
{9,6,31,12},
{9,6,35,15},
{9,6,39,19},
{9,6,43,26},
{9,6,59,24, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // Occupied but may be applying a limited setback.
{9,7,7,28, occType::OCC_NONE}, // Not yet up and about.  But not actually dark.
{9,7,15,66},
{9,7,27,181, occType::OCC_PROBABLE, false, true, ALDataSample::SB_NONEECO}, // Curtains drawn: temporary occupancy, some setback OK.
{9,7,43,181},
{9,7,51,181},
{9,7,59,181, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::UNKNOWN_ACT_OCC, ALDataSample::SB_NONEECO}, // Not dark, occupancy unknown, some setback OK.
    { }
    };
// Test with real data set.
TEST(AmbientLightOccupancyDetection,sample5sHard)
{
    simpleDataSampleRun(sample5sHard);
}


// "5s" 2016/12/01--04 test set with some fine-grained data in the second half.
// 2016/12/03 all of 3l, 5s, 6k, 7h: vacant from 11:00Z to 14:00Z but wrongly seen as occupied.
// 5s also probably occupied 16:00--16:30 and 18:14--19:16 and 19:29--21:07.
static const ALDataSample sample5sHard2[] =
    {
{1,0,1,1, occType::OCC_NONE, true, false},
{1,0,22,1, occType::OCC_NONE, true, false},
// ...
{1,6,29,1},
{1,6,47,1},
{1,7,5,2},
{1,7,17,1, occType::OCC_NONE, true, false},
{1,7,37,6},
{1,7,46,9},
{1,7,50,11},
{1,8,5,19},
{1,8,18,25},
{1,8,22,26},
{1,8,33,37},
{1,8,41,45},
{1,8,49,86},
{1,8,52,83},
{1,8,53,82},
{1,8,57,86},
{1,9,12,115},
{1,9,21,103},
{1,9,25,114},
{1,9,37,108},
{1,9,41,74},
{1,9,53,60},
{1,10,3,82},
{1,10,5,100},
{1,10,13,78},
{1,10,17,76},
{1,10,29,89},
{1,10,39,79},
{1,10,45,92},
{1,10,57,125},
{1,11,1,106},
{1,11,9,87},
{1,11,25,78},
{1,11,33,75},
{1,11,37,73},
{1,11,53,69},
{1,12,5,64},
{1,12,9,62},
{1,12,19,58},
{1,12,21,57},
{1,12,33,53},
{1,12,41,50},
{1,12,45,49},
{1,13,1,46},
{1,13,19,44},
{1,13,29,43},
{1,13,45,42},
{1,14,1,39},
{1,14,15,36},
{1,14,21,35},
{1,14,29,33},
{1,14,45,29},
{1,14,58,26},
{1,15,13,21},
{1,15,21,19},
{1,15,28,15},
{1,15,41,11},
{1,15,53,7},
{1,16,5,4},
{1,16,16,2},
{1,16,17,2},
{1,16,29,2},
{1,16,45,2},
{1,16,57,2},
{1,17,5,7},
{1,17,13,1},
{1,17,21,1},
{1,17,33,1},
{1,17,49,24},
{1,17,53,24},
{1,18,3,2},
{1,18,13,26},
{1,18,29,40},
{1,18,33,2},
{1,18,45,2},
{1,19,1,2},
{1,19,17,2},
{1,19,33,2},
{1,19,53,2},
{1,20,9,2},
{1,20,10,1},
{1,20,25,1},
{1,20,49,1},
{1,21,1,1},
{1,21,15,1},
{1,21,29,2},
{1,21,41,1},
{1,21,57,1},
{1,22,13,2},
{1,22,29,2},
{1,22,45,2},
{1,23,1,2},
{1,23,17,2},
{1,23,25,1},
{1,23,29,1, occType::OCC_NONE, true, false},
// ...
{2,6,49,1, occType::OCC_NONE, true, false},
{2,7,1,1},
{2,7,17,2},
{2,7,21,2},
{2,7,33,2},
{2,7,49,3},
{2,7,53,4},
{2,7,59,6},
{2,8,1,19},
{2,8,13,11},
{2,8,17,12},
{2,8,33,15},
{2,8,45,17},
{2,9,1,20},
{2,9,5,19},
{2,9,17,25},
{2,9,21,28},
{2,9,37,37},
{2,9,38,38},
{2,9,49,40},
{2,10,5,44},
{2,10,13,43},
{2,10,25,47},
{2,10,37,50},
{2,10,41,50},
{2,10,57,50},
{2,11,9,54},
{2,11,13,54},
{2,11,29,50},
{2,11,41,50},
{2,12,1,53},
{2,12,11,51},
{2,12,13,50},
{2,12,22,48},
{2,12,25,46},
{2,12,37,44},
{2,12,54,41},
{2,13,5,39},
{2,13,9,38},
{2,13,21,32},
{2,13,29,29},
{2,13,31,28},
{2,13,45,27},
{2,14,5,22},
{2,14,21,20},
{2,14,25,20},
{2,14,41,17},
{2,14,45,15},
{2,15,17,8},
{2,15,33,5},
{2,15,37,4},
{2,15,45,3},
{2,16,10,30, occType::OCC_PROBABLE, false, true}, // Light on, occupied.
{2,16,14,25},
{2,16,25,25},
{2,16,41,25},
{2,16,45,34, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{2,16,46,25},
{2,16,50,25},
{2,16,55,25},
{2,16,59,25},
{2,17,0,24, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{2,17,3,25},
{2,17,4,24},
{2,17,6,25},
{2,17,6,25},
{2,17,9,24},
{2,17,14,24},
{2,17,17,24},
{2,17,20,24},
{2,17,22,25},
{2,17,24,24},
{2,17,25,24},
{2,17,27,25},
{2,17,29,24},
{2,17,33,25, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{2,17,34,24},
{2,17,37,24},
{2,17,38,25},
{2,17,40,25},
{2,17,42,24},
{2,17,45,25},
{2,17,49,25},
{2,17,52,25},
{2,17,54,24},
{2,17,55,25},
{2,18,0,24, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{2,18,2,25},
{2,18,6,24},
{2,18,9,24},
{2,18,9,25},
{2,18,13,25},
{2,18,13,24},
{2,18,16,24},
{2,18,20,33},
{2,18,21,24},
{2,18,22,25},
{2,18,23,24},
{2,18,23,24},
{2,18,23,24},
{2,18,23,24},
{2,18,24,25},
{2,18,25,24},
{2,18,29,24},
{2,18,32,24, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{2,18,33,25},
{2,18,36,24},
{2,18,40,24},
{2,18,43,25},
{2,18,46,33},
{2,18,47,25, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{2,18,50,1, occType::OCC_NONE, true},
{2,18,51,1},
{2,18,55,1},
{2,18,58,1},
{2,19,1,1, occType::OCC_NONE, true},
{2,19,2,26, occType::OCC_PROBABLE, false, true}, // Light on, occupied.
{2,19,5,25},
{2,19,6,26},
{2,19,9,25},
{2,19,13,25},
{2,19,17,25},
{2,19,20,25},
{2,19,24,25},
{2,19,28,25},
{2,19,31,25},
{2,19,35,25, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{2,19,38,25},
{2,19,42,25},
{2,19,45,25},
{2,19,49,25},
{2,19,53,25},
{2,19,56,25},
{2,20,0,25, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{2,20,3,24},
{2,20,7,24},
{2,20,11,24},
{2,20,15,24},
{2,20,19,24},
{2,20,22,24},
{2,20,26,24},
{2,20,29,24},
{2,20,33,24}, // FIXME ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{2,20,37,24},
{2,20,40,24},
{2,20,42,2},
{2,20,44,2},
{2,20,48,2},
{2,20,51,2},
{2,20,55,2},
{2,20,59,2},
{2,21,2,2},
{2,21,6,26, occType::OCC_PROBABLE, false, true}, // Light on, occupied.
{2,21,9,25},
{2,21,13,25},
{2,21,17,25},
{2,21,21,25},
{2,21,24,24},
{2,21,25,24},
{2,21,29,24},
{2,21,33,24, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{2,21,37,24},
{2,21,41,24},
{2,21,45,24},
{2,21,49,24},
{2,21,52,24},
{2,21,56,24},
{2,21,59,24},
{2,22,3,24, ALDataSample::NO_OCC_EXPECTATION, false}, // Light, occupied.  // FIXME, unusual time.
{2,22,7,24},
{2,22,10,24},
{2,22,14,24},
{2,22,18,24},
{2,22,21,24},
{2,22,24,25},
{2,22,25,24},
{2,22,28,25},
{2,22,29,25},
{2,22,30,24, ALDataSample::NO_OCC_EXPECTATION, false}, // Light, occupied.  // FIXME, unusual time.
{2,22,33,25},
{2,22,34,24},
{2,22,36,25, ALDataSample::NO_OCC_EXPECTATION, false}, // Light, occupied.  // FIXME, unusual time.
{2,22,38,2, occType::OCC_NONE, true},
{2,22,41,2},
{2,22,45,2},
{2,22,49,2},
{2,22,53,2},
{2,22,57,2},
{2,23,1,2},
{2,23,3,1},
{2,23,5,1, occType::OCC_NONE, true},
// ...
{3,7,38,1, occType::OCC_NONE, true},
{3,7,42,1},
{3,7,46,2},
{3,7,50,2},
{3,7,54,2},
{3,7,56,3},
{3,7,58,3},
{3,8,2,3},
{3,8,4,4},
{3,8,6,4},
{3,8,10,4},
{3,8,14,10},
{3,8,18,11},
{3,8,20,12},
{3,8,22,12},
{3,8,25,12},
{3,8,30,12},
{3,8,33,15},
{3,8,37,17},
{3,8,41,21},
{3,8,45,22},
{3,8,50,21},
{3,8,51,21},
{3,8,52,22},
{3,8,55,22},
{3,8,59,24},
{3,9,1,26},
{3,9,3,28},
{3,9,5,33},
{3,9,7,34},
{3,9,8,36},
{3,9,9,38},
{3,9,12,41},
{3,9,13,43},
{3,9,14,47},
{3,9,17,47},
{3,9,18,46},
{3,9,22,63},
{3,9,23,67},
{3,9,24,70},
{3,9,27,78},
{3,9,28,75},
{3,9,32,80},
{3,9,33,149}, // Cloud passing?  Mean ~ 81.
{3,9,37,98},
{3,9,38,120},
{3,9,39,101},
{3,9,42,141},
{3,9,43,145},
{3,9,47,120},
{3,9,48,117},
{3,9,49,110},
{3,9,52,88},
{3,9,53,87},
{3,9,54,77},
{3,9,56,73},
{3,9,58,82},
{3,10,1,92},
{3,10,2,94},
{3,10,5,115},
{3,10,6,138},
{3,10,7,98},
{3,10,10,81},
{3,10,14,88},
{3,10,15,84},
{3,10,16,75},
{3,10,19,90},
{3,10,23,78},
{3,10,24,91},
{3,10,27,96},
{3,10,28,103},
{3,10,31,113},
{3,10,32,111},
{3,10,35,109},
{3,10,36,113},
{3,10,39,92},
{3,10,40,66},
{3,10,41,67},
{3,10,44,86},
{3,10,45,87},
{3,10,48,102},
{3,10,49,135},
{3,10,50,81},
{3,10,53,90},
{3,10,56,143}, // Cloud passing?  Mean ~ 98.
{3,10,58,154},
{3,11,1,149, ALDataSample::NO_OCC_EXPECTATION, false, false}, // Light, vacant.
{3,11,2,140},
{3,11,6,126},
{3,11,7,131},
{3,11,11,135},
{3,11,15,145},
{3,11,19,145},
{3,11,23,148},
{3,11,27,107},
{3,11,31,103, ALDataSample::NO_OCC_EXPECTATION, false, false}, // Light, vacant.
{3,11,35,154},
{3,11,40,132},
{3,11,41,130},
{3,11,45,131},
{3,11,46,126},
{3,11,50,88},
{3,11,51,90},
{3,11,52,99},
{3,11,55,70},
{3,11,56,78},
{3,11,57,77},
{3,12,0,82, ALDataSample::NO_OCC_EXPECTATION, false, false}, // Light, vacant.
{3,12,1,108},
{3,12,5,79},
{3,12,6,99},
{3,12,7,75},
{3,12,10,71},
{3,12,11,74},
{3,12,12,85},
{3,12,13,71},
{3,12,15,70},
{3,12,16,91},
{3,12,17,100},
{3,12,20,101},
{3,12,24,88},
{3,12,25,87},
{3,12,28,87},
{3,12,32,85, ALDataSample::NO_OCC_EXPECTATION, false, false}, // Light, vacant.
{3,12,33,77},
{3,12,34,76},
{3,12,37,77},
{3,12,39,75},
{3,12,41,67},
{3,12,45,67},
{3,12,46,65},
{3,12,50,64},
{3,12,51,64},
{3,12,55,59},
{3,12,56,58},
{3,12,57,57},
{3,13,0,56, ALDataSample::NO_OCC_EXPECTATION, false, false}, // Light, vacant.
{3,13,1,57},
{3,13,2,56},
{3,13,5,56},
{3,13,9,53},
{3,13,10,50},
{3,13,14,41},
{3,13,18,40},
{3,13,21,54},
{3,13,23,55},
{3,13,25,57},
{3,13,27,46},
{3,13,29,50},
{3,13,30,51, ALDataSample::NO_OCC_EXPECTATION, false, false}, // Light, vacant.
{3,13,31,60},
{3,13,32,61},
{3,13,34,61},
{3,13,35,58},
{3,13,36,48},
{3,13,39,41},
{3,13,40,48},
{3,13,42,47},
{3,13,44,43},
{3,13,47,47},
{3,13,49,46},
{3,13,53,45},
{3,13,55,43},
{3,13,59,43, ALDataSample::NO_OCC_EXPECTATION, false, false}, // Light, vacant.
{3,14,3,43},
{3,14,8,46},
{3,14,11,49},
{3,14,15,51},
{3,14,19,48},
{3,14,21,46},
{3,14,23,45},
{3,14,27,44},
{3,14,29,43},
{3,14,31,42},
{3,14,36,40},
{3,14,40,39},
{3,14,41,39},
{3,14,42,38},
{3,14,45,36},
{3,14,49,34},
{3,14,53,33},
{3,14,57,33},
{3,14,59,32},
{3,15,1,30},
{3,15,6,28},
{3,15,7,28},
{3,15,8,27},
{3,15,11,26},
{3,15,13,25},
{3,15,16,24},
{3,15,17,23},
{3,15,20,23},
{3,15,21,22},
{3,15,24,21},
{3,15,28,19},
{3,15,29,18},
{3,15,30,17},
{3,15,33,16},
{3,15,34,15},
{3,15,37,14},
{3,15,39,13},
{3,15,41,12},
{3,15,42,11},
{3,15,46,9},
{3,15,47,9},
{3,15,49,8},
{3,15,51,8},
{3,15,52,7},
{3,15,56,6},
{3,16,0,24},
{3,16,3,23},
{3,16,7,22},
{3,16,11,22},
{3,16,13,20},
{3,16,16,26},
{3,16,19,25},
{3,16,23,26},
{3,16,27,25},
{3,16,28,26},
{3,16,32,25},
{3,16,36,1},
{3,16,37,2},
{3,16,38,1},
{3,16,41,1},
{3,16,45,1},
{3,16,49,44},
{3,16,53,37},
{3,16,55,46},
{3,16,57,46},
{3,16,58,37},
{3,17,0,2},
{3,17,3,2},
{3,17,6,2},
{3,17,10,2},
{3,17,15,2},
{3,17,18,2},
{3,17,23,2},
{3,17,26,2},
{3,17,31,2},
{3,17,34,2},
{3,17,39,2},
{3,17,42,2},
{3,17,44,1},
{3,17,46,2},
{3,17,49,2},
{3,17,53,2},
{3,17,57,2},
{3,18,1,2},
{3,18,6,2},
{3,18,10,9, occType::OCC_PROBABLE, false, true}, // Light on, occupied.
{3,18,14,19, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{3,18,15,16},
{3,18,16,14},
{3,18,19,14},
{3,18,21,22},
{3,18,24,22},
{3,18,25,14},
{3,18,28,22},
{3,18,29,17},
{3,18,30,19, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{3,18,32,16},
{3,18,34,21},
{3,18,37,16},
{3,18,38,22},
{3,18,40,14},
{3,18,42,22},
{3,18,43,16},
{3,18,44,18},
{3,18,47,14},
{3,18,49,16},
{3,18,52,13},
{3,18,55,12},
{3,18,57,19},
{3,18,59,12},
{3,19,1,12},
{3,19,3,14},
{3,19,5,21},
{3,19,7,18},
{3,19,11,18},
{3,19,13,12},
{3,19,15,13},
{3,19,16,18, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{3,19,20,6},
{3,19,21,2, ALDataSample::NO_OCC_EXPECTATION, true}, // Dark, temporarily vacant.
{3,19,25,2},
{3,19,29,17, occType::OCC_PROBABLE, false, true}, // Light, occupied.
{3,19,33,22},
{3,19,37,13},
{3,19,41,19},
{3,19,43,22},
{3,19,46,22},
{3,19,50,21},
{3,19,51,22},
{3,19,52,21},
{3,19,55,22},
{3,19,57,18},
{3,20,0,20, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{3,20,1,21},
{3,20,2,14},
{3,20,5,22},
{3,20,6,21},
{3,20,7,22},
{3,20,10,16},
{3,20,11,17},
{3,20,15,13},
{3,20,16,16},
{3,20,17,21},
{3,20,20,22},
{3,20,21,19},
{3,20,22,13},
{3,20,25,22},
{3,20,27,14},
{3,20,29,15},
{3,20,31,13},
{3,20,33,21, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{3,20,35,12},
{3,20,38,16},
{3,20,39,17},
{3,20,40,15},
{3,20,43,22},
{3,20,45,18},
{3,20,48,18},
{3,20,49,16},
{3,20,50,13},
{3,20,53,13},
{3,20,55,18},
{3,20,58,20},
{3,20,59,16},
{3,21,2,13},
{3,21,3,20},
{3,21,4,13},
{3,21,7,21, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Light, occupied.
{3,21,8,7},
{3,21,9,2},
{3,21,12,2},
{3,21,16,2},
{3,21,20,2},
{3,21,24,2},
{3,21,28,2},
{3,21,32,2},
{3,21,36,2},
{3,21,40,2},
{3,21,44,2},
{3,21,48,2},
{3,21,52,2},
{3,21,57,2},
{3,22,0,2},
{3,22,4,2},
{3,22,8,14},
{3,22,10,2},
{3,22,12,2},
{3,22,15,2},
{3,22,19,2},
{3,22,23,2},
{3,22,27,2},
{3,22,31,2},
{3,22,35,2},
{3,22,39,3},
{3,22,43,2},
{3,22,47,2},
{3,22,51,1},
{3,22,52,2},
{3,22,55,2},
{3,22,57,1},
{3,22,58,2},
{3,23,0,1},
{3,23,3,1, occType::OCC_NONE, true, false}, // Dark, no active occpancy.
// ...
{4,7,4,1},
{4,7,7,1},
{4,7,11,2},
{4,7,15,2},
{4,7,21,2},
{4,7,25,2},
{4,7,29,2},
{4,7,33,3},
{4,7,37,4},
{4,7,41,5},
{4,7,45,6},
{4,7,49,7},
{4,7,50,6},
{4,7,51,7},
{4,7,54,8},
{4,7,58,9},
{4,7,58,10},
{4,8,2,11},
{4,8,6,13},
{4,8,7,13},
{4,8,9,14},
{4,8,11,14},
{4,8,13,15},
{4,8,16,16},
{4,8,19,24},
{4,8,21,24},
{4,8,25,27},
{4,8,27,30},
{4,8,28,31},
{4,8,30,35},
{4,8,33,38},
{4,8,35,46},
{4,8,37,43},
{4,8,39,38},
{4,8,41,43},
{4,8,45,47},
{4,8,46,63},
{4,8,49,105},
{4,8,51,91},
{4,8,51,96},
{4,8,54,94},
{4,8,58,119},
{4,9,3,133},
{4,9,5,119},
{4,9,7,125},
{4,9,9,142},
{4,9,9,135},
{4,9,12,104},
{4,9,15,111},
{4,9,16,92},
{4,9,16,86},
{4,9,20,132},
{4,9,21,140},
{4,9,24,101},
{4,9,28,175},
{4,9,31,175},
{4,9,34,134},
{4,9,34,114},
{4,9,35,133},
{4,9,37,141},
    { }
    };
// Test with real data set.
TEST(AmbientLightOccupancyDetection,sample5sHard2)
{
    simpleDataSampleRun(sample5sHard2);
}

// "2b" 2016/10/08+09 test set with tough occupancy to detect in the evening ~19:00Z to 20:00Z.
static const ALDataSample sample2bHard[] =
    {
{8,0,12,3},
{8,0,24,3, occType::OCC_NONE, true, false}, // Dark, vacant.
// ...
{8,7,28,3, occType::OCC_NONE, true, false}, // Dark, vacant.
{8,7,40,180, occType::OCC_PROBABLE, false, true}, // Curtains drawn, OCCUPANCY.
{8,7,44,179, ALDataSample::NO_OCC_EXPECTATION, false, true}, // Curtains drawn, OCCUPANCY.
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
{8,12,4,186, ALDataSample::NO_OCC_EXPECTATION, false, false}, // Broad daylight, vacant.
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
{8,18,12,3, occType::OCC_NONE, true, false},  // Dark, vacant.
{8,18,24,3},
{8,18,40,3},
{8,18,52,3},
{8,19,4,3},
{8,19,20,3},
{8,19,32,4},
{8,19,39,4},
{8,19,52,4, occType::OCC_NONE, true, false},  // Dark, vacant.
{8,20,0,7},
{8,20,16,6},
{8,20,20,10, occType::OCC_PROBABLE, ALDataSample::NO_RD_EXPECTATION, true}, // Light on, OCCUPANCY.  FIXME: should be light.
{8,20,28,6, ALDataSample::NO_OCC_EXPECTATION, ALDataSample::NO_RD_EXPECTATION, true}, // Occupied.
{8,20,36,3, occType::OCC_NONE, true},  // Dark, becoming vacant.
{8,20,42,3},
// ...
{9,7,40,3},
{9,7,48,3},
{9,7,52,4},
{9,8,8,176, occType::OCC_PROBABLE, false, true}, // Curtains drawn, OCCUPANCY.
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
{9,12,4,184, ALDataSample::NO_OCC_EXPECTATION, false}, // Broad daylight.
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
{9,17,32,7, occType::OCC_NONE, ALDataSample::NO_RD_EXPECTATION, false}, // No active occupancy.
{9,17,36,5},
{9,17,41,4},
{9,17,48,3},
{9,18,0,3},
{9,18,12,3, occType::OCC_NONE, true, false}, // Light off, no active occupancy.
{9,18,28,3},
{9,18,40,3},
{9,18,56,3},
{9,19,8,10, occType::OCC_PROBABLE, false, true}, // Light on, OCCUPANCY.  FIXME: should be light.
{9,19,16,9, ALDataSample::NO_OCC_EXPECTATION, ALDataSample::NO_RD_EXPECTATION, true}, // Occupied.
{9,19,28,10, ALDataSample::NO_OCC_EXPECTATION, ALDataSample::NO_RD_EXPECTATION, true}, // Occupied.
{9,19,44,6, ALDataSample::NO_OCC_EXPECTATION, ALDataSample::NO_RD_EXPECTATION, true}, // Occupied.
{9,19,48,11, occType::OCC_PROBABLE, false, true}, // Small light on?  Possible occupancy.  FIXME: should be light.
{9,19,56,8},
{9,20,4,8},
{9,20,8,3, occType::OCC_NONE, true}, // Light off.
{9,20,20,3, occType::OCC_NONE, true}, // Dark.
{9,20,36,3, occType::OCC_NONE, true, false}, // Dark, no active occupancy.
    { }
    };
// Test with real data set.
TEST(AmbientLightOccupancyDetection,sample2bHard)
{
    simpleDataSampleRun(sample2bHard);
}

// "2b" 2016/11/28+29 test set with tough occupancy to detect in the evening ~20:00Z to 21:00Z.
static const ALDataSample sample2bHard2[] =
    {
{28,0,8,8, occType::OCC_NONE, true, false}, // Sleeping, albeit with week night light.
{28,0,16,8, occType::OCC_NONE, true, false}, // Sleeping, albeit with week night light.
// ...
{28,7,21,8},
{28,7,33,8},
{28,7,40,35, ALDataSample::NO_OCC_EXPECTATION, false, true}, // FIXME: should be able to detect curtains drawn here (occType::OCC_PROBABLE).
{28,7,53,54, ALDataSample::NO_OCC_EXPECTATION, false}, // FIXME: should be able to detect curtains drawn here (occType::OCC_PROBABLE).
{28,8,0,69},
{28,8,12,85},
{28,8,16,90},
{28,8,24,103},
{28,8,37,115},
{28,8,41,120},
{28,8,53,133},
{28,8,54,134},
{28,9,0,140},
{28,9,9,148},
{28,9,13,152},
{28,9,25,164},
{28,9,29,167},
{28,9,40,173},
{28,9,44,174},
{28,9,56,176},
{28,10,4,176},
{28,10,10,177},
{28,10,17,177},
{28,10,23,178},
{28,10,24,178},
{28,10,45,179},
{28,10,50,179},
{28,11,0,179},
{28,11,17,179},
{28,11,28,179},
{28,11,37,180},
{28,11,41,180},
{28,11,57,180},
{28,12,4,180, ALDataSample::NO_OCC_EXPECTATION, false, false}, // Broad daylight, vacant.
{28,12,20,181},
{28,12,33,181},
{28,12,44,182},
{28,12,57,182},
{28,13,8,183},
{28,13,21,183},
{28,13,25,184},
{28,13,28,184},
{28,13,45,184},
{28,13,48,185},
{28,13,52,185},
{28,14,8,185},
{28,14,21,185},
{28,14,25,185},
{28,14,32,185},
{28,14,41,183},
{28,14,56,184},
{28,15,5,183},
{28,15,8,182},
{28,15,20,176},
{28,15,24,174},
{28,15,25,172},
{28,15,32,151},
{28,15,40,118},
{28,15,45,111},
{28,15,52,68},
{28,16,1,42},
{28,16,4,34},
{28,16,9,8},
{28,16,16,8},
// ....
{28,19,13,8},
{28,19,28,8},
{28,19,44,14, occType::OCC_PROBABLE, ALDataSample::NO_RD_EXPECTATION, true}, // Light on: OCCUPIED.  FIXME: should not be dark.
{28,19,48,13},
{28,20,1,16, ALDataSample::NO_OCC_EXPECTATION, ALDataSample::NO_RD_EXPECTATION, true}, // Light on: OCCUPIED.  FIXME: should not be dark.
{28,20,16,13},
{28,20,28,12},
{28,20,36,15, occType::OCC_NONE, ALDataSample::NO_RD_EXPECTATION}, // Light on: OCCUPIED.  FIXME: should not be dark nor vacant.
{28,20,40,8},
{28,20,48,8},
// ...
{29,7,20,8},
{29,7,32,8},
{29,7,48,34, ALDataSample::NO_OCC_EXPECTATION, false, true}, // FIXME: Should be able to detect curtains drawn here.
{29,8,1,30},
{29,8,12,77},
{29,8,16,82},
{29,8,36,107},
{29,8,44,118},
{29,8,48,122},
{29,9,0,134},
{29,9,8,142},
{29,9,20,153},
{29,9,24,158},
{29,9,40,171},
{29,9,52,175},
{29,10,4,176},
{29,10,20,177},
{29,10,36,178},
{29,10,52,179},
{29,11,0,179},
{29,11,12,179},
{29,11,28,179},
{29,11,48,180},
{29,12,0,180},
{29,12,8,180},
{29,12,24,180},
{29,12,36,181},
{29,12,40,181},
{29,12,52,182},
{29,12,56,182},
{29,13,8,183},
{29,13,24,183},
{29,13,36,184},
{29,13,44,184},
{29,13,48,185},
{29,13,56,185},
{29,14,8,185},
{29,14,24,185},
{29,14,32,184},
{29,14,44,181},
{29,14,48,183},
{29,14,52,184},
{29,15,4,183},
{29,15,8,181},
{29,15,12,174},
{29,15,24,130},
{29,15,28,121},
{29,15,40,89},
{29,15,44,78},
{29,15,48,67},
{29,16,0,38},
{29,16,8,24},
{29,16,12,20},
{29,16,20,13},
{29,16,29,10},
{29,16,32,9},
{29,16,36,9},
{29,16,48,8},
{29,16,52,8},
// ...
{29,19,28,8},
{29,19,40,8},
{29,19,56,16, occType::OCC_PROBABLE, ALDataSample::NO_RD_EXPECTATION, true}, // Light on: OCCUPIED.  FIXME: should not be dark.
{29,20,4,12},
{29,20,8,11},
{29,20,16,10},
{29,20,32,8},
{29,20,44,8},
// ...
{29,23,44,8},
{29,23,56,8, occType::OCC_NONE, true, false}, // Light off, dark, no active occupation.
     { }
    };
// Test with real data set.
TEST(AmbientLightOccupancyDetection,sample2bHard2)
{
    simpleDataSampleRun(sample2bHard2);
}

// "6k" 2016/10/08+09 test set relatively easy to detect daytime occupancy in busy room.
static const ALDataSample sample6k[] =
    {
{8,0,7,1, occType::OCC_NONE, true, false}, // Not occupied.
{8,0,19,1},
{8,0,35,1},
{8,0,47,1},
{8,1,3,1, occType::OCC_NONE, true, false, ALDataSample::SB_ECOMAX}, // Dark, vacant, signficant setback.
{8,1,19,2},
{8,1,35,2},
{8,1,39,2},
// ...
{8,4,3,2, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Dark, vacant, max setback.
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
{8,7,31,121, occType::OCC_PROBABLE, false, true, ALDataSample::SB_NONE}, // Light on: OCCUPIED, no setback.
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
{8,12,7,155, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_NONEECO}, // Broad daylight, limited setback possible.
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
{8,14,19,53, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::UNKNOWN_ACT_OCC, ALDataSample::SB_NONEECO}, // occType::OCC_WEAK}, // Light still on?  Occupied? Possible small setback.
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
{8,16,35,164, occType::OCC_PROBABLE, false, true, ALDataSample::SB_NONE}, // Light on: OCCUPIED.  No setback.
{8,16,39,151},
{8,16,51,153},
{8,17,3,151},
{8,17,11,122},
{8,17,15,131},
{8,17,31,138},
{8,17,35,1, occType::OCC_NONE, true}, // Light off: (just) not occupied.
{8,17,43,1},
{8,17,55,1},
{8,18,3,1},
{8,18,15,1},
{8,18,23,1},
{8,18,35,1, occType::OCC_NONE, true, false, ALDataSample::SB_NONEECO}, // Light off: not occupied, small setback possible.
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
{8,21,27,90, occType::OCC_PROBABLE, false, true, ALDataSample::SB_NONE}, // Light on: OCCUPIED.  No setback.
{8,21,43,82},
{8,21,47,80},
{8,21,51,79},
{8,22,7,1, occType::OCC_NONE, true, false, ALDataSample::SB_NONEECO}, // Light off: not occupied.  Small setback possible.
{8,22,19,1},
// ...
{9,5,15,1, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Dark, vacant, max setback.
// ...
{9,5,59,1},
{9,6,7,2},
{9,6,11,2},
{9,6,15,3},
{9,6,23,4},
{9,6,31,6},
{9,6,35,8},
{9,6,47,50, occType::OCC_PROBABLE, false, true, ALDataSample::SB_NONE}, // Light on or blinds open: OCCUPIED. No setback.
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
    simpleDataSampleRun(sample6k);
}

//-----------------------------------------------------------------------------------------------------------------
// "3l" fortnight to 2016/11/24 looking for habitual evening artificial lighting to watch TV, etc.
// This is not especially intended to check response to other events, though will verify some key ones.
// See http://www.earth.org.uk/img/20161124-16WWal.png
static const ALDataSample sample3leveningTV[] =
    {
{10,0,7,1, occType::OCC_NONE, true, false}, // Definitely not occupied.
// ...
{10,6,31,1, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Dark, vacant, running long enough for max setback.
{10,6,47,1},
{10,6,59,2},
{10,7,3,2},
{10,7,23,9, ALDataSample::NO_OCC_EXPECTATION, ALDataSample::NO_RD_EXPECTATION, true, ALDataSample::SB_NONEECO}, // Curtains drawn, temporarily occupied, small setback still possible.  FIXME: should not be classified as dark.
{10,7,31,12},
{10,7,39,17},
{10,7,47,23},
{10,7,59,27},
{10,8,3,29, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // Light, may be occupied, should only have at most ECO setback because light.
{10,8,19,45},
{10,8,31,61},
{10,8,47,61},
{10,8,59,94},
{10,9,15,78, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::NO_RD_EXPECTATION, ALDataSample::SB_NONEECO}, // Light, probably not occupied, should only have at most ECO setback because light.
{10,9,19,76},
{10,9,27,74},
{10,9,39,73},
{10,9,43,76},
{10,9,55,83},
{10,10,11,116, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::NO_RD_EXPECTATION, ALDataSample::SB_NONEECO}, // Light, probably not occupied, should only have at most ECO setback because light.
{10,10,23,143},
{10,10,27,138},
{10,10,39,154},
{10,10,51,155},
{10,10,59,173},
{10,11,11,173, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::NO_RD_EXPECTATION, ALDataSample::SB_NONEECO}, // Light, probably not occupied, should only have at most ECO setback because light.
{10,11,15,177},
{10,11,23,176},
{10,11,39,164},
{10,11,51,152},
{10,11,55,159},
{10,11,59,156},
{10,12,3,171, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_NONEECO}, // Broad daylight, vacant, should only have at most ECO setback because light.
{10,12,11,181},
{10,12,15,180},
{10,12,23,125},
{10,12,27,102},
{10,12,31,112},
{10,12,39,111},
{10,12,47,118},
{10,12,51,125},
{10,13,3,164, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::NO_RD_EXPECTATION, ALDataSample::SB_NONEECO}, // Light, probably not occupied, should only have at most ECO setback because light.
{10,13,11,110},
{10,13,15,96},
{10,13,17,95},
{10,13,19,96},
{10,13,23,96},
{10,13,27,91},
{10,13,35,85},
{10,13,43,57},
{10,13,51,67},
{10,13,55,100},
{10,14,3,140, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::NO_RD_EXPECTATION, ALDataSample::SB_NONEECO}, // Light, probably not occupied, should only have at most ECO setback because light.
{10,14,7,137},
{10,14,11,129},
{10,14,19,178},
{10,14,23,170},
{10,14,27,149},
{10,14,35,178},
{10,14,39,182},
{10,14,43,178},
{10,14,55,153},
{10,14,59,142},
{10,15,3,163, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::NO_RD_EXPECTATION, ALDataSample::SB_NONEECO}, // Light, probably not occupied, should only have at most ECO setback because light.
{10,15,7,177},
{10,15,15,178},
{10,15,23,152},
{10,15,27,176},
{10,15,31,131},
{10,15,39,83},
{10,15,43,56},
{10,15,51,41},
{10,15,59,44, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::UNKNOWN_ACT_OCC, ALDataSample::SB_NONEECO}, // TV watching, occupied, no setback.
{10,16,3,39},
{10,16,15,19},
{10,16,23,44, occType::OCC_PROBABLE, false, true, ALDataSample::SB_NONE}, // TV watching, occupied, no setback.
{10,16,35,36},
{10,16,47,33},
{10,16,51,35, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONE}, // FIXME: occType::OCC_WEAK}, // TV watching, occupied, no setback.
{10,17,3,34},
{10,17,7,35},
{10,17,19,36},
{10,17,23,35},
{10,17,39,35, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONE}, // TV watching, occupied, no setback.
{10,17,51,34},
{10,17,59,30},
{10,18,3,31, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONE}, // TV watching, occupied, no setback.
{10,18,15,31},
{10,18,27,31},
{10,18,31,30},
{10,18,39,30, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // TV watching, borderline occupied, dark, maybe small setback.
{10,18,51,30},
{10,19,7,31},
{10,19,15,40},
{10,19,27,40, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // TV watching, borderline occupied, borderline dark, maybe small setback.
{10,19,43,39},
{10,19,55,41, occType::OCC_WEAK, false, true, ALDataSample::SB_NONEECO}, // TV watching, borderline occupied, borderline dark, maybe small setback.
{10,19,59,42},
{10,20,11,39},
{10,20,23,41, occType::OCC_WEAK, false, true, ALDataSample::SB_NONEECO}, // TV watching, borderline occupied, borderline dark, maybe small setback.
{10,20,31,39},
{10,20,43,40, occType::OCC_WEAK, false, true, ALDataSample::SB_NONEECO}, // TV watching, borderline occupied, borderline dark, maybe small setback.
{10,20,47,39},
{10,20,51,40, occType::OCC_WEAK, false, true, ALDataSample::SB_NONEECO}, // TV watching, borderline occupied, borderline dark, maybe small setback.
{10,21,7,40},
{10,21,9,41},
{10,21,15,41},
{10,21,35,40},
{10,21,47,40},
{10,21,55,39, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // TV watching, borderline occupied, borderline dark, maybe small setback.
{10,22,7,1},
{10,22,15,1, occType::OCC_NONE, true, false, ALDataSample::SB_ECOMAX}, // Vacant, dark.
// ...
{11,6,27,1, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Vacant, dark, dark long enough for full setback.
{11,6,43,1},
{11,6,55,2},
{11,7,7,5, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Vacant, dark, dark long enough for full setback.
{11,7,19,11},
{11,7,23,13},
{11,7,31,19},
{11,7,35,21},
{11,7,43,25},
{11,7,55,32},
{11,8,7,41},
{11,8,23,55},
{11,8,35,65},
{11,8,43,70},
{11,8,47,72},
{11,9,3,92},
{11,9,11,103},
{11,9,15,115},
{11,9,27,119},
{11,9,39,137},
{11,9,43,152},
{11,9,51,154},
{11,9,55,147},
{11,10,7,144},
{11,10,15,157},
{11,10,19,162},
{11,10,31,168},
{11,10,35,172},
{11,10,47,167},
{11,10,59,171},
{11,11,3,166},
{11,11,15,176},
{11,11,23,175},
{11,11,31,176},
{11,11,42,177},
{11,11,47,177},
{11,12,3,177},
{11,12,15,178},
{11,12,19,178},
{11,12,35,178},
{11,12,47,178},
{11,12,59,179},
{11,13,11,180},
{11,13,15,180},
{11,13,23,180},
{11,13,39,182},
{11,13,47,182},
{11,14,3,182},
{11,14,15,182},
{11,14,23,182},
{11,14,27,182},
{11,14,39,182},
{11,14,47,177},
{11,14,55,174},
{11,15,7,150},
{11,15,11,135},
{11,15,23,69},
{11,15,35,49},
{11,15,39,45},
{11,15,49,43},
{11,15,55,38},
{11,15,59,34},
{11,16,7,19},
{11,16,11,14},
{11,16,23,1},
{11,16,39,1},
{11,16,47,13},
{11,16,55,1},
{11,17,3,1},
{11,17,15,1},
{11,17,31,1},
{11,17,47,10},
{11,18,3,9},
{11,18,15,10},
{11,18,19,10},
{11,18,35,9},
{11,18,47,31},
{11,18,55,29},
{11,18,59,29},
{11,19,15,29},
{11,19,27,24},
{11,19,39,24},
{11,19,51,25},
{11,20,3,25},
{11,20,19,25},
{11,20,20,24},
{11,20,27,25},
{11,20,35,38},
{11,20,39,40},
{11,20,53,40},
{11,21,7,41},
{11,21,11,40},
{11,21,19,41},
{11,21,35,39},
{11,21,47,41},
{11,21,51,39},
{11,21,55,40},
{11,22,7,1},
{11,22,11,1},
// ...
{12,7,7,1},
{12,7,19,1},
{12,7,35,5},
{12,7,38,6},
{12,7,39,6},
{12,7,51,7},
{12,7,59,11},
{12,8,15,11},
{12,8,31,52},
{12,8,35,56},
{12,8,47,54},
{12,8,59,56},
{12,9,7,54},
{12,9,15,54},
{12,9,27,14},
{12,9,31,16},
{12,9,35,20},
{12,9,43,32},
{12,9,51,37},
{12,10,3,68},
{12,10,15,63},
{12,10,19,54},
{12,10,35,62},
{12,10,51,64},
{12,10,55,53},
{12,11,7,64},
{12,11,11,65},
{12,11,23,83},
{12,11,35,83},
{12,11,39,82},
{12,11,55,92},
{12,11,59,94},
{12,12,7,75},
{12,12,19,71},
{12,12,23,79},
{12,12,31,72},
{12,12,39,68},
{12,12,47,60},
{12,12,51,60},
{12,13,5,69},
{12,13,7,68},
{12,13,11,69},
{12,13,31,69},
{12,13,43,70},
{12,13,47,74},
{12,13,51,66},
{12,14,3,57},
{12,14,23,28},
{12,14,35,30},
{12,14,47,27},
{12,14,55,29},
{12,14,59,29},
{12,15,15,18},
{12,15,19,15},
{12,15,31,11}, // KEY/SENSITIVE DATA POINT FOLLOWS...
{12,15,35,46, occType::OCC_PROBABLE, false, true, ALDataSample::SB_NONE}, // Light on?  Occupied, no setback.
{12,15,47,49},
{12,15,51,47},
{12,15,59,43},
{12,16,10,41},
{12,16,11,43},
{12,16,23,41},
{12,16,27,43},
{12,16,35,41, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // TV watching, small or no setback.
{12,16,47,42},
{12,16,51,43},
{12,17,0,43},
{12,17,11,42, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // TV watching, small or no setback.
{12,17,23,1},
{12,17,39,13},
{12,17,40,14},
{12,17,47,13},
{12,17,59,14},
{12,18,11,44, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // TV watching, small or no setback.
{12,18,19,43},
{12,18,23,45},
{12,18,39,44},
{12,18,51,41},
{12,18,55,41},
{12,19,11,37, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // TV watching, small or no setback.
{12,19,15,35},
{12,19,19,35},
{12,19,35,34},
{12,19,47,35},
{12,19,59,42, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // TV watching, small or no setback.
{12,20,15,42},
{12,20,26,44},
{12,20,27,43},
{12,20,31,42},
{12,20,43,43},
{12,20,59,43},
{12,21,7,43, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // TV watching, small or no setback.
{12,21,11,45},
{12,21,21,43},
{12,21,23,44},
{12,21,39,42, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // TV watching, small or no setback.
{12,21,40,44},
{12,21,51,42},
{12,21,55,44},
{12,22,3,43},
{12,22,19,43},
{12,22,31,43},
{12,22,35,44, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEECO}, // TV watching, small or no setback.
{12,22,51,14},
{12,22,59,14},
{12,23,3,14},
{12,23,19,13},
{12,23,31,13},
{12,23,43,14},
{12,23,51,14},
{12,23,59,13},
{13,0,4,14},
{13,0,11,14},
{13,0,15,13},
{13,0,31,14},
{13,0,35,13},
{13,0,47,14},
{13,0,51,1, occType::OCC_NONE, true, false}, // Dark, vacant.
{13,1,3,1},
{13,1,19,1, occType::OCC_NONE, true, false, ALDataSample::SB_MINECO}, // Dark, vacant, some setback should be in place.
// ...
{13,4,11,1, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Dark and vacant long enough for max setback.
// ...
{13,5,7,1, occType::OCC_NONE, true, false, ALDataSample::SB_MAX}, // Dark and vacant long enough for max setback.
// ...
{13,7,23,1},
{13,7,35,1},
{13,7,51,52, occType::OCC_PROBABLE, false, true, ALDataSample::SB_NONEMIN}, // Dark, vacant, some setback possible.
{13,8,7,71},
{13,8,19,73},
{13,8,27,85},
{13,8,35,93},
{13,8,39,97},
{13,8,43,103},
{13,8,51,101},
{13,8,55,103},
{13,9,11,103},
{13,9,15,105},
{13,9,30,81},
{13,9,43,127},
{13,9,51,136},
{13,9,59,145},
{13,10,7,163},
{13,10,11,168},
{13,10,27,172},
{13,10,31,176},
{13,10,47,126},
{13,11,3,177},
{13,11,10,178},
{13,11,19,176},
{13,11,31,140},
{13,11,35,179},
{13,11,51,177},
{13,11,55,176},
{13,12,3,185},
{13,12,4,185},
{13,12,8,177},
{13,12,12,179},
{13,12,29,179},
{13,12,41,179},
{13,12,48,172},
{13,12,53,178},
{13,13,5,180},
{13,13,8,181},
{13,13,13,181},
{13,13,25,102},
{13,13,33,145},
{13,13,41,167},
{13,13,53,48},
{13,13,56,52},
{13,14,9,19},
{13,14,16,14},
{13,14,18,14},
{13,14,33,5},
{13,14,53,178},
{13,15,8,130},
{13,15,20,17},
{13,15,33,62},
{13,15,36,59},
{13,15,52,40},
{13,16,5,37},
{13,16,9,25},
{13,16,24,52},
{13,16,29,50},
{13,16,40,44},
{13,16,52,43},
{13,16,57,44},
{13,17,4,44},
{13,17,16,44},
{13,17,29,45},
{13,17,37,44},
{13,17,41,43},
{13,17,52,45},
{13,18,0,46},
{13,18,17,45},
{13,18,20,46},
{13,18,25,46},
{13,18,32,45},
{13,18,37,44},
{13,18,48,43},
{13,18,56,45},
{13,19,1,45},
{13,19,17,45},
{13,19,28,44},
{13,19,37,44},
{13,19,45,39},
{13,19,49,46},
{13,20,1,44},
{13,20,16,44},
{13,20,24,46},
{13,20,37,46},
{13,20,41,45},
{13,20,45,45},
{13,20,57,44},
{13,21,9,44},
{13,21,12,45},
{13,21,32,46},
{13,21,49,3},
{13,22,1,3},
// ...
{14,6,52,3},
{14,7,8,3},
{14,7,16,5},
{14,7,20,5},
{14,7,37,11},
{14,7,40,13},
{14,7,48,22},
{14,7,56,32},
{14,8,4,30},
{14,8,8,32},
{14,8,20,47},
{14,8,24,51},
{14,8,28,52},
{14,8,36,43},
{14,8,44,58},
{14,8,52,60},
{14,8,56,57},
{14,9,8,62},
{14,9,17,63},
{14,9,21,62},
{14,9,32,96},
{14,9,36,117},
{14,9,40,132},
{14,9,44,137},
{14,10,0,116},
{14,10,9,114},
{14,10,20,120},
{14,10,32,120},
{14,10,36,101},
{14,10,57,131},
{14,11,12,120},
{14,11,29,85},
{14,11,40,87},
{14,11,44,84},
{14,11,52,151},
{14,12,4,139},
{14,12,8,169},
{14,12,17,135},
{14,12,24,153},
{14,12,32,156},
{14,12,44,134},
{14,12,49,114},
{14,13,0,137},
{14,13,16,112},
{14,13,32,94},
{14,13,48,84},
{14,13,52,65},
{14,14,0,81},
{14,14,13,80},
{14,14,26,71},
{14,14,32,52},
{14,14,44,46},
{14,14,52,41},
{14,15,0,42},
{14,15,4,51},
{14,15,12,39},
{14,15,20,40},
{14,15,25,28},
{14,15,36,18},
{14,15,44,16},
{14,15,48,15},
{14,16,0,19},
{14,16,12,17},
{14,16,16,16},
{14,16,32,3},
{14,16,40,3},
{14,16,52,16},
{14,16,56,15},
{14,17,4,3},
{14,17,16,3},
{14,17,24,3},
{14,17,36,3},
{14,17,48,3},
{14,18,4,3},
{14,18,20,3},
{14,18,32,3},
{14,18,44,3},
{14,19,0,3},
{14,19,20,48},
{14,19,28,46},
{14,19,32,45},
{14,19,44,45},
{14,19,52,46},
{14,19,56,46},
{14,20,4,46},
{14,20,12,46},
{14,20,24,46},
{14,20,28,44},
{14,20,32,45},
{14,20,36,3},
{14,20,48,3},
{14,20,56,3},
{14,21,12,47},
{14,21,16,49},
{14,21,20,47},
{14,21,24,46},
{14,21,32,46},
{14,21,36,45},
{14,21,40,46},
{14,21,52,43},
{14,22,0,16},
{14,22,4,3},
{14,22,20,3},
// ...
{15,6,48,3},
{15,7,0,3},
{15,7,12,4},
{15,7,22,5},
{15,7,28,6},
{15,7,36,11},
{15,7,52,19},
{15,8,4,34},
{15,8,8,33},
{15,8,16,33},
{15,8,28,48},
{15,8,32,55},
{15,8,48,76},
{15,9,0,63},
{15,9,4,108},
{15,9,16,92},
{15,9,20,112},
{15,9,24,102},
{15,9,28,72},
{15,9,32,73},
{15,9,48,125},
{15,9,56,52},
{15,10,0,63},
{15,10,4,100},
{15,10,12,134},
{15,10,24,102},
{15,10,28,115},
{15,10,36,112},
{15,10,40,144},
{15,10,52,180},
{15,10,56,175},
{15,11,8,159},
{15,11,12,142},
{15,11,24,137},
{15,11,32,144},
{15,11,36,130},
{15,11,44,103},
{15,11,56,177},
{15,12,0,154},
{15,12,16,145},
{15,12,32,178},
{15,12,40,176},
{15,12,44,173},
{15,12,56,114},
{15,13,0,105},
{15,13,4,92},
{15,13,12,87},
{15,13,20,86},
{15,13,24,123},
{15,13,36,166},
{15,13,44,98},
{15,13,48,96},
{15,13,56,72},
{15,14,4,149},
{15,14,12,62},
{15,14,16,76},
{15,14,28,178},
{15,14,36,60},
{15,14,40,50},
{15,14,44,41},
{15,14,52,21},
{15,15,0,20},
{15,15,4,21},
{15,15,8,27},
{15,15,16,15},
{15,15,24,16},
{15,15,28,17},
{15,15,40,13},
{15,15,45,46},
{15,15,48,50},
{15,16,0,45},
{15,16,6,44},
{15,16,8,45},
{15,16,16,69},
{15,16,17,27},
{15,16,20,15},
{15,16,20,15},
{15,16,32,48},
{15,16,43,48},
{15,16,48,49},
{15,16,52,48},
{15,17,4,47},
{15,17,12,47},
{15,17,16,46},
{15,17,24,48},
{15,17,36,46},
{15,17,40,48},
{15,17,44,47},
{15,18,0,48},
{15,18,4,46},
{15,18,16,48},
{15,18,20,47},
{15,18,28,43},
{15,18,44,44},
{15,18,56,46},
{15,19,8,45},
{15,19,12,44},
{15,19,20,43},
{15,19,28,46},
{15,19,44,46},
{15,19,56,44},
{15,20,8,45},
{15,20,16,47},
{15,20,20,45},
{15,20,28,46},
{15,20,44,3},
{15,20,56,3},
// ...
{16,6,48,3},
{16,7,0,3},
{16,7,12,5},
{16,7,16,6},
{16,7,24,9},
{16,7,40,15},
{16,7,48,14},
{16,7,52,13},
{16,7,56,20},
{16,8,8,37},
{16,8,12,38},
{16,8,20,44},
{16,8,32,53},
{16,8,36,55},
{16,8,48,58},
{16,9,0,90},
{16,9,4,105},
{16,9,8,122},
{16,9,12,136},
{16,9,16,143},
{16,9,32,107},
{16,9,40,96},
{16,9,44,133},
{16,9,52,145},
{16,10,0,160},
{16,10,4,174},
{16,10,8,177},
{16,10,17,149},
{16,10,20,170},
{16,10,24,142},
{16,10,44,140},
{16,10,52,171},
{16,10,56,166},
{16,11,0,178},
{16,11,8,180},
{16,11,14,177},
{16,11,16,179},
{16,11,20,178},
{16,11,36,177},
{16,11,52,180},
{16,12,0,178},
{16,12,12,177},
{16,12,16,178},
{16,12,20,178},
{16,12,24,176},
{16,12,36,177},
{16,12,48,178},
{16,13,0,155},
{16,13,4,159},
{16,13,8,151},
{16,13,16,103},
{16,13,24,148},
{16,13,27,176},
{16,13,28,177},
{16,13,40,183},
{16,13,52,178},
{16,14,4,181},
{16,14,16,124},
{16,14,20,73},
{16,14,23,86},
{16,14,24,100},
{16,14,32,176},
{16,14,40,178},
{16,14,48,179},
{16,15,0,155},
{16,15,4,135},
{16,15,12,117},
{16,15,16,102},
{16,15,20,90},
{16,15,28,75},
{16,15,32,68},
{16,15,44,33},
{16,15,49,28},
{16,15,52,21},
{16,15,56,16},
{16,16,8,48},
{16,16,12,45},
{16,16,16,47},
{16,16,28,45},
{16,16,36,43},
{16,16,44,43},
{16,16,48,45},
{16,17,0,43},
{16,17,4,45},
{16,17,20,43},
{16,17,24,45},
{16,17,36,43},
{16,17,40,45},
{16,17,48,45},
{16,18,0,45},
{16,18,4,43},
{16,18,12,44},
{16,18,24,45},
{16,18,36,43},
{16,18,48,43},
{16,18,52,42},
{16,18,56,41},
{16,19,8,44},
{16,19,16,44},
{16,19,24,43},
{16,19,28,44},
{16,19,40,43},
{16,19,44,41},
{16,19,48,42},
{16,20,0,42},
{16,20,4,43},
{16,20,12,43},
{16,20,20,42},
{16,20,24,43},
{16,20,36,43},
{16,20,40,43},
{16,20,52,44},
{16,21,8,43},
{16,21,20,44},
{16,21,28,43},
{16,21,32,44},
{16,21,36,43},
{16,21,44,44},
{16,21,48,44},
{16,22,4,43},
{16,22,8,42},
{16,22,16,44},
{16,22,24,3},
{16,22,40,3},
// ...
{17,6,56,3},
{17,7,8,3},
{17,7,20,5},
{17,7,24,7},
{17,7,25,8},
{17,7,32,14},
{17,7,48,24},
{17,7,56,22},
{17,8,0,21},
{17,8,8,30},
{17,8,20,47},
{17,8,24,46},
{17,8,32,53},
{17,8,48,56},
{17,8,52,64},
{17,9,0,57},
{17,9,12,55},
{17,9,24,54},
{17,9,36,49},
{17,9,40,54},
{17,9,52,58},
{17,9,56,62},
{17,10,4,83},
{17,10,12,137},
{17,10,20,145},
{17,10,24,147},
{17,10,40,87},
{17,10,44,171},
{17,10,52,175},
{17,10,56,158},
{17,11,0,153},
{17,11,16,170},
{17,11,24,166},
{17,11,36,51},
{17,11,44,56},
{17,11,49,103},
{17,11,52,93},
{17,12,8,179},
{17,12,20,173},
{17,12,28,123},
{17,12,40,86},
{17,12,44,106},
{17,12,56,182},
{17,13,0,177},
{17,13,8,170},
{17,13,12,169},
{17,13,16,182},
{17,13,28,176},
{17,13,32,181},
{17,13,44,180},
{17,13,56,180},
{17,14,4,148},
{17,14,8,101},
{17,14,20,119},
{17,14,24,82},
{17,14,40,122},
{17,14,52,101},
{17,15,4,108},
{17,15,12,110},
{17,15,16,108},
{17,15,28,93},
{17,15,36,51},
{17,15,40,40},
{17,15,56,23},
{17,16,0,21},
{17,16,3,19},
{17,16,12,16},
{17,16,16,15},
{17,16,20,15},
{17,16,40,14},
{17,16,48,14},
{17,16,52,15},
{17,16,56,3},
{17,17,0,3},
{17,17,16,3},
{17,17,24,3},
{17,17,36,3},
{17,17,48,3},
{17,17,56,3},
{17,18,4,3},
{17,18,12,3},
{17,18,32,3},
{17,18,44,3},
{17,18,56,37},
{17,19,4,46},
{17,19,16,44},
{17,19,28,44},
{17,19,40,43},
{17,19,52,44},
{17,20,0,44},
{17,20,8,43},
{17,20,16,43},
{17,20,28,43},
{17,20,36,45},
{17,20,44,45},
{17,20,56,44},
{17,21,8,45},
{17,21,12,43},
{17,21,20,43},
{17,21,36,45},
{17,21,52,43},
{17,22,8,45},
{17,22,20,3},
{17,22,32,3},
// ...
{18,6,40,3},
{18,6,56,3},
{18,7,8,4},
{18,7,13,5},
{18,7,16,6},
{18,7,32,13},
{18,7,36,15},
{18,7,44,20},
{18,7,56,29},
{18,7,58,32},
{18,8,4,38},
{18,8,20,55},
{18,8,36,77},
{18,8,44,87},
{18,8,52,102},
{18,9,0,126},
{18,9,4,137},
{18,9,20,173},
{18,9,24,175},
{18,9,36,176},
{18,9,44,163},
{18,9,48,152},
{18,10,4,148},
{18,10,20,173},
{18,10,32,160},
{18,10,40,152},
{18,10,51,128},
{18,10,52,127},
{18,11,8,123},
{18,11,24,121},
{18,11,36,132},
{18,11,40,142},
{18,11,50,175},
{18,12,4,176},
{18,12,19,177},
{18,12,24,180},
{18,12,28,178},
{18,12,36,180},
{18,12,48,175},
{18,12,52,174},
{18,13,8,178},
{18,13,20,164},
{18,13,32,180},
{18,13,36,182},
{18,13,48,182},
{18,13,52,183},
{18,14,4,182},
{18,14,24,180},
{18,14,40,176},
{18,14,52,178},
{18,15,4,171},
{18,15,8,132},
{18,15,24,94},
{18,15,32,58},
{18,15,36,71},
{18,15,48,48},
{18,16,0,16},
{18,16,4,12},
{18,16,16,48},
{18,16,32,45},
{18,16,48,55},
{18,16,52,45},
{18,17,0,44},
{18,17,4,45},
{18,17,8,45},
{18,17,19,3},
{18,17,28,15},
{18,17,40,44},
{18,17,45,46},
{18,17,48,46},
{18,18,4,43},
{18,18,16,45},
{18,18,32,43},
{18,18,48,45},
{18,19,4,46},
{18,19,12,43},
{18,19,24,46},
{18,19,36,46},
{18,19,48,46},
{18,19,52,46},
{18,20,8,45},
{18,20,19,45},
{18,20,24,44},
{18,20,28,44},
{18,20,44,46},
{18,20,48,43},
{18,20,52,44},
{18,21,8,44},
{18,21,16,45},
{18,21,28,45},
{18,21,44,45},
{18,21,48,43},
{18,22,0,3},
{18,22,12,3},
// ...
{19,7,24,3},
{19,7,40,3},
{19,7,52,30},
{19,8,0,38},
{19,8,12,41},
{19,8,20,46},
{19,8,36,54},
{19,8,52,65},
{19,9,4,87},
{19,9,8,99},
{19,9,20,139},
{19,9,32,122},
{19,9,44,124},
{19,10,0,149},
{19,10,4,165},
{19,10,12,171},
{19,10,28,115},
{19,10,40,107},
{19,10,44,143},
{19,10,56,156},
{19,11,5,165},
{19,11,8,137},
{19,11,20,170},
{19,11,24,174},
{19,11,36,176},
{19,11,48,173},
{19,12,0,178},
{19,12,12,178},
{19,12,32,179},
{19,12,44,172},
{19,12,48,174},
{19,12,56,178},
{19,13,8,176},
{19,13,12,174},
{19,13,20,176},
{19,13,32,180},
{19,13,40,180},
{19,13,52,179},
{19,14,0,178},
{19,14,4,177},
{19,14,16,154},
{19,14,24,127},
{19,14,44,63},
{19,15,0,56},
{19,15,12,43},
{19,15,13,41},
{19,15,32,27},
{19,15,44,15},
{19,15,48,12},
{19,16,0,6},
{19,16,4,5},
{19,16,16,3},
{19,16,24,3},
{19,16,36,3},
{19,16,48,3},
{19,16,56,15},
{19,17,4,15},
{19,17,12,15},
{19,17,24,16},
{19,17,32,16},
{19,17,44,3},
{19,17,56,44},
{19,18,1,45},
{19,18,8,45},
{19,18,16,45},
{19,18,28,46},
{19,18,40,45},
{19,18,48,46},
{19,18,56,47},
{19,19,12,47},
{19,19,20,45},
{19,19,28,45, occType::OCC_WEAK, false, true, ALDataSample::SB_NONEECO}, // TV watching, small or no setback.
{19,19,32,46},
{19,19,44,45},
{19,20,0,45},
{19,20,12,46},
{19,20,20,46, occType::OCC_WEAK, false, true, ALDataSample::SB_NONEECO}, // TV watching, small or no setback.
{19,20,32,43},
{19,20,36,45},
{19,20,48,44},
{19,20,59,44},
{19,21,12,3, occType::OCC_NONE, true},  // Dark, just vacated.
{19,21,28,16}, // Unusual lighting, ie not the 'habitual' level.
{19,21,40,14},
{19,21,44,15}, // FIXME  // Lights on, TV watching.
{19,21,52,15},
{19,22,4,15},
{19,22,16,15},
{19,22,32,15},
{19,22,48,15},
{19,23,0,16},
{19,23,4,15},
{19,23,8,15},
{19,23,24,15},
{19,23,40,16},
{19,23,52,15},
{20,0,0,15},
{20,0,12,16},
{20,0,16,15},
{20,0,28,15},
{20,0,32,16},
{20,0,40,16},
{20,0,48,15},
{20,1,0,15},
{20,1,8,15},
{20,1,24,16},
{20,1,28,15},
{20,1,37,15},
{20,1,52,3},
{20,2,4,3},
{20,2,16,3},
// ...
{20,7,28,3},
{20,7,40,3},
{20,7,52,17},
{20,8,8,19},
{20,8,12,29},
{20,8,25,33},
{20,8,40,35},
{20,8,52,25},
{20,9,4,44},
{20,9,16,41},
{20,9,24,40},
{20,9,36,47},
{20,9,52,95},
{20,10,4,97},
{20,10,8,67},
{20,10,24,83},
{20,10,36,65},
{20,10,40,85},
{20,10,52,113},
{20,11,4,81},
{20,11,16,70},
{20,11,20,62},
{20,11,36,77},
{20,11,40,70},
{20,11,48,58},
{20,12,0,81},
{20,12,16,80},
{20,12,20,75},
{20,12,32,81},
{20,12,48,70},
{20,12,53,66},
{20,12,56,54},
{20,13,4,66},
{20,13,16,47},
{20,13,20,68},
{20,13,28,63},
{20,13,40,86},
{20,13,44,119},
{20,13,52,73},
{20,14,0,71},
{20,14,4,70},
{20,14,12,89},
{20,14,20,81},
{20,14,35,27},
{20,14,44,28},
{20,14,52,28},
{20,14,56,25},
{20,15,8,30},
{20,15,12,27},
{20,15,28,25},
{20,15,32,34},
{20,15,40,33},
{20,15,56,21},
{20,16,12,15},
{20,16,20,15},
{20,16,32,16},
{20,16,48,15},
{20,17,0,15},
{20,17,8,15},
{20,17,20,14},
{20,17,32,14},
{20,17,44,15},
{20,17,56,15},
{20,18,8,14},
{20,18,24,15},
{20,18,32,58},
{20,18,36,55},
{20,18,48,53},
{20,18,56,54},
{20,19,0,54},
{20,19,12,54},
{20,19,20,54},
{20,19,32,53},
{20,19,40,44},
{20,19,48,43},
{20,19,56,43},
{20,20,12,43},
{20,20,28,43},
{20,20,36,43},
{20,20,40,44},
{20,20,44,43},
{20,20,52,43},
{20,20,56,43},
{20,21,12,43},
{20,21,16,42},
{20,21,20,42},
{20,21,32,44},
{20,21,40,44},
{20,21,48,44},
{20,21,56,43},
{20,22,12,43},
{20,22,24,3},
{20,22,36,3},
// ...
{21,7,4,3},
{21,7,23,3},
{21,7,32,4},
{21,7,44,5},
{21,7,48,6},
{21,8,0,9},
{21,8,12,33},
{21,8,16,39},
{21,8,28,35},
{21,8,44,55},
{21,8,56,88},
{21,9,12,89},
{21,9,22,111},
{21,9,24,131},
{21,9,32,123},
{21,9,48,75},
{21,9,56,63},
{21,10,0,55},
{21,10,16,30},
{21,10,28,65},
{21,10,32,47},
{21,10,52,49},
{21,11,4,38},
{21,11,8,58},
{21,11,20,56},
{21,11,36,68},
{21,11,48,51},
{21,12,0,19},
{21,12,8,18},
{21,12,12,23},
{21,12,24,20},
{21,12,40,13},
{21,12,48,46},
{21,12,56,25},
{21,13,9,18},
{21,13,16,16},
{21,13,19,19},
{21,13,32,20},
{21,13,36,34},
{21,13,44,177},
{21,14,0,175},
{21,14,12,148},
{21,14,16,170},
{21,14,24,178},
{21,14,28,157},
{21,14,32,178},
{21,14,48,175},
{21,14,52,176},
{21,15,4,169},
{21,15,24,39},
{21,15,40,19},
{21,15,56,56},
{21,16,8,47},
{21,16,12,45},
{21,16,20,46},
{21,16,32,16},
{21,16,44,3},
{21,16,56,16},
{21,17,12,3},
{21,17,32,3},
{21,17,44,3},
{21,17,55,3},
{21,18,4,3},
{21,18,24,3},
{21,18,36,3},
{21,18,48,3},
{21,19,8,16},
{21,19,28,45},
{21,19,32,46},
{21,19,40,46},
{21,19,44,44},
{21,19,48,45},
{21,20,4,46},
{21,20,20,46},
{21,20,24,47},
{21,20,28,46},
{21,20,40,44},
{21,20,48,45},
{21,20,56,46},
{21,21,16,46},
{21,21,28,46},
{21,21,44,45},
{21,21,48,46},
{21,21,56,46},
{21,22,4,3},
{21,22,16,3},
// ...
{22,6,56,3},
{22,7,8,3},
{22,7,18,4},
{22,7,31,53},
{22,7,47,22},
{22,8,0,30},
{22,8,11,36},
{22,8,24,49},
{22,8,31,46},
{22,8,48,62},
{22,8,56,53},
{22,9,8,59},
{22,9,24,86},
{22,9,28,78},
{22,9,39,99},
{22,9,52,128},
{22,9,56,111},
{22,10,3,153},
{22,10,12,137},
{22,10,19,141},
{22,10,24,114},
{22,10,27,120},
{22,10,36,131},
{22,10,48,167},
{22,11,0,170},
{22,11,7,137},
{22,11,12,167},
{22,11,20,103},
{22,11,32,137},
{22,11,47,166},
{22,11,51,171},
{22,12,0,167},
{22,12,4,151},
{22,12,16,170},
{22,12,19,104},
{22,12,36,158},
{22,12,51,179},
{22,13,8,180},
{22,13,20,180},
{22,13,23,181},
{22,13,32,181},
{22,13,44,147},
{22,13,48,183},
{22,13,59,183},
{22,14,7,174},
{22,14,11,183},
{22,14,23,175},
{22,14,31,176},
{22,14,39,158},
{22,14,52,177},
{22,15,3,132},
{22,15,8,108},
{22,15,24,93},
{22,15,27,110},
{22,15,48,51},
{22,16,3,18},
{22,16,16,47},
{22,16,20,49},
{22,16,32,45},
{22,16,43,46},
{22,16,48,45},
{22,16,55,46},
{22,17,4,47},
{22,17,7,47},
{22,17,15,46},
{22,17,19,45},
{22,17,24,46},
{22,17,32,46},
{22,17,48,15},
{22,18,0,47},
{22,18,11,47},
{22,18,27,44},
{22,18,40,46},
{22,18,56,45},
{22,19,12,46},
{22,19,24,46},
{22,19,28,44},
{22,19,40,45},
{22,19,51,46},
{22,20,4,46},
{22,20,19,46},
{22,20,32,45},
{22,20,43,46},
{22,20,51,46},
{22,20,56,45},
{22,21,8,46},
{22,21,12,46},
{22,21,27,46},
{22,21,32,45},
{22,21,40,45},
{22,21,52,46},
{22,22,4,47},
{22,22,8,45},
{22,22,19,3},
{22,22,28,3},
// ...
{23,4,59,3},
{23,5,7,3},
{23,5,11,2},
{23,5,20,3},
{23,5,31,3},
// ...
{23,6,59,3},
{23,7,8,3},
{23,7,24,4},
{23,7,35,5},
{23,7,48,9},
{23,7,51,10},
{23,8,0,13},
{23,8,15,21},
{23,8,27,32},
{23,8,43,60},
{23,8,59,81},
{23,9,11,103},
{23,9,27,117},
{23,9,35,117},
{23,9,39,122},
{23,9,55,112},
{23,10,7,131},
{23,10,23,127},
{23,10,40,175},
{23,10,51,178},
{23,11,4,162},
{23,11,12,175},
{23,11,16,173},
{23,11,40,178},
{23,11,52,164},
{23,12,7,176},
{23,12,15,171},
{23,12,20,170},
{23,12,39,176},
{23,13,11,178},
{23,13,28,176},
{23,13,39,147},
{23,13,48,104},
{23,13,59,107},
{23,14,11,114},
{23,14,13,113},
{23,14,19,95},
{23,14,31,86},
{23,14,40,50},
{23,14,47,55},
{23,14,54,38},
{23,14,55,36},
{23,14,59,25},
{23,15,3,17},
{23,15,19,12},
{23,15,31,8},
{23,15,43,6},
{23,16,0,5},
{23,16,3,6},
{23,16,11,5},
{23,16,27,3},
{23,16,39,45, occType::OCC_PROBABLE, false, true, ALDataSample::SB_NONEMIN}, // TV watching, small or no setback.
{23,16,53,46},
{23,16,59,47},
{23,17,7,47},
{23,17,12,46},
{23,17,28,47},
{23,17,39,46},
{23,17,55,47, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEMIN}, // Lights on, TV watching.  FIXME: should be seen as WEAK occupancy, small or no setback.
{23,18,8,45},
{23,18,15,47},
{23,18,19,44},
{23,18,23,45},
{23,18,35,45},
{23,18,55,45},
{23,19,8,47},
{23,19,11,44},
{23,19,23,45},
{23,19,32,44},
{23,19,35,44},
{23,19,47,46},
{23,19,59,46},
{23,20,19,44},
{23,20,31,46},
{23,20,43,46},
{23,20,47,44},
{23,20,59,46},
{23,21,19,44},
{23,21,31,44},
{23,21,35,46},
{23,21,47,44},
{23,22,3,44},
{23,22,7,46},
{23,22,19,3},
{23,22,35,3},
// ...
{24,6,59,3, ALDataSample::NO_OCC_EXPECTATION, true, false, ALDataSample::SB_MAX}, // Dark, vacant, max setback.
{24,7,15,3, ALDataSample::NO_OCC_EXPECTATION, true, false}, // Dark, vacant.
{24,7,23,4},
{24,7,43,8},
{24,7,53,15},
{24,7,59,19},
{24,8,11,35},
{24,8,15,39},
{24,8,27,52},
{24,8,29,56},
{24,8,35,67},
{24,8,51,74},
{24,9,1,80, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Light but vacant.
{24,9,11,103},
{24,9,15,113},
{24,9,35,137},
{24,9,50,147},
{24,9,55,129},
{24,9,59,117},
{24,10,15,109, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Light but vacant.
{24,10,35,113},
{24,10,47,104},
{24,10,59,154},
{24,11,7,159, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Light but vacant.
{24,11,19,174},
{24,11,23,173},
{24,11,27,175},
{24,11,39,177},
{24,11,50,179},
{24,11,55,177},
{24,12,11,153, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Light but vacant.
{24,12,19,166},
{24,12,23,175},
{24,12,31,173},
{24,12,39,170},
{24,12,47,175},
{24,12,55,137},
{24,12,59,139},
{24,13,3,109, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Light but vacant.
{24,13,11,112},
{24,13,23,67},
{24,13,35,51},
{24,13,39,90},
{24,13,47,92},
{24,14,3,134, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Light but vacant.
{24,14,19,96},
{24,14,35,62},
{24,14,51,89},
{24,15,3,59, ALDataSample::NO_OCC_EXPECTATION, false, false, ALDataSample::SB_MINECO}, // Light but vacant.
{24,15,7,60},
{24,15,16,29},
{24,15,19,28},
{24,15,23,39},
{24,15,43,22},
{24,15,55,11},
{24,16,3,48, occType::OCC_PROBABLE, false, true, ALDataSample::SB_NONE}, // Lights on, TV watching.
{24,16,15,47},
{24,16,23,46, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEMIN},
{24,16,31,43},
{24,16,43,46},
{24,16,51,46},
{24,17,3,43, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEMIN},
{24,17,19,44},
{24,17,27,46},
{24,17,39,45},
{24,17,43,44, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEMIN},
{24,17,47,46},
{24,17,59,46},
{24,18,15,46},
{24,18,27,45, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEMIN},
{24,18,43,47},
{24,18,55,47},
{24,18,59,46},
{24,19,3,47, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEMIN},
{24,19,15,44},
{24,19,19,46},
{24,19,23,46, occType::OCC_WEAK, false, true, ALDataSample::SB_NONEMIN}, // TV watching?
{24,19,39,44},
{24,19,55,46},
{24,20,3,45, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEMIN},
{24,20,7,47},
{24,20,23,45, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEMIN},
{24,20,27,44},
{24,20,39,46, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEMIN},
{24,20,43,45},
{24,20,55,46, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEMIN}, // occType::OCC_WEAK}, // TV watching?
{24,21,3,44},
{24,21,7,46},
{24,21,15,44},
{24,21,29,47, ALDataSample::NO_OCC_EXPECTATION, false, true, ALDataSample::SB_NONEMIN},
{24,21,35,46},
{24,21,47,46},
{24,21,55,46, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::SB_NONEMIN}, // occType::OCC_WEAK}, // TV watching?  FIXME: should show occupancy.
{24,22,7,47, ALDataSample::NO_OCC_EXPECTATION, false, ALDataSample::SB_NONEMIN}, // occType::OCC_WEAK}, // TV watching?  FIXME: should show occupancy.
{24,22,11,46},
{24,22,15,3, ALDataSample::NO_OCC_EXPECTATION, true}, // Dark.
    { }
    };
// "3l" fortnight to 2016/11/24 looking for habitual artificial lighting to watch TV, etc.
// This is not especially intended to check response to other events, though will verify some key ones.
TEST(AmbientLightOccupancyDetection,sample3leveningTV)
{
    simpleDataSampleRun(sample3leveningTV);
}
