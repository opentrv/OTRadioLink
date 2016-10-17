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
 * OpenTRV radiator valve basic parameters.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_PARAMETERS_H
#define ARDUINO_LIB_OTRADVALVE_PARAMETERS_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


    // Minimum and maximum bounds target temperatures; degrees C/Celsius/centigrade, strictly positive.
    // Minimum is some way above 0C to avoid freezing pipework even with small measurement errors and non-uniform temperatures.
    // Maximum is set a little below boiling/100C for DHW applications for safety.
    // Setbacks and uplifts cannot move temperature targets outside this range for safety.
    static const uint8_t MIN_TARGET_C = 5; // Minimum temperature setting allowed (to avoid freezing, allowing for offsets at temperature sensor, etc).
    static const uint8_t MAX_TARGET_C = 95; // Maximum temperature setting allowed (eg for DHW).

    // 18C is a safe room temperature even for the slightly infirm according to NHS England 2014:
    //    http://www.nhs.uk/Livewell/winterhealth/Pages/KeepWarmKeepWell.aspx
    // Small babies have relatively poor thermoregulation so a device with setbacks may not be suitable for them, else ~18C is good:
    //    http://www.nhs.uk/conditions/pregnancy-and-baby/pages/reducing-risk-cot-death.aspx
    // so could possibly be marked explicitly on the control.
    // 21C is recommended living temperature in retirement housing:
    //     http://ipc.brookes.ac.uk/publications/pdf/Identifying_the_health_gain_from_retirement_housing.pdf
    static const uint8_t SAFE_ROOM_TEMPERATURE = 18; // Safe for most purposes.


    // Templated set of constant parameters derived together from common arguments.
    // Can be tweaked to parameterise different products,
    // or to make a bigger shift such as to DHW control.
    //   * ecoMin  basic target frost-protection temperature (C).
    //   * comMin  minimum temperature in comfort mode at any time, even for frost protection (C).
    //   * ecoWarm  'warm' in ECO mode.
    //   * comWarm  'warm' in comfort mode.
    template<uint8_t ecoMinC, uint8_t comMinC, uint8_t ecoWarmC, uint8_t comWarmC,
             uint8_t bakeLiftC = 10, uint8_t bakeLiftM = 30>
    class ValveControlParameters
        {
        public:
            // Basic frost protection threshold.
            // Must be in range [MIN_TARGET_C,MAX_TARGET_C[.
            static constexpr uint8_t FROST = OTV0P2BASE::fnmin(OTV0P2BASE::fnmax(ecoMinC, MIN_TARGET_C), MAX_TARGET_C);
            // Frost protection threshold temperature in eco-friendly / ECO-bias mode.
            // Must be in range [MIN_TARGET_C,FROST_COM[.
            static constexpr uint8_t FROST_ECO = FROST;
            // Frost protection threshold temperature in comfort mode, eg to be safer for someone infirm.
            // Must be in range ]FROST_ECO,MAX_TARGET_C].
            static constexpr uint8_t FROST_COM = OTV0P2BASE::fnmax(OTV0P2BASE::fnmin(comMinC, MAX_TARGET_C), FROST_ECO);

            // Warm temperatures.
            // Warm temperature in eco-friendly / ECO-bias mode.
            // Must be in range [FROST_ECO+1,MAX_TARGET_C].
            static constexpr uint8_t WARM_ECO = OTV0P2BASE::fnmax(OTV0P2BASE::fnmin(ecoWarmC, MAX_TARGET_C), (uint8_t)(FROST_ECO+1));
            // Warm temperature in comfort mode.
            // Must be in range [FROST_COM+1,MAX_TARGET_C].
            static constexpr uint8_t WARM_COM = OTV0P2BASE::fnmax(OTV0P2BASE::fnmin(comWarmC, MAX_TARGET_C), (uint8_t)(FROST_COM+1));
            // Default 'warm' at a 'safe' temperature.
            static constexpr uint8_t WARM = OTV0P2BASE::fnmax(WARM_ECO, SAFE_ROOM_TEMPERATURE);

            // Typical markings for a temperature scale.
            // Scale can run from eco warm -1 to comfort warm + 1, eg: * 16 17 18 >19< 20 21 22 BOOST
            // Bottom of range for adjustable-base-temperature systems.
            static constexpr uint8_t TEMP_SCALE_MIN = (WARM_ECO-1);
            // Middle of range for adjustable-base-temperature systems; should be 'eco' baised.
            static constexpr uint8_t TEMP_SCALE_MID = ((WARM_ECO + WARM_COM + 1)/2);
            // Top of range for adjustable-base-temperature systems.
            static constexpr uint8_t TEMP_SCALE_MAX = (WARM_COM+1);

            // Raise target by this many degrees in 'BAKE' mode (strictly positive).
            // DHD20160927 TODO-980 raised from 5 to 10 to ensure very rarely fails to trigger in in shoulder season.
            static constexpr uint8_t BAKE_UPLIFT = bakeLiftC;
            // Maximum 'BAKE' minutes, ie time to crank heating up to BAKE setting (minutes, strictly positive, <255).
            static constexpr uint8_t BAKE_MAX_M = bakeLiftM;

            // Initial minor setback degrees C (strictly positive).  Note that 1C heating setback may result in ~8% saving in the UK.
            // This may be the maximum setback applied with a comfort bias for example.
            static constexpr uint8_t SETBACK_DEFAULT = 1;
            // Enhanced setback, eg in eco mode, for extra energy savings.  Not more than SETBACK_FULL.
            static constexpr uint8_t SETBACK_ECO = 1+SETBACK_DEFAULT;
            // Full setback degrees C (strictly positive and significantly, ie several degrees, greater than SETBACK_DEFAULT, less than MIN_TARGET_C).
            // Deeper setbacks increase energy savings at the cost of longer times to return to target temperatures.
            // See also (recommending 13F/7C setback to 55F/12C): https://www.mge.com/images/pdf/brochures/residential/setbackthermostat.pdf
            // See also (suggesting for an 8hr setback, 1F set-back = 1% energy savings): http://joneakes.com/jons-fixit-database/1270-How-far-back-should-a-set-back-thermostat-be-set
            // This must set back to no more than than MIN_TARGET_C to avoid problems with unsigned arithmetic.
            static constexpr uint8_t SETBACK_FULL = 4;
        };

    // Typical radiator valve control parameters.
    typedef ValveControlParameters<
        // Default frost-protection (minimum) temperatures in degrees C, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
        // Setting frost temperatures at a level likely to protect (eg) fridge/freezers as well as water pipes.
        // Note that 5C or below carries a risk of hypothermia: http://ipc.brookes.ac.uk/publications/pdf/Identifying_the_health_gain_from_retirement_housing.pdf
        // Other parts of the room may be somewhat colder than where the sensor is, so aim a little over 5C.
        // 14C avoids risk of raised blood pressure and is a generally safe and comfortable sleeping temperature.
        // Note: BS EN 215:2004 S5.3.5 says maximum setting must be <= 32C, minimum in range [5C,12C].
        // 15C+ may help mould/mold risk from condensation, see: http://www.nea.org.uk/Resources/NEA/Publications/2013/Resource%20-%20Dealing%20with%20damp%20and%20condensation%20%28lo%20res%29.pdf
        6,  // Target FROST temperature for ECO bias.
        14, // Target FROST temperature for Comfort bias.

        // Target 'warm' temperatures, strictly positive, in range [<frost+1>,MAX_TARGET_C].
        // Set so that mid-point is at ~19C (BRE and others regard this as minimum comfort temperature)
        // and half the scale will be below 19C and thus save ('eco') compared to typical UK room temperatures.
        // (17/18 good for energy saving at ~1C below typical UK room temperatures of ~19C in 2012).
        // Note: BS EN 215:2004 S5.3.5 says maximum setting must be <= 32C, minimum in range [5C,12C].
        17, // Target WARM temperature for ECO bias.
        21  // Target WARM temperature for Comfort bias.
        > DEFAULT_ValveControlParameters;

    // Typical DHW (Domestic Hot Water) valve control parameters.
    typedef ValveControlParameters<
        // Default frost-protection (minimum) temperatures in degrees C, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
        6,  // Target FROST temperature for ECO bias.
        20, // Target FROST temperature for Comfort bias.

        // Target 'warm' temperatures, strictly positive, in range [<frost+1>,MAX_TARGET_C].
        // 55C+ centre value with boost to 60C+ for DHW Legionella control where needed.
        // Note that the low end (~45C) is safe against scalding but may worry some for storage as a Legionella risk.
        45, // Target DHW WARM temperature for ECO bias.
        65  // Target DHW WARM temperature for Comfort bias.
        > DEFAULT_DHW_ValveControlParameters;


    // Default minimum valve percentage open to be considered actually/significantly open; [1,99].
    // Anything like this will usually be shut or very minimal flows.
    // Setting this above 0 delays calling for heat from a central boiler until water is likely able to flow.
    // (It may however be possible to scavenge some heat if a particular valve opens below this and the circulation pump is already running, for example.)
    // DHD20130522: FHT8V + valve heads in use have not typically been open until around 6%; at least one opens at ~20%.
    // DHD20151014: may need reduction to <5 for use in high-pressure systems.
    // DHD20151030: with initial dead-reckoning direct drive impl valves may not be open until ~45%.
    // Allowing valve to linger at just below this level without calling for heat when shutting
    // may allow comfortable boiler pump overrun in older systems with no/poor bypass to avoid overheating.
    static const uint8_t DEFAULT_VALVE_PC_MIN_REALLY_OPEN = 15;

    // Safer value for valves to very likely be significantly open, in range [DEFAULT_VALVE_PC_MIN_REALLY_OPEN+1,DEFAULT_VALVE_PC_MODERATELY_OPEN-1].
    // NOTE: below this value is likely to let a boiler switch off also,
    // ie a value at/above this is a definite call for heat.
    // so DO NOT CHANGE this value between boiler and valve code without good reason.
    // DHD20151030: with initial dead-reckoning direct drive impl valves may not be open until ~45%.
    static const uint8_t DEFAULT_VALVE_PC_SAFER_OPEN = 50;

    // Default valve percentage at which significant heating power is being provided [DEFAULT_VALVE_PC_SAFER_OPEN+1,99].
    // For many valves much of the time this may be effectively fully open,
    // ie no change beyond this makes significant difference to heat delivery.
    // NOTE: at/above this value is likely to force a boiler on also,
    // so DO NOT CHANGE this value between boiler and valve code without good reason.
    // Should be significantly higher than DEFAULT_MIN_VALVE_PC_REALLY_OPEN.
    // DHD20151014: has been ~33% but ~66% more robust, eg for tricky all-in-one units.
    static const uint8_t DEFAULT_VALVE_PC_MODERATELY_OPEN = 67;


    // Default maximum time to allow the boiler to run on to allow for lost call-for-heat transmissions etc.
    // Should be (much) greater than the gap between transmissions (eg ~2m for FHT8V/FS20).
    // Should be greater than the run-on time at the OpenTRV boiler unit and any further pump run-on time.
    // Valves may have to linger open at minimum of this plus maybe an extra minute or so for timing skew
    // for systems with poor/absent bypass to avoid overheating.
    // Having too high a linger time value may cause excessive temperature overshoot.
    static const uint8_t DEFAULT_MAX_RUN_ON_TIME_M = 5;

    // Default delay in minutes after increasing flow before re-closing is allowed.
    // This is to avoid excessive seeking/noise in the presence of strong draughts for example.
    // Too large a value may cause significant temperature overshoots and possible energy wastage.
    static const uint8_t DEFAULT_ANTISEEK_VALVE_RECLOSE_DELAY_M = 5;
    // Default delay in minutes after restricting flow before re-opening is allowed.
    // This is to avoid excessive seeking/noise in the presence of strong draughts for example.
    // Too large a value may cause significant temperature undershoots and discomfort/annoyance.
    static const uint8_t DEFAULT_ANTISEEK_VALVE_REOPEN_DELAY_M = (DEFAULT_ANTISEEK_VALVE_RECLOSE_DELAY_M*2);

    // Typical heat turn-down response time; in minutes, strictly positive.
    static const uint8_t DEFAULT_TURN_DOWN_RESPONSE_TIME_M = (DEFAULT_ANTISEEK_VALVE_RECLOSE_DELAY_M + 3);

    // Assumed daily budget in cumulative (%) valve movement for battery-powered devices.
    static const uint16_t DEFAULT_MAX_CUMULATIVE_PC_DAILY_VALVE_MOVEMENT = 400;


    }

#endif
