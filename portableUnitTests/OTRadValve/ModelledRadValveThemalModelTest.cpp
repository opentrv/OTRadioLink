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
 * OTRadValve ModelledRadValve thermal model tests.
 *
 * Aim is to, for example, model different
 * radiator efficacies, valve behaviours, boiler speeds, radio loss, etc;
 * to ensure that responsiveness, temperature regulation
 * and valve movement/noise/energy are OK.
 *
 * Model for all-in-one and split unit configurations.
 *
 * This can even be extended to DHW tests.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>

#include "OTRadValve_AbstractRadValve.h"
#include "OTRadValve_ModelledRadValve.h"


// Holds references to a valve and temperature sensor
// and models the how the former drives the latter
// given the characteristics of the room, boiler, etc.
// NOTE: All constants are the absolute values for the room.
// Heat capacities etc. should be calculated from room size etc. before feeding into the model!
class ThermalModelBase
    {
    protected:
        // Simulated valve, internal.
        OTRadValve::RadValveMock radValveInternal;
        
        // Simulated room temperature, internal.
        OTV0P2BASE::TemperatureC16Mock roomTemperatureInternal;

        // Constants & variables
        float airTemperature;
        float outsideTemp;
        const float radiatorConductance;
        const float wallConductance;
        const float storageCapacitance;
        const float storageConductance;
        const float airCapacitance;
        float storedHeat;

        // Internal methods
        /**
         * @brief   Calculate heat input this interval by radiator.
         * @note    Heat flow into the room is positive.
         * @note    Assumes that radiator temperature (and therefore heat input):
         *          - increases linearly.
         *          - increases monotonically.
         *          - Cannot be below air temperature (the radiator cannot sink heat).
         */
        float calcHeatFlowRad(const float airTemp, const uint8_t radValveOpenPC) {
            // convert radValveOpenPC to radiator temp (badly)
            const float radTemp = (2.0 * (float)radValveOpenPC) - 80.0;
            // Calculate heat transfer, making sure rad temp cannot go below air temperature.
            return (radTemp > airTemp) ? ((radTemp - airTemp) * radiatorConductance) : 0.0;
        }
        /**
         * @brief   Calculate heat input this interval through the walls.
         * @note    Heat flow into the room is positive.
         * @note    This is the heat loss, but is modelled as a heat input for consistency.
         *          - Should return a negative value if outside temp is colder than inside temp.
         */
        float calcHeatFlowWalls(const float airTemp, const float outsideTemp) {
            return ((outsideTemp - airTemp) * wallConductance);
        }
        /**
         * @brief   Calculate the heat flow into the room from miscellaneous heat capacitances.
         * @note    Intended to model the effects things within the room other than air
         *          (e.g. walls, furniture) storing and releasing heat.
         */
        float calcHeatStored(const float airTemp, const float storedHeat) {
            return (((storedHeat / storageCapacitance) - airTemp) * storageConductance);
        }

    public:
        ThermalModelBase(const float startTemp,             // [C]
                         const float _outsideTemp,          // [C]
                         const float _radiatorConductance,  // [W/K]
                         const float _wallConductance,      // [W/K]
                         const float _storageCapacitance,   // [J/K]
                         const float _storageConductance,   // [W/K]
                         const float _airCapacitance)       // [J/K]
                       : airTemperature(startTemp),
                         outsideTemp(_outsideTemp),
                         radiatorConductance(_radiatorConductance),
                         wallConductance(_wallConductance),
                         storageCapacitance(_storageCapacitance),
                         storageConductance(_storageConductance),
                         airCapacitance(_airCapacitance),
                         storedHeat(0.0)
        {
            const float temperatureC16 = (int16_t)(startTemp * 16.0);
            storedHeat = startTemp * storageCapacitance;
            roomTemperatureInternal.set(temperatureC16);
            radValveInternal.set(0);
        };

        // Read-only view of simulated room temperature.
        const OTV0P2BASE::TemperatureC16Base &roomTemperature = roomTemperatureInternal;

        // Read-only view of simulated radiator valve.
        const OTRadValve::AbstractRadValve &radValve = radValveInternal;

        // Calculate new temperature
        void calcNewAirTemperature(uint8_t radValveOpenPC) {
            float sumHeats = 0.0;
            float deltaHeat = calcHeatStored(airTemperature, storedHeat);
            radValveInternal.set(radValveOpenPC);
            storedHeat -= deltaHeat;  // all heat flows are positive into the air!
            sumHeats += calcHeatFlowRad(airTemperature, radValveInternal.get());
            sumHeats += calcHeatFlowWalls(airTemperature, outsideTemp);
            sumHeats += deltaHeat;
            airTemperature += (sumHeats * (1.0 / airCapacitance));
            int16_t temperatureC16 = (int16_t)(airTemperature * 16.0);
            roomTemperatureInternal.set(temperatureC16);
            fprintf(stderr, "T = %.1f C\n", airTemperature);
        }
        float getAirTemperature() { return airTemperature; }
    };


// Basic tests of RadValveModel room with valve fully off.
// Hijacked as a test against my python model (DE20170207)
TEST(ModelledRadValveThermalModel, roomModelCold)
{
    ThermalModelBase model(20.0, 0.0, 25.0, 38.4, 1000000.0, 1.0, 41780.3625);
    for(auto i = 0; i < 1000; ++i) {
        model.calcNewAirTemperature(0);
    }
    EXPECT_NEAR(8.1, model.getAirTemperature(), 0.01); // Value for same inputs in my python model is: 8.096024519493666 C
}

// Basic tests of RadValveModel room with valve fully on.
// Hijacked as a test against my python model (DE20170207)
TEST(ModelledRadValveThermalModel,roomModelHot)
{
    ThermalModelBase model(20.0, 0.0, 25.0, 38.4, 1000000.0, 1.0, 41780.3625);
    for(auto i = 0; i < 1000; ++i) {
        model.calcNewAirTemperature(100);
    }
    EXPECT_EQ(100, model.radValve.get());
    EXPECT_NEAR(41.14, model.getAirTemperature(), 0.01);
}


// Basic tests of RadValveModel room with valve fully on.
// Starts at an unrealistically high temperature (40C).
TEST(ModelledRadValveThermalModel, roomHotBasic)
{

    // Target temperature without setback.
    const uint8_t targetTempC = 19;
    // Valve starts fully shut.
    uint8_t valvePCOpen = 0;
    OTRadValve::ModelledRadValveInputState is0(281); // 281 ~ 17.6C.
    OTRadValve::ModelledRadValveState rs0;

    ThermalModelBase model(10.0, 0.0, 25.0, 38.4, 1000000.0, 1.0, 41780.3625);

    for(auto i = 0; i < 1000; ++i) {

        model.calcNewAirTemperature();
    }
}

/* TODO

Test for sticky / jammed / closed value calling for heat in stable temp room running boiler continually: TODO-1096

Test for difficult cases such as:
  * DHW (esp needing glacial as per Bo).
  * All-in-one (TRV1.x) on flow end of rad very slow to heat up
    so valve must let whole rad get warm to let room get warm
    (eg see 1g 2017/01/14 12:00 to 15:00 om tag 20170114-responsiveness).
  * All-in-one (TRV1.x) on rad with very poor air circulation.
  * Split unit (REV2+FHT8V style) with sensor close to and far from radiator.
  * All-in-one or split unit in draughty room.
  * All-in-one or split unit in room with door or window opened on cold day.
  * Behaviour in well-insulated (or otherwise) house
    with central timer only set for a few hours per day.
 */
