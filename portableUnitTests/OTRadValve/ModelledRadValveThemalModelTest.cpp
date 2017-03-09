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
namespace TMB {
static constexpr uint_fast8_t valveUpdateTime = 60;  // Length of valve update cycle.

bool verbose = false;
bool splitUnit = false;

class ThermalModelBase
    {
    protected:
        // Simulated valve, internal.
        OTRadValve::RadValveMock radValveInternal;
        
        // Simulated room temperature, internal.
        OTV0P2BASE::TemperatureC16Mock roomTemperatureInternal;

        // Constants & variables
        const float conductance_21;
        const float conductance_10;
        const float conductance_0W;
        const float capacitance_2;
        const float capacitance_1;
        const float capacitance_0;
        float airTemperature;
        float roomTemp;
        float t1;
        float t0;
        float outsideTemp;
        const float radiatorConductance;
        const float maxRadiatorTemp;
        float valveTemp;
        // Internal methods
        /**
         * @brief   Calculate heat transfer through a thermal resistance. Flow from temp1 to temp2 is positive.
         */
        static float heatTransfer(const float conductance, const float temp1, const float temp2)
        {
            return conductance * (temp1 - temp2);
        }
        /**
         * @brief   Calculate heat input this interval by radiator.
         * @note    Heat flow into the room is positive.
         * @note    Assumes that radiator temperature (and therefore heat input):
         *          - increases linearly.
         *          - increases monotonically.
         *          - Cannot be below air temperature (the radiator cannot sink heat).
         * @retval  Heat transfer into room from radiator, in J
         */
        float calcHeatFlowRad(const float airTemp, const uint8_t radValveOpenPC) {
            // convert radValveOpenPC to radiator temp (badly)
            const float radTemp = (2.0 * (float)radValveOpenPC) - 80;
            // Making sure the radiator temp does not exceed sensible values
            const float scaledRadTemp = (radTemp < maxRadiatorTemp) ? radTemp : maxRadiatorTemp;
            // Calculate heat transfer, making sure rad temp cannot go below air temperature.
            return (radTemp > airTemp) ? (heatTransfer(radiatorConductance, scaledRadTemp, airTemp)) : 0.0;
        }
        /**
         * @brief   Calculate temp seen by valve this interval.
         * @note    Heat flow into the room is positive.
         * @note    Assumes that radiator temperature (and therefore heat input):
         *          - increases linearly.
         *          - increases monotonically.
         *          - Cannot be below air temperature (the radiator cannot sink heat).
         * @retval  Heat transfer into room from radiator, in J
         */
        float calcValveTemp(const float airTemp, const float localTemp, const float heatFlowFromRad) {
            static constexpr float thermalConductanceRad = 0.05f;  // fixme literal is starting estimate for thermal resistance
            static constexpr float thermalConductanceRoom = 10.0f;
            const float heatIn = heatFlowFromRad * thermalConductanceRad;
            const float heatOut = heatTransfer(thermalConductanceRoom, localTemp, airTemp);
            const float valveHeatFlow = heatIn - heatOut;
            const float newLocalTemp = localTemp + (valveHeatFlow / 5000);  // fixme literal is starting estimate for thermal capacitance
            return newLocalTemp;
        }

    public:
        ThermalModelBase(const float _conductance_21,      // [W/K]
                         const float _conductance_10,   // [J/K]
                         const float _conductance_0W,   // [W/K]
                         const float _capacitance_2,   // [J/K]
                         const float _capacitance_1,   // [J/K]
                         const float _capacitance_0,   // [J/K]
                         const float startTemp,             // [C]
                         const float _outsideTemp = 0.0,          // [C]
                         const float _radiatorConductance = 25.0,  // [W/K]
                         const float _maxRadiatorTemp = 70.0)      // [C]
                       : conductance_21(_conductance_21),
                         conductance_10(_conductance_10),
                         conductance_0W(_conductance_0W),
                         capacitance_2(_capacitance_2),
                         capacitance_1(_capacitance_1),
                         capacitance_0(_capacitance_0),
                         airTemperature(startTemp),
                         roomTemp(startTemp),
                         t1(startTemp),
                         t0(startTemp),
                         outsideTemp(_outsideTemp),
                         radiatorConductance(_radiatorConductance),
                         maxRadiatorTemp(_maxRadiatorTemp),
                         valveTemp(startTemp)
//                         storedHeat(0.0)
        {
            const float temperatureC16 = (int16_t)(startTemp * 16.0);
//            storedHeat = startTemp * storageCapacitance;
            roomTemperatureInternal.set(temperatureC16);
            radValveInternal.set(0);
        };

        // Read-only view of simulated room temperature.
        const OTV0P2BASE::TemperatureC16Base &roomTemperature = roomTemperatureInternal;

        // Read-only view of simulated radiator valve.
        const OTRadValve::AbstractRadValve &radValve = radValveInternal;

        // Calculate new temperature
        void calcNewAirTemperature(uint8_t radValveOpenPC) {
            radValveInternal.set(radValveOpenPC);
            // Calc heat in from rad
            const float heat_in = calcHeatFlowRad(airTemperature, radValveInternal.get());
            // Calc heat flow from seg2 to seg1
            const float heat_21 = heat_in - heatTransfer(conductance_21, roomTemp, t1);
            // Calc heat flow from seg1 to seg0
            const float heat_10 = heatTransfer(conductance_21, roomTemp, t1) - heatTransfer(conductance_10, t1, t0);
            // Calc heat flow from seg0 to world.
            const float heat_out = heatTransfer(conductance_10, t1, t0) - heatTransfer(conductance_0W, t0, outsideTemp);
            // Calc new temps.
            roomTemp += heat_21 / capacitance_2;
            t1 += heat_10 / capacitance_1;
            t0 += heat_out / capacitance_0;
            // Calc temp of thermostat. This is the same as the room temp in a splot unit.
            if(!splitUnit) valveTemp = calcValveTemp(roomTemp, valveTemp, heat_in);
            else valveTemp = roomTemp;
            if(verbose) { }  // todo put print out in here
        }
        float getAirTemperature() { return roomTemp; }
        float getValveTemperature() {return valveTemp;}
    };

class ThermalModelValveBase
{
protected:
    uint_fast8_t valvePCOpen;
    OTRadValve::ModelledRadValveInputState is0;
    OTRadValve::ModelledRadValveState rs0;
public:
    ThermalModelValveBase(const uint_fast8_t startValvePCOpen, const float targetTemp) : valvePCOpen(startValvePCOpen)
        { is0.targetTempC = targetTemp; }
    void tick(const float curTempC) {
        is0.setReferenceTemperatures((uint_fast16_t)(curTempC * 16));
        rs0.tick(valvePCOpen, is0, NULL);
    }
    float getValvePCOpen() { return valvePCOpen; }
    float getTargetTempC() { return is0.targetTempC; }
};
}

//// Basic tests of RadValveModel room with valve fully off.
//// Hijacked as a test against my python model (DE20170207)
//TEST(ModelledRadValveThermalModel, roomModelCold)
//{
//    TMB::ThermalModelBase model(38.4, 1000000.0, 1.0, 41780.3625, 20.0);
//    for(auto i = 0; i < 1000; ++i) {
//        model.calcNewAirTemperature(0);
//    }
//    EXPECT_NEAR(8.1, model.getAirTemperature(), 0.01); // Value for same inputs in my python model is: 8.096024519493666 C
//}
//
//// Basic tests of RadValveModel room with valve fully on.
//// Hijacked as a test against my python model (DE20170207)
//TEST(ModelledRadValveThermalModel,roomModelHot)
//{
//    TMB::ThermalModelBase model(38.4, 1000000.0, 1.0, 41780.3625, 20.0);
//    for(auto i = 0; i < 1000; ++i) {
//        model.calcNewAirTemperature(100);
//    }
//    EXPECT_EQ(100, model.radValve.get());
//    EXPECT_NEAR(25.88, model.getAirTemperature(), 0.01);
//}


// Basic tests of RadValveModel room with valve fully on.
// Starts at an unrealistically high temperature (40C).
//TEST(ModelledRadValveThermalModel, roomHotBasic)
//{
//    // Room start temp
//    const float startTempC = 16.0f;
//    // Target temperature without setback.
//    const float targetTempC = 19.0f;
//    // Valve starts fully shut.
//    uint8_t valvePCOpen = 100;
//    //
//    OTRadValve::ModelledRadValveInputState is0((uint_fast16_t)(startTempC * 16));
//    is0.targetTempC = targetTempC;
//    OTRadValve::ModelledRadValveState rs0;
//
//    TMB::ThermalModelBase model(500, 300, 50,
//                                350000, 1300000, 7000000,
//                                startTempC);
//
//    // Keep track of maximum and minimum room temps.
//    float maxRoomTempC = 0.0;
//    float minRoomTempC = 100.0;
//    for(auto i = 0; i < 20000; ++i) {
//        const float curTempC = model.getAirTemperature(); // current air temperature in C
//        if(0 == (i % 60)) {
//            if((60 * 60 * 2) == i) valvePCOpen = 0;  // run for one hour then shut.
////            fprintf(stderr, "[ \"%u\", \"\", {\"T|C16\": %.2f, \"tT|C\": %.2f, \"v|%%\": %u} ]\n", i, curTempC, targetTempC, valvePCOpen);
//        }
//        model.calcNewAirTemperature(valvePCOpen);
//        maxRoomTempC = (maxRoomTempC > curTempC) ? maxRoomTempC : curTempC;
//        minRoomTempC = ((minRoomTempC < curTempC) && (1000 < i)) ? minRoomTempC : curTempC;  // avoid comparing during initial warm-up.
//    }
//}


TEST(ModelledRadValveThermalModel, roomHotControlled)
{
    TMB::splitUnit = false;
    // Room start temp
    const float startTempC = 16.0f;
    const float targetTempC = 19.0f;
    // Keep track of maximum and minimum room temps.
    //float maxRoomTempC = 0.0;
    //float minRoomTempC = 100.0;
    // keep track of valve positions.
    const uint_fast8_t startingValvePCOpen = 0;
    // Set up TRV.
//    OTRadValve::ModelledRadValveInputState is0((uint_fast16_t)(startTempC * 16));
//    is0.targetTempC = targetTempC;
//    OTRadValve::ModelledRadValveState rs0;
    // Set up room model.
    TMB::ThermalModelValveBase valve(startingValvePCOpen, targetTempC);
    TMB::ThermalModelBase model(500, 300, 50,
                                350000, 1300000, 7000000,
                                startTempC);
    // Delay in radiator responding to change in valvePCOpen. Should possibly be asymmetric. todo move into room model.
    std::vector<uint_fast8_t> radDelay(5, startingValvePCOpen);
    for(auto i = 0; i < 20000; ++i) {
        const float curTempC = model.getValveTemperature(); // current air temperature in C
        if(0 == (i % 60)) {  // once per minute tasks.
            const uint_fast8_t valvePCOpen = valve.getValvePCOpen();
            fprintf(stderr, "[ \"%u\", \"\", {\"T|C\": %.2f, \"TV|C\": %.2f, \"tT|C\": %.2f, \"v|%%\": %u} ]\n", i, model.getAirTemperature(), curTempC, targetTempC, valvePCOpen);
//            is0.setReferenceTemperatures((uint_fast16_t)(curTempC * 16));
//            rs0.tick(valvePCOpen, is0, NULL);
            valve.tick(curTempC);
            radDelay.erase(radDelay.begin());
            radDelay.push_back(valvePCOpen);
        }
        model.calcNewAirTemperature(radDelay.front());
//        maxRoomTempC = (maxRoomTempC > curTempC) ? maxRoomTempC : curTempC;
//        minRoomTempC = ((minRoomTempC < curTempC) && (1000 < i)) ? minRoomTempC : curTempC;  // avoid comparing during initial warm-up.
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
