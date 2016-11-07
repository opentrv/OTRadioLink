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
 * OTRadValve TempControl tests.
 */

#include <gtest/gtest.h>

#include "OTRadValve_Parameters.h"
#include "OTRadValve_TempControl.h"


// Test for general sanity of TempControlTempPot_computeWARMTargetC().
// In particular, simulate some nominal REV7/DORM1/TRV1 numbers.
TEST(TempControl,TRV1TempControlTempPotcomputeWARMTargetC)
{
    //    // If true then be more verbose.
    //    const static bool verbose = false;

    // Parameters as for REV7/DORM1/TRV1 at 2016/10/27.
    typedef OTRadValve::ValveControlParameters<
        6,  // Target FROST temperature for ECO bias.
        14, // Target FROST temperature for Comfort bias.
        17, // Target WARM temperature for ECO bias.
        21  // Target WARM temperature for Comfort bias.
        > TRV1ValveControlParameters;

    const uint8_t usefulScale = 47; // hiEndStop - loEndStop + 1;
    const uint8_t loEndStop = 200; // Arbitrary.
    const uint8_t hiEndStop = loEndStop + usefulScale - 1;

    // Test extremes.
    const uint8_t tsmin = TRV1ValveControlParameters::TEMP_SCALE_MIN;
    const uint8_t tsmax = TRV1ValveControlParameters::TEMP_SCALE_MAX;
    EXPECT_EQ(tsmin, OTRadValve::TempControlTempPot_computeWARMTargetC<TRV1ValveControlParameters>(0, loEndStop, hiEndStop));
    EXPECT_EQ(tsmax, OTRadValve::TempControlTempPot_computeWARMTargetC<TRV1ValveControlParameters>(255, loEndStop, hiEndStop));
    EXPECT_EQ(tsmin, OTRadValve::TempControlTempPot_computeWARMTargetC<TRV1ValveControlParameters>(loEndStop, loEndStop, hiEndStop));
    EXPECT_EQ(tsmax, OTRadValve::TempControlTempPot_computeWARMTargetC<TRV1ValveControlParameters>(hiEndStop, loEndStop, hiEndStop));
    // Test for wiggle room.
    EXPECT_EQ(tsmin, OTRadValve::TempControlTempPot_computeWARMTargetC<TRV1ValveControlParameters>(loEndStop+1, loEndStop, hiEndStop));
    EXPECT_EQ(tsmax, OTRadValve::TempControlTempPot_computeWARMTargetC<TRV1ValveControlParameters>(hiEndStop-1, loEndStop, hiEndStop));

    // Test mid-point.
    const uint8_t tsmid = TRV1ValveControlParameters::TEMP_SCALE_MID;
    const uint8_t approxMidPoint = loEndStop + (usefulScale/2);
    EXPECT_EQ(tsmid, OTRadValve::TempControlTempPot_computeWARMTargetC<TRV1ValveControlParameters>(approxMidPoint, loEndStop, hiEndStop));
    // Test for wiggle room.
    EXPECT_EQ(tsmid, OTRadValve::TempControlTempPot_computeWARMTargetC<TRV1ValveControlParameters>(approxMidPoint-1, loEndStop, hiEndStop));
    EXPECT_EQ(tsmid, OTRadValve::TempControlTempPot_computeWARMTargetC<TRV1ValveControlParameters>(approxMidPoint+1, loEndStop, hiEndStop));
}


