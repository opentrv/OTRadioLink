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

// Holds references to a valve and temperature sensor
// and models the how the former drives the latter
// given the characteristics of the room, boiler, etc.
// NOTE: All constants are the absolute values for the room.
// Heat capacities etc. should be calculated from room size etc. before feeding into the model!
namespace TMB {

static bool verbose = false;
static bool splitUnit = false;

// Length of valve model update cycle in seconds.
static constexpr uint_fast8_t valveUpdateTime = 60;

// Initial conditions of the room and valve.
struct InitConditions_t {
    // Room start temp
    const double roomTempC;
    const double targetTempC;
    // keep track of valve positions.
    const uint_fast8_t valvePCOpen;
};

/**
 * Helper class to handle updating and storing state of TRV.
 */
class ValveModelBase
{
public:
    // Initialise the model.
    virtual void init(const InitConditions_t init) = 0;
    /**
     * @brief   Set current temperature at valve and calculate new valve state.
     *
     * Should be called once per valve update cycle (see valveUpdateTime).
     * 
     * @param   curTempC: Current temperature in C.
     */
    virtual void tick(const double curTempC) = 0;
    virtual void tick(const double curTempC, const uint32_t /*seconds*/) { tick(curTempC); }
    // Get valve percentage open.
    virtual uint_fast8_t getValvePCOpen() const = 0;
    // get target temperature in C.
    virtual double getTargetTempC() const = 0;
    // Get the effective valve percentage open the model should use.
    virtual double getEffectiveValvePCOpen() const = 0;    
};

/**
 * Helper class to handle updating and storing state of TRV.
 * Runs a binary valve control algorithm.
 */
template<bool isBinary = false>
class ValveModel : public ValveModelBase
{
private:
    // Components required for the valve model.
    uint_fast8_t valvePCOpen {0};
    OTRadValve::ModelledRadValveInputState is0;
    OTRadValve::ModelledRadValveState<isBinary> rs0;

    // Delay in radiator responding to change in valvePCOpen. Should possibly be asymmetric.
    std::vector<uint_fast8_t> responseDelay = {0, 0, 0, 0, 0};


public:
    // Initialise the model with the room conditions..
    void init(const InitConditions_t init) override {
        valvePCOpen = init.valvePCOpen;
        is0.targetTempC = init.targetTempC;
        // Init
        responseDelay.assign(5, init.valvePCOpen);
    }
    /**
     * @brief   Set current temperature at valve and calculate new valve state.
     *
     * Should be called once per valve update cycle (see valveUpdateTime).
     * 
     * @param   curTempC: Current temperature in C.
     */
    void tick(const double curTempC) override {
        is0.setReferenceTemperatures((uint_fast16_t)(curTempC * 16));
        rs0.tick(valvePCOpen, is0, NULL);

        // May make more sense in the thermal model, but only needs to be run
        // once every time this function is called.
        responseDelay.erase(responseDelay.begin());
        responseDelay.push_back(valvePCOpen);
    }
    // 
    uint_fast8_t getValvePCOpen() const override { return (valvePCOpen); }
    double getTargetTempC() const override { return (is0.targetTempC); }
    double getEffectiveValvePCOpen() const override { return (responseDelay.front()); }
};


/**
 * @brief   Physical constants modelling heat transfer from the room to the  
 *          rest of the world.
 * 
 * TODO
 */
struct RoomParams_t
{
    // Conductance of the air to the wall in W/K.
    const double conductance_21;
    // Conductance through the wall in W/K.
    const double conductance_10;
    // Conductance of the wall to the outside world in W/K.
    const double conductance_0W;
    // Capacitance of the TODO in J/K.
    const double capacitance_2;
    // Capacitance of the TODO in J/K.
    const double capacitance_1;
    // Capacitance of the TODO in J/K.
    const double capacitance_0;
};

//Modelled on DHD's office (Valve 5s, EPC Band B house).
static const RoomParams_t roomParams_Default {
    500, 300, 50, 350000, 1300000, 7000000,
};

/**
 * @brief   Physical constants modelling the radiator.
 */
struct RadParams_t
{
    // Conductance from the radiator to the room in W/K.
    const double conductance;
    // Maximum temperature the radiator can reach in C.
    const double maxTemp;
};
static const RadParams_t radParams_Default {
    25.0, 70.0
};

/**
 * @brief   Current state of the room.
 */ 
struct ThermalModelState_t
{
    // Inside air temperature in C
    double airTemperature {0.0};
    // ??
    double roomTemp {0.0};
    double t1 {0.0};
    double t0 {0.0};
    // Temperature of the outside world in C.
    double outsideTemp {0.0};
    // Temperature at the rad valve in C.
    double valveTemp {0.0};
};

static void initThermalModelState(ThermalModelState_t& state, const InitConditions_t& init) {
    state.airTemperature = init.roomTempC;
    state.roomTemp = init.roomTempC;
    state.t0 = init.roomTempC;
    state.t1 = init.roomTempC;
    // state.outsideTemp = init.outsideTemp;
    state.valveTemp = init.roomTempC;
}

/**
 * @brief   Basic 3 segment lumped thermal model of a room.
 * 
 * Heat flows from a simulated radiator into the room and then through a wall
 * to the outside world. No air flow effects are simulated.
 * 
 * Additionally, heat flow to the radvalve is modelled to allow simulating its
 * position.
 */
class ThermalModelBasic
    {
    protected:
        // Simulated valve, internal.
        OTRadValve::RadValveMock radValveInternal;

        // Constants & variables
        ThermalModelState_t roomState;
        const RoomParams_t roomParams;
        const RadParams_t radParams;

        double radHeatFlow {0.0};

        // Internal methods
        /**
         * @brief   Calculate heat transfer through a thermal resistance. Flow from temp1 to temp2 is positive.
         */
        static double heatTransfer(const double conductance, const double temp1, const double temp2)
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
        double calcHeatFlowRad(const double airTemp, const uint8_t radValveOpenPC) const 
        {
            // convert radValveOpenPC to radiator temp (badly)
            const double radTemp = (2.0 * (double)radValveOpenPC) - 80.0;
            // Making sure the radiator temp does not exceed sensible values
            const double scaledRadTemp = (radTemp < radParams.maxTemp) ? radTemp : radParams.maxTemp;
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
        double calcValveTemp(const double airTemp, const double localTemp, const double heatFlowFromRad) const
        {
            static constexpr double thermalConductanceRad {0.05};  // fixme literal is starting estimate for thermal resistance
            static constexpr double thermalConductanceRoom {10.0};
            const double heatIn = heatFlowFromRad * thermalConductanceRad;
            const double heatOut = heatTransfer(thermalConductanceRoom, localTemp, airTemp);
            const double valveHeatFlow = heatIn - heatOut;
            const double newLocalTemp = localTemp + (valveHeatFlow / 5000);  // fixme literal is starting estimate for thermal capacitance
            return newLocalTemp;
        }

    public:
        ThermalModelBasic(
            const RoomParams_t _roomParams = roomParams_Default,
            const RadParams_t _radParams = radParams_Default) : 
            roomParams(_roomParams), 
            radParams(_radParams) {  }

        // Read-only view of simulated radiator valve.
        const OTRadValve::AbstractRadValve &radValve = radValveInternal;

        void init(const InitConditions_t init) {
            // Init the thermal model
            initThermalModelState(roomState, init);

            // Init valve position of the mock rad valve.
            radValveInternal.set(init.valvePCOpen);
        }

        // Calculate new temperature
        void calcNewAirTemperature(uint8_t radValveOpenPC) {
            radValveInternal.set(radValveOpenPC);
            // Calc heat in from rad
            const double heat_in = calcHeatFlowRad(roomState.airTemperature, radValveInternal.get());
            radHeatFlow = heat_in;

            // Calculate change in heat of each segment.
            const double heatDelta_21 = heatTransfer(roomParams.conductance_21, roomState.roomTemp, roomState.t1);
            const double heatDelta_10 = heatTransfer(roomParams.conductance_10, roomState.t1, roomState.t0);
            const double heatDelta_0w = heatTransfer(roomParams.conductance_0W, roomState.t0, roomState.outsideTemp);

            // Calc new heat of each segment.
            const double heat_21 = heat_in - heatDelta_21;
            const double heat_10 = heatDelta_21 - heatDelta_10;
            const double heat_out = heatDelta_10 - heatDelta_0w;

            // Calc new temps.
            roomState.roomTemp += heat_21 / roomParams.capacitance_2;
            roomState.t1 += heat_10 / roomParams.capacitance_1;
            roomState.t0 += heat_out / roomParams.capacitance_0;

            // Calc temp of thermostat. This is the same as the room temp in a splot unit.
            if(!splitUnit) { roomState.valveTemp = calcValveTemp(roomState.roomTemp, roomState.valveTemp, heat_in); }
            else { roomState.valveTemp = roomState.roomTemp; }
            if(verbose) { }  // todo put print out in here
        }
        ThermalModelState_t getState() const { return  (roomState); }
        double getHeatInput() const { return (radHeatFlow); }
    };

/**
 * @brief   Helper function that prints a JSON frame in the style of an OpenTRV frame.
 * @param   i: current model iteration
 * @param   roomTempC: average air temperature of the room (key 'T|C').
 * @param   valveTempC: temperature as measured by the TRV (key 'TV|C). This should be the same as roomTempC in a split unit TRV.
 * @param   targetTempC: target room temperature (key 'tT|C').
 * @param   valvePCOpen: current valve position in % (key 'v|%').
 */
static void printFrame(const unsigned int i, const ThermalModelState_t& state, const double targetTempC, const uint_fast8_t valvePCOpen) {
    // fprintf(stderr, "[ \"%u\", \"\", {\"T|C\": %.2f, \"TV|C\": %.2f, \"tT|C\": %.2f, \"v|%%\": %u} ]\n",
    //         i, state.airTemperature, state.valveTemp, state.targetTemp, valvePCOpen);
    fprintf(stderr, "[ \"%u\", \"\", {\"T|C\": %.2f, \"TV|C\": %.2f, \"tT|C\": %.2f, \"v|%%\": %u} ]\n",
        i, state.roomTemp, state.valveTemp, targetTempC, valvePCOpen);
}

// Struct for storing the max and min temperatures seen this test.
struct TempBoundsC_t {
    // Delay in minutes to wait before starting to record values.
    const uint32_t startDelayM = 100;
    // Maximum temperature observed in C
    double max = 0.0;
    // Minumum temperature observed in C
    double min = 100.0;
};

/**
 * @brief    Helper function for updating the bounds.
 * 
 * @param   bounds: Previous min and max temps.
 * @param   roomTemp: Current room temp
 */
static void updateTempBounds(TempBoundsC_t& bounds, const double roomTemp)
{
    bounds.max = (bounds.max > roomTemp) ? bounds.max : roomTemp;
    bounds.min = (bounds.min < roomTemp) ? bounds.min : roomTemp;  
}

/**
 * @brief   Helper function that handles ticking the model by 1 second.
 * 
 * @param   seconds: The current time elapsed.
 * @param   v: The valve model.
 * @param   m: The room model.
 */
static void internalModelTick(
    const uint32_t seconds, 
    ValveModelBase& v, 
    ThermalModelBasic& m)
{
    const uint_fast8_t valvePCOpen = v.getEffectiveValvePCOpen();
    // once per minute tasks.
    if(0 == (seconds % valveUpdateTime)) {
        const ThermalModelState_t state = m.getState();
        if (verbose) {
            printFrame(seconds, state, v.getTargetTempC(), valvePCOpen);
        }
        v.tick(state.valveTemp, seconds);
    }
    m.calcNewAirTemperature(valvePCOpen);
}


/**
 * @brief   Whole room model
 */
class RoomModelBasic
{

    ///// Variables
    // Keep track of maximum and minimum room temps.
    TempBoundsC_t tempBounds;

    // Models
    ValveModelBase& valve;
    ThermalModelBasic& model;

public:
    RoomModelBasic(const InitConditions_t init, ValveModelBase& _valve, ThermalModelBasic& _model) :
        valve(_valve), model(_model)
    { 
        valve.init(init);
        model.init(init);
    }

    // Advances the model by 1 second
    void tick(const uint32_t seconds)
    {
        internalModelTick(seconds, valve, model);

        // Ignore initially bringing the room to temperature.
        if (seconds > (60 * tempBounds.startDelayM)) {
            const ThermalModelState_t state = model.getState();
            updateTempBounds(tempBounds, state.roomTemp);
        }
    }

    TempBoundsC_t getTempBounds() const { return (tempBounds); }
};

}

}
}
