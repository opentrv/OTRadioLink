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

Author(s) / Copyright (s): Damon Hart-Davis 2016--2018
*/

/*
 * Exported state for OTV0P2BASE_SensorAmbientLightOccupancy tests.
 */


#ifndef PUT_OTRADIOLINK_AMBIENTLIGHTOCCUPANCYDETECTIONTEST_H
#define PUT_OTRADIOLINK_AMBIENTLIGHTOCCUPANCYDETECTIONTEST_H

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>
#include <OTRadValve.h>
#include "OTV0P2BASE_SensorAmbientLightOccupancy.h"


namespace OTV0P2BASE {
namespace PortableUnitTest {


// Import occType enum values.
typedef ::OTV0P2BASE::SensorAmbientLightOccupancyDetectorInterface::occType occType;

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
            SB_MINMAX, // Some setback from MIN to MAX.
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
        unsigned long currentMinute() const { return((((d * 24L) + H) * 60L) + M); }

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

// Stats bending type: BL_FROMSTATS (0) is closest to embedded use.
enum blending_t : uint8_t { BL_FROMSTATS = 0, BL_NONE, BL_HALFHOURMIN, BL_HALFHOUR, BL_BYMINUTE, BL_END };

// Collection of 'flavoured' events in one run.
class SimpleFlavourStatCollection final
    {
    private:
        bool sensitive;
        blending_t blending;

    public:
        constexpr SimpleFlavourStatCollection(bool sensitive_ = false, blending_t blending_ = BL_FROMSTATS)
            : sensitive(sensitive_), blending(blending_) { }

        bool getSensitive() const { return(sensitive); }
        blending_t getBlending() const { return(blending); }

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

        // Checking failure to anticipate occupancy by reducing setback.
        // This may exclude circumstances where
        // setback reduction is undesirable,
        // eg where bringing the heating on may wake people up early.
        SimpleFlavourStats occupancyAnticipationFailureNotAfterSleep;
        SimpleFlavourStats occupancyAnticipationFailureLargeNotAfterSleep;

        // Checking setback accuracy vs actual occupation/vacancy.
        SimpleFlavourStats setbackTooFar; // Excessive setback.
        SimpleFlavourStats setbackInsufficient; // Insufficient setback.

        // Checking time at various significant energy-saving setback levels.
        SimpleFlavourStats setbackAtLeastDEFAULT;
        SimpleFlavourStats setbackAtLeastECO;
        SimpleFlavourStats setbackAtMAX;
    };


// Some trivial data samples.

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


} }

#endif
