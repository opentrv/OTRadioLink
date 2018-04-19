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
                           Deniz Erbilgin 2016-2018
*/

#include <cstdint>
#include <cstdio>

#include "OTRadValve_AbstractRadValve.h"
#include "OTRadValve_ModelledRadValve.h"

namespace OTRadValve
{
namespace PortableUnitTest
{

/**
 * @brief   Test an all in one unit.
 */
namespace TMTRHC {
static constexpr uint_fast8_t valveUpdateTime = 60;  // Length of valve update cycle in seconds.
/**
 * Helper class to handle updating and storing state of TRV.
 */
class ThermalModelValve
{
protected:
    uint_fast8_t valvePCOpen;
    OTRadValve::ModelledRadValveInputState is0;
    OTRadValve::ModelledRadValveState<> rs0;
public:
    ThermalModelValve(const uint_fast8_t startValvePCOpen, const float targetTemp) : valvePCOpen(startValvePCOpen)
        { is0.targetTempC = targetTemp; }
    /**
     * @brief   Set current temperature at valve and calculate new valve state.
     *          Should be called once per valve update cycle (see valveUpdateTime).
     */
    void tick(const float curTempC) {
        is0.setReferenceTemperatures((uint_fast16_t)(curTempC * 16));
        rs0.tick(valvePCOpen, is0, NULL);
    }
    float getValvePCOpen() { return valvePCOpen; }
    float getTargetTempC() { return is0.targetTempC; }
};

/**
 * Helper class to handle updating and storing state of TRV.
 * Runs a binary valve control algorithm.
 */
class ThermalModelBinaryValve
{
protected:
    uint_fast8_t valvePCOpen;
    OTRadValve::ModelledRadValveInputState is0;
    OTRadValve::ModelledRadValveState<true> rs0;
public:
    ThermalModelBinaryValve(const uint_fast8_t startValvePCOpen, const float targetTemp) : valvePCOpen(startValvePCOpen)
        { is0.targetTempC = targetTemp; }
    /**
     * @brief   Set current temperature at valve and calculate new valve state.
     *          Should be called once per valve update cycle (see valveUpdateTime).
     */
    void tick(const float curTempC) {
        is0.setReferenceTemperatures((uint_fast16_t)(curTempC * 16));
        rs0.tick(valvePCOpen, is0, NULL);
    }
    float getValvePCOpen() { return valvePCOpen; }
    float getTargetTempC() { return is0.targetTempC; }
};

}

// Holds references to a valve and temperature sensor
// and models the how the former drives the latter
// given the characteristics of the room, boiler, etc.
// NOTE: All constants are the absolute values for the room.
// Heat capacities etc. should be calculated from room size etc. before feeding into the model!
namespace TMB {

static bool verbose = false;
static bool splitUnit = false;

/**
 * @brief   Physical constants modelling heat transfer from the room to the  
 *          rest of the world.
 * 
 * TODO
 */
struct ThermalModelRoomParams_t
{
    // Conductance of the air to the wall in W/K.
    float conductance_21;
    // Conductance through the wall in W/K.
    float conductance_10;
    // Conductance of the wall to the outside world in W/K.
    float conductance_0W;
    // Capacitance of the TODO in J/K.
    float capacitance_2;
    // Capacitance of the TODO in J/K.
    float capacitance_1;
    // Capacitance of the TODO in J/K.
    float capacitance_0;
};
static const ThermalModelRoomParams_t roomParams_Default {
    500, 300, 50, 350000, 1300000, 7000000,
};

/**
 * @brief   Physical constants modelling the radiator.
 */
struct ThermalModelRadParams_t
{
    // Conductance from the radiator to the room in W/K.
    float conductance;
    // Maximum temperature the radiator can reach in C.
    float maxTemp;
};

/**
 * @brief   Current state of the room.
 */ 
struct ThermalModelState_t
{
    // Inside air temperature in C
    float airTemperature {0.0};
    // ??
    float roomTemp {0.0};
    float t1 {0.0};
    float t0 {0.0};
    // Temperature of the outside world in C.
    float outsideTemp {0.0};
    // Temperature at the rad valve in C.
    float valveTemp {0.0};

    // Everything but the outside temp is assumed to start at room temperature.
    constexpr ThermalModelState_t(float startTemp) :
        airTemperature(startTemp),
        roomTemp(startTemp),
        t1(startTemp),
        t0(startTemp),
        valveTemp(startTemp) {}
    constexpr ThermalModelState_t(float startTemp, float _outsideTemp) : 
        airTemperature(startTemp),
        roomTemp(startTemp),
        t1(startTemp),
        t0(startTemp),
        outsideTemp(_outsideTemp),
        valveTemp(startTemp) {}
};


class ThermalModelBase
    {
    protected:
        // Simulated valve, internal.
        OTRadValve::RadValveMock radValveInternal;
        
        // Simulated room temperature, internal.
        OTV0P2BASE::TemperatureC16Mock roomTemperatureInternal;

        // Constants & variables
        ThermalModelState_t roomVars;
        const ThermalModelRoomParams_t roomParams;
        const ThermalModelRadParams_t radParams;

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
        float calcHeatFlowRad(const float airTemp, const uint8_t radValveOpenPC) const 
        {
            // convert radValveOpenPC to radiator temp (badly)
            const float radTemp = (2.0 * (float)radValveOpenPC) - 80.0;
            // Making sure the radiator temp does not exceed sensible values
            const float scaledRadTemp = (radTemp < radParams.maxTemp) ? radTemp : radParams.maxTemp;
            // Calculate heat transfer, making sure rad temp cannot go below air temperature.
            return (radTemp > airTemp) ? (heatTransfer(radParams.conductance, scaledRadTemp, airTemp)) : 0.0;
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
        float calcValveTemp(const float airTemp, const float localTemp, const float heatFlowFromRad) const
        {
            static constexpr float thermalConductanceRad {0.05f};  // fixme literal is starting estimate for thermal resistance
            static constexpr float thermalConductanceRoom {10.0f};
            const float heatIn = heatFlowFromRad * thermalConductanceRad;
            const float heatOut = heatTransfer(thermalConductanceRoom, localTemp, airTemp);
            const float valveHeatFlow = heatIn - heatOut;
            const float newLocalTemp = localTemp + (valveHeatFlow / 5000);  // fixme literal is starting estimate for thermal capacitance
            return newLocalTemp;
        }

    public:
        ThermalModelBase(
            const float startTemp,
            const ThermalModelRoomParams_t _roomParams,
            const ThermalModelRadParams_t _radParams = {25.0, 70.0}) : 
            roomVars(startTemp),
            roomParams(_roomParams), 
            radParams(_radParams)
        {
            // Init internal temp of the mock temp sensor
            roomTemperatureInternal.set((int16_t)(startTemp * 16.0));
            // Init valve position of the mock rad valve.
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
            const float heat_in = calcHeatFlowRad(roomVars.airTemperature, radValveInternal.get());

            // Calculate change in heat of each segment.
            const float heatDelta_21 = heatTransfer(roomParams.conductance_21, roomVars.roomTemp, roomVars.t1);
            const float heatDelta_10 = heatTransfer(roomParams.conductance_10, roomVars.t1, roomVars.t0);
            const float heatDelta_0w = heatTransfer(roomParams.conductance_0W, roomVars.t0, roomVars.outsideTemp);

            // Calc new heat of each segment.
            const float heat_21 = heat_in - heatDelta_21;
            const float heat_10 = heatDelta_21 - heatDelta_10;
            const float heat_out = heatDelta_10 - heatDelta_0w;

            // Calc new temps.
            roomVars.roomTemp += heat_21 / roomParams.capacitance_2;
            roomVars.t1 += heat_10 / roomParams.capacitance_1;
            roomVars.t0 += heat_out / roomParams.capacitance_0;

            // Calc temp of thermostat. This is the same as the room temp in a splot unit.
            if(!splitUnit) { roomVars.valveTemp = calcValveTemp(roomVars.roomTemp, roomVars.valveTemp, heat_in); }
            else { roomVars.valveTemp = roomVars.roomTemp; }
            if(verbose) { }  // todo put print out in here
        }
        float getAirTemperature() { return (roomVars.roomTemp); }
        float getValveTemperature() {return (roomVars.valveTemp);}
    };

/**
 * @brief   Helper function that prints a JSON frame in the style of an OpenTRV frame.
 * @param   i: current model iteration
 * @param   airTempC: average air temperature of the room (key 'T|C').
 * @param   valveTempC: temperature as measured by the TRV (key 'TV|C). This should be the same as airTempC in a split unit TRV.
 * @param   targetTempC: target room temperature (key 'tT|C').
 * @param   valvePCOpen: current valve position in % (key 'v|%').
 */
static void printFrame(const unsigned int i, const float airTempC, const float valveTempC, const float targetTempC, const uint_fast8_t valvePCOpen) {
    fprintf(stderr, "[ \"%u\", \"\", {\"T|C\": %.2f, \"TV|C\": %.2f, \"tT|C\": %.2f, \"v|%%\": %u} ]\n",
            i, airTempC, valveTempC, targetTempC, valvePCOpen);
}

struct InitConditions_t {
    // Room start temp
    const float roomTempC;
    const float targetTempC;
    // keep track of valve positions.
    const uint_fast8_t valvePCOpen;
};
struct TempBoundsC_t {
    float max = 0.0;
    float min = 100.0;
};

/**
 * @brief   Whole room model
 */
class RoomModelBasic
{
    //////  Constants
    const InitConditions_t initCond;

    ///// Variables
    // Keep track of maximum and minimum room temps.
    TempBoundsC_t tempBounds;

    // Delay in radiator responding to change in valvePCOpen. Should possibly be asymmetric. todo move into room model.
    std::vector<uint_fast8_t> radDelay;

    // Models
    TMTRHC::ThermalModelValve valve;
    ThermalModelBase model;

public:
    RoomModelBasic(InitConditions_t init) :
        initCond(init),
        radDelay(5, initCond.valvePCOpen),
        valve(initCond.valvePCOpen, initCond.targetTempC),
        model(initCond.roomTempC, roomParams_Default) {  }

    // Advances the model by 1 second
    void tick(const uint32_t seconds)
    {
        const float valveTempC = model.getValveTemperature(); // current air temperature in C
        const float airTempC = model.getAirTemperature();

        if(0 == (seconds % TMTRHC::valveUpdateTime)) {  // once per minute tasks.
            const uint_fast8_t valvePCOpen = valve.getValvePCOpen();
            if (verbose) {
                printFrame(seconds, airTempC, valveTempC, initCond.targetTempC, valvePCOpen);
            }
            valve.tick(valveTempC);
            radDelay.erase(radDelay.begin());
            radDelay.push_back(valvePCOpen);
        }
        model.calcNewAirTemperature(radDelay.front());
        tempBounds.max = (tempBounds.max > airTempC) ? tempBounds.max : airTempC;
        tempBounds.min = ((tempBounds.min < airTempC) && ((60 * 100) < seconds)) ? tempBounds.min : airTempC;  // avoid comparing during the first 100 mins
    }

    TempBoundsC_t getTempBounds() { return (tempBounds); }
};

}

}
}
