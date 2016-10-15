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
    template<uint8_t ecoMin, uint8_t comMin, uint8_t ecoWarm, uint8_t comWarm>
    class ValveControlParameters
        {
        public:
            // Basic frost protection threshold.
            // Must be in range [MIN_TARGET_C,MAX_TARGET_C[.
            static const uint8_t FROST = OTV0P2BASE::fnmin(OTV0P2BASE::fnmax(ecoMin, MIN_TARGET_C), MAX_TARGET_C);
            // Frost protection threshold temperature in eco-friendly / ECO-bias mode.
            // Must be in range [MIN_TARGET_C,FROST_COM[.
            static const uint8_t FROST_ECO = FROST;
            // Frost protection threshold temperature in comfort mode, eg to be safer for someone infirm.
            // Must be in range ]FROST_ECO,MAX_TARGET_C].
            static const uint8_t FROST_COM = OTV0P2BASE::fnmax(OTV0P2BASE::fnmin(comMin, MAX_TARGET_C), FROST_ECO);

            // Warm temperatures.
            // Warm temperature in eco-friendly / ECO-bias mode.
            // Must be in range [FROST_ECO+1,MAX_TARGET_C].
            static const uint8_t WARM_ECO = OTV0P2BASE::fnmax(OTV0P2BASE::fnmin(ecoWarm, MAX_TARGET_C), (uint8_t)(FROST_ECO+1));
            // Warm temperature in comfort mode.
            // Must be in range [FROST_COM+1,MAX_TARGET_C].
            static const uint8_t WARM_COM = OTV0P2BASE::fnmax(OTV0P2BASE::fnmin(comWarm, MAX_TARGET_C), (uint8_t)(FROST_COM+1));
            // Default 'warm' at a 'safe' temperature.
            static const uint8_t WARM = OTV0P2BASE::fnmax(WARM_ECO, SAFE_ROOM_TEMPERATURE);

            // Typical markings for a temperature scale.
            // Scale can run from eco warm -1 to comfort warm + 1, eg: * 16 17 18 >19< 20 21 22 BOOST
            // Bottom of range for adjustable-base-temperature systems.
            static const uint8_t TEMP_SCALE_MIN = (WARM_ECO-1);
            // Middle of range for adjustable-base-temperature systems; should be 'eco' baised.
            static const uint8_t TEMP_SCALE_MID = ((WARM_ECO + WARM_COM + 1)/2);
            // Top of range for adjustable-base-temperature systems.
            static const uint8_t TEMP_SCALE_MAX = (WARM_COM+1);
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

    }

#endif
