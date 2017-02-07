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
class ThermalModelBase
    {
    protected:
        // Simulated valve, internal.
        OTRadValve::RadValveMock radValveInternal;
        
        // Simulated room temperature, internal.
        OTV0P2BASE::TemperatureC16Mock roomTemperatureInternal;

        // Constants
        const float radiatorConductance;
        const float wallConductance;
        const float storageCapacitance;
        const float storageConductance;
        const float airCapacitance;
        float outsideTemp;
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
            const float radTemp = 2.0 * (float)radValveOpenPC - 80.0;
            // Calculate heat transfer, making sure rad temp cannot go below air temperature.
            return radTemp > airTemp ? ((radTemp - airTemp) * radiatorConductance) : airTemp;
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
        ThermalModelBase(float _radiatorConductance,
                         float _wallConductance,
                         float _storageCapacitance,
                         float _storageConductance,
                         float _airCapacitance)
                       : radiatorConductance(_radiatorConductance),
                         wallConductance(_wallConductance),
                         storageCapacitance(_storageCapacitance),
                         storageConductance(_storageConductance),
                         airCapacitance(_airCapacitance),
                         outsideTemp(0.0),
                         storedHeat(0.0)
        {
//            Missing things:
//            - starting temp
//                - also affects stored heat
//            - set outside temp
        };

        // Read-only view of simulated room temperature.
        const OTV0P2BASE::TemperatureC16Base &roomTemperature = roomTemperatureInternal;

        // Read-only view of simulated radiator valve.
        const OTRadValve::AbstractRadValve &radValve = radValveInternal;

        // Calculate new temperature
        int16_t calcNewAirTemperature() {
            int16_t temperatureC16 = roomTemperatureInternal.read();
            float temperature = ((float)temperatureC16) / 16.0;
            float sumHeats = 0.0;
            float deltaHeat = calcHeatStored(temperature, storedHeat);
            storedHeat -= deltaHeat;  // all heat flows are positive into the air!
            sumHeats += calcHeatFlowRad(temperature, radValveInternal.get());
            sumHeats += calcHeatFlowWalls(temperature, outsideTemp);
            sumHeats += deltaHeat;
            temperature += (sumHeats * (1.0 / airCapacitance));
            temperatureC16 = (int16_t)(temperature / 16.0);
            roomTemperatureInternal.set(temperatureC16);
            return temperatureC16;
        }
    };


// Basic tests against thermal model.
TEST(ModelledRadValveThermalModel,basic)
{
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
