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

// Test that temp-pot mid-point (eg for DORM1/TRV1) is sane.
namespace MidSane
  {
  constexpr uint8_t usefulScale = 47; // hiEndStop - loEndStop + 1;
  constexpr uint8_t loEndStop = 200; // Arbitrary.
  constexpr uint8_t hiEndStop = loEndStop + usefulScale - 1;

  OTV0P2BASE::SensorTemperaturePotMock tp(loEndStop, hiEndStop);
  }
TEST(TempControl,MidSane)
{
    // Parameters as for REV7/DORM1/TRV1 at 2016/10/27.
    typedef OTRadValve::ValveControlParameters<
        6,  // Target FROST temperature for ECO bias.
        14, // Target FROST temperature for Comfort bias.
        17, // Target WARM temperature for ECO bias.
        21  // Target WARM temperature for Comfort bias.
        > TRV1ValveControlParameters;
    const uint8_t tsm = TRV1ValveControlParameters::TEMP_SCALE_MID;
    ASSERT_EQ(19, tsm);
    OTRadValve::TempControlTempPot
        <
        decltype(MidSane::tp), &MidSane::tp,
        TRV1ValveControlParameters
        > tctp0;
    EXPECT_TRUE(tctp0.hasEcoBias()) << "mid point should by default have an ECO bias";
    EXPECT_FALSE(tctp0.isComfortTemperature(tsm)) << "mid point should be neither strongly ECO nor comfort";
    EXPECT_FALSE(tctp0.isEcoTemperature(tsm)) << "mid point should be neither strongly ECO nor comfort";

    // Test again with current default parameter set, which may have changed from TRV1.5 glory days.
    typedef OTRadValve::DEFAULT_ValveControlParameters currentDefaults;
    const uint8_t tsmc = currentDefaults::TEMP_SCALE_MID;
    EXPECT_NEAR(19, tsmc, 2);
    OTRadValve::TempControlTempPot
        <
        decltype(MidSane::tp), &MidSane::tp,
        currentDefaults
        > tctp1;
    EXPECT_TRUE(tctp1.hasEcoBias()) << "mid point should by default have an ECO bias";
    EXPECT_FALSE(tctp1.isComfortTemperature(tsm)) << "mid point should be neither strongly ECO nor comfort";
    EXPECT_FALSE(tctp1.isEcoTemperature(tsm)) << "mid point should be neither strongly ECO nor comfort";
}

// Test for frost temperature response to high relative humidity (eg for DORM1/TRV1).
namespace FROSTRH
  {
  constexpr uint8_t usefulScale = 47; // hiEndStop - loEndStop + 1;
  constexpr uint8_t loEndStop = 200; // Arbitrary.
  constexpr uint8_t hiEndStop = loEndStop + usefulScale - 1;

  OTV0P2BASE::SensorTemperaturePotMock tp(loEndStop, hiEndStop);
  OTV0P2BASE::HumiditySensorMock rh;
  }
TEST(TempControl,FROSTRH)
{
    // Parameters as for REV7/DORM1/TRV1 at 2016/10/27.
    typedef OTRadValve::ValveControlParameters<
        6,  // Target FROST temperature for ECO bias.
        14, // Target FROST temperature for Comfort bias.
        17, // Target WARM temperature for ECO bias.
        21  // Target WARM temperature for Comfort bias.
        > TRV1ValveControlParameters;
    OTRadValve::TempControlTempPot
        <
        decltype(FROSTRH::tp), &FROSTRH::tp,
        TRV1ValveControlParameters // ,
        // class rh_t = OTV0P2BASE::HumiditySensorBase, const rh_t *rh = (const rh_t *)NULL
        > tctp0;
    EXPECT_TRUE(tctp0.hasEcoBias());
    // Normally frost temperature is fixed.
    const uint8_t ft = TRV1ValveControlParameters::FROST_ECO;
    EXPECT_EQ(ft, tctp0.getFROSTTargetC());

    OTRadValve::TempControlTempPot
        <
        decltype(FROSTRH::tp), &FROSTRH::tp,
        TRV1ValveControlParameters,
        decltype(FROSTRH::rh), &FROSTRH::rh
        > tctp;
     FROSTRH::rh.set(0, false);
     // Normally frost temperature is fixed.
     EXPECT_EQ(ft, tctp.getFROSTTargetC());
     FROSTRH::rh.set(100, true);
     // Normally frost temperature is fixed.
     const uint8_t hft = TRV1ValveControlParameters::FROST_COM;
     EXPECT_EQ(hft, tctp.getFROSTTargetC());
}
