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


    // Minimum and maximum bounds target temperatures;
    // degrees C/Celsius/centigrade, strictly positive.
    // Minimum is some way above 0C to avoid freezing pipework
    // allowing for small measurement errors and non-uniform temperatures.
    // Maximum is set a little below boiling/100C for DHW for safety.
    // Setbacks and uplifts cannot move temperature targets outside this
    // range for safety.
    //
    // Minimum temperature setting allowed (to avoid freezing, allowing for offsets at temperature sensor, etc).
    static constexpr uint8_t MIN_TARGET_C = 5;
    // Maximum temperature setting allowed (eg for DHW).
    static constexpr uint8_t MAX_TARGET_C = 95;

    // 18C is a safe room temperature even for the slightly infirm according to
    // NHS England 2014:
    //    http://www.nhs.uk/Livewell/winterhealth/Pages/KeepWarmKeepWell.aspx
    // Small babies have relatively poor thermoregulation so a device
    // with setbacks may not be suitable for them, else ~18C is good:
    //    http://www.nhs.uk/conditions/pregnancy-and-baby/pages/reducing-risk-cot-death.aspx
    // so could possibly be marked explicitly on the control.
    // 21C is recommended living temperature in retirement housing:
    //     http://ipc.brookes.ac.uk/publications/pdf/Identifying_the_health_gain_from_retirement_housing.pdf
    static constexpr uint8_t SAFE_ROOM_TEMPERATURE = 18; // Safe for most purposes.

    // Templated set of constant parameters derived from common arguments.
    // Can be tweaked to parameterise different products,
    // or to make a bigger shift such as to DHW control.
    //   * ecoMin  basic target frost-protection temperature (C).
    //   * comMin  minimum temperature in comfort mode at any time,
    //     even for frost protection (C).
    //   * ecoWarm  'warm' in ECO mode.
    //   * comWarm  'warm' in comfort mode.
    //   * bakeLiftC  defaults to 10C (TODO-980) to ensure that very
    //     rarely BAKE will fail to trigger even in in shoulder seasons.
    //   * setbackECO  usual 'ECO' temperature setback defaults to 3C
    //     for ~30% potential savings eg in UK winter.
    //   * setbackFULL  'FULL' temperature setback defaults to 6C
    //     to minimise night-time triggering of heating where no central
    //     clock.
    template<uint8_t ecoMinC, uint8_t comMinC, uint8_t ecoWarmC, uint8_t comWarmC,
             uint8_t bakeLiftC = 10, uint8_t setbackECO = 3, uint8_t setbackFULL = 6>
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

            // Raise target by this many degrees in 'BAKE' mode (strictly +ve).
            // DHD20160927 (TODO-980) default lift raised from 5C to 10C
            // so as to ensure reliable trigger even in in shoulder seasons.
            static constexpr uint8_t BAKE_UPLIFT = bakeLiftC;

            // Initial minor setback degrees C (strictly positive).
            // Note that 1C heating setback may result in ~8% saving in the UK.
            // This may be the maximum setback generally applied
            // with a comfort temperature setting for example.
            static constexpr uint8_t SETBACK_DEFAULT = 1;
            // Enhanced setback, eg in eco mode, for extra energy savings.
            // This may be the most-used setback and thus
            // the key determinant of potential savings.
            // More than SETBACK_DEFAULT, less than SETBACK_FULL.
            static constexpr uint8_t SETBACK_ECO =
                OTV0P2BASE::fnmax(setbackECO, uint8_t(SETBACK_DEFAULT+1));
            // Full setback degrees C (strictly positive and significantly,
            // ie several degrees, greater than SETBACK_DEFAULT,
            // no more than MIN_TARGET_C).
            // Deeper setbacks increase potential energy savings
            // at the cost of a longer time to return to target temperature.
            // Deeper setbacks at night help avoid noisy/unwanted heating then.
            // See (recommending 13F/7C setback to 55F/12C):
            //     https://www.mge.com/images/pdf/brochures/residential/setbackthermostat.pdf
            // See (suggesting an 8hr setback, 1F set-back = 1% energy savings):
            //     http://joneakes.com/jons-fixit-database/1270-How-far-back-should-a-set-back-thermostat-be-set
            // See savings, comfort and condensation with setbacks > ~4C
            // (eg ~15% saving for 6C setback overnight):
            //     https://www.cmhc-schl.gc.ca/en/co/grho/grho_002.cfm
            // Preferably no more than than MIN_TARGET_C
            // to avoid problems with unsigned arithmetic.
            static constexpr uint8_t SETBACK_FULL =
                OTV0P2BASE::fnmax(setbackFULL, uint8_t(SETBACK_ECO+1));
        };

    // Mechanism to make ValveControlParameters available at run-time.
    class ValveControlParametersRTBase
      {
      public:
        const uint8_t BAKE_UPLIFT;
        const uint8_t SETBACK_DEFAULT;
        const uint8_t SETBACK_ECO;
        const uint8_t SETBACK_FULL;
        // Construct an instance.
        ValveControlParametersRTBase(
          const uint8_t _BAKE_UPLIFT,
          const uint8_t _SETBACK_DEFAULT,
          const uint8_t _SETBACK_ECO,
          const uint8_t _SETBACK_FULL)
          : BAKE_UPLIFT(_BAKE_UPLIFT),
            SETBACK_DEFAULT(_SETBACK_DEFAULT), SETBACK_ECO(_SETBACK_ECO), SETBACK_FULL(_SETBACK_FULL)
          { }
      };
    template<typename VCP>
    class ValveControlParametersRT : public ValveControlParametersRTBase
      {
      public:
        constexpr ValveControlParametersRT()
          : ValveControlParametersRTBase(
            VCP::BAKE_UPLIFT,
            VCP::SETBACK_DEFAULT,
            VCP::SETBACK_ECO,
            VCP::SETBACK_FULL)
          { }
      };


    // Typical radiator valve control parameters.
    typedef ValveControlParameters<
        // Default frost-protection (minimum) temperatures in degrees C, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
        // Setting frost temperatures at a level likely to protect (eg) fridge/freezers as well as water pipes.
        // Note that 5C or below carries a risk of hypothermia: http://ipc.brookes.ac.uk/publications/pdf/Identifying_the_health_gain_from_retirement_housing.pdf
        // Other parts of the room may be somewhat colder than where the sensor is, so aim a little over 5C.
        // 14C avoids risk of raised blood pressure and is a generally safe and comfortable sleeping temperature.
        // Note: BS EN 215:2004 S5.3.5 says maximum setting must be <= 32C, minimum in range [5C,12C].
        // 15C+ may help avoid mould/mold risk from condensation, see: http://www.nea.org.uk/Resources/NEA/Publications/2013/Resource%20-%20Dealing%20with%20damp%20and%20condensation%20%28lo%20res%29.pdf
        6,  // Target FROST temperature for ECO bias.
        14, // Target FROST temperature for Comfort bias.

        // Target 'warm' temperatures, strictly positive, in range [<frost+1>,MAX_TARGET_C].
        // Set so that mid-point is at ~19C (BRE and others regard this as minimum comfort temperature)
        // and half the scale will be below 19C and thus save ('ECO') compared to typical UK room temperatures.
        // (17/18 good for energy saving at ~1C below typical UK room temperatures of ~19C in 2012).
        // Note: BS EN 215:2004 S5.3.5 says maximum setting must be <= 32C, minimum in range [5C,12C].
        17, // Target WARM temperature for ECO bias.
        21  // Target WARM temperature for Comfort bias.
        > DEFAULT_ValveControlParameters;

    // Slightly raised upper threshold compared to default so that range [18,21]
    // (which includes recommended bedroom and living room temperatures)
    // is in the central (non-ECO, non-comfort) part of the range (TODO-1059).
    // Proposed default radiator valve control parameters from TRV2.
    typedef ValveControlParameters<
        // Default frost-protection (minimum) temperatures in degrees C, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
        // Setting frost temperatures at a level likely to protect (eg) fridge/freezers as well as water pipes.
        // Note that 5C or below carries a risk of hypothermia: http://ipc.brookes.ac.uk/publications/pdf/Identifying_the_health_gain_from_retirement_housing.pdf
        // Other parts of the room may be somewhat colder than where the sensor is, so aim a little over 5C.
        // 14C avoids risk of raised blood pressure and is a generally safe and comfortable sleeping temperature.
        // Note: BS EN 215:2004 S5.3.5 says maximum setting must be <= 32C, minimum in range [5C,12C].
        // 15C+ may help avoid mould/mold risk from condensation, see: http://www.nea.org.uk/Resources/NEA/Publications/2013/Resource%20-%20Dealing%20with%20damp%20and%20condensation%20%28lo%20res%29.pdf
        6,  // Target FROST temperature for ECO bias.
        14, // Target FROST temperature for Comfort bias.

        // Target 'warm' temperatures, strictly positive, in range [<frost+1>,MAX_TARGET_C].
        // Set so that mid-point is close to 19C (BRE and others regard 19C as minimum comfort temperature)
        // and half the scale will be below 19C and thus save ('ECO') compared to typical UK room temperatures.
        // (17/18 good for energy saving at ~1C below typical UK room temperatures of ~19C in 2012).
        // Note: BS EN 215:2004 S5.3.5 says maximum setting must be <= 32C, minimum in range [5C,12C].
        // As low as 12C recommended for cellar/stairs, and as high as 22C for bathrooms: http://www.energie-environnement.ch/conseils-de-saison/97-bien-utiliser-la-vanne-thermostatique
        16, // Target WARM temperature for ECO bias.
        22  // Target WARM temperature for Comfort bias.
        > Proposed_DEFAULT_ValveControlParameters;

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

    // Default 'BAKE' minutes, ie time to crank heating up to BAKE setting (minutes, strictly positive, <255).
    static constexpr uint8_t DEFAULT_BAKE_MAX_M = 31;

    // Default typical minimum valve percentage open to be considered actually/significantly open; [1,99].
    // Anything like this will usually be shut or very minimal flows.
    // Setting this above 0 delays calling for heat from a central boiler
    // until water is likely able to flow.
    // (It may however be possible to scavenge some heat if a particular valve
    // opens below this and the circulation pump is already running, for example.)
    // DHD20130522: FHT8V + valve heads in use have not typically been open
    //     until ~6%; at least one opens at ~20%.
    // DHD20151014: may need reduction to <5 for use in high-pressure systems.
    // DHD20151030: with TRV1.x dead reckoning, valves may not open until ~45%.
    // Allowing valve to linger at just below this level
    // without calling for heat when shutting
    // may allow comfortable boiler pump overrun in older systems
    // with no/poor bypass to avoid overheating.
    static constexpr uint8_t DEFAULT_VALVE_PC_MIN_REALLY_OPEN = 15;

    // Safer value for valves to very likely be significantly open, in range [DEFAULT_VALVE_PC_MIN_REALLY_OPEN+1,DEFAULT_VALVE_PC_MODERATELY_OPEN-1].
    // NOTE: below this value is will let a boiler switch off,
    // ie a value at/above this is a call for heat from the boiler also.
    // so DO NOT CHANGE this value between boiler and valve code lightly.
    // DHD20151030: with TRV1.x dead reckoning, valves may not open until ~45%.
    static constexpr uint8_t DEFAULT_VALVE_PC_SAFER_OPEN = 50;

    // Default valve percentage at which significant heating power is being provided [DEFAULT_VALVE_PC_SAFER_OPEN+1,99].
    // For many valves much of the time this may be effectively fully open,
    // ie no change beyond this makes significant difference to heat delivery.
    // NOTE: at/above this value a strong call for heat from the boiler also,
    // so DO NOT CHANGE this value between boiler and valve code lightly.
    // Should be significantly higher than DEFAULT_MIN_VALVE_PC_REALLY_OPEN.
    // DHD20151014: has been ~33% but ~67% more robust, eg for all-in-one units.
    static constexpr uint8_t DEFAULT_VALVE_PC_MODERATELY_OPEN = 67;


    // Default maximum time to allow boiler to run on to allow for lost TXs etc (min).
    // This is also the default minimum-off time to avoid short cycling.
    // Should be (much) greater than the gap between transmissions
    // (eg ~2m for FHT8V/FS20, 4m for the TRV1 secure protocol circa 2016).
    // Should be greater than the run-on time at the OpenTRV boiler unit
    // and any further pump run-on time.
    // Valves should possibly linger open at least this
    // plus maybe an extra minute or so for timing skew
    // for systems with poor/absent bypass to help avoid overheating.
    // Having too high a linger time value may cause excessive
    // temperature overshoot.
    static constexpr uint8_t DEFAULT_MAX_RUN_ON_TIME_M = 5;

    // Typical time for boiler to start pumping hot water to rads from off (min).
    // This includes an allowance for TX time/interval from valves,
    // and some time for hot water to reach the rads.
    // These numbers are for a typical single-family European household,
    // so not a huge sprawling mansion, with a reasonably specified boiler,
    // and not quite at the coldest depths of winter etc (eg 90% level).
    static constexpr uint8_t BOILER_RESPONSE_TIME_FROM_OFF = 5;

    // Default delay in minutes after increasing flow before re-closing is allowed.
    // This is to avoid excessive seeking/noise
    // in the presence of strong draughts for example.
    // Too large a value may cause significant temperature overshoots
    // and thus energy waste.
    // Attempting to run rads less than the typical boiler minimum-on time
    // is probably nugatory.
    // There's probably little value in running most rads less than ~10 minutes.
    static constexpr uint8_t DEFAULT_ANTISEEK_VALVE_RECLOSE_DELAY_M =
        OTV0P2BASE::fnmax(uint8_t(10),
            OTV0P2BASE::fnmax(BOILER_RESPONSE_TIME_FROM_OFF,
                              DEFAULT_MAX_RUN_ON_TIME_M));
    // Default delay in minutes after restricting flow before re-opening is allowed.
    // This is to avoid excessive seeking/noise
    // in the presence of strong draughts for example.
    // Attempting turn rads off for less than typical boiler minimum-off time
    // is probably nugatory.
    // A value larger than DEFAULT_ANTISEEK_VALVE_RECLOSE_DELAY_M helps savings
    // but may prevent a poorly-functioning radiator providing enough heat.
    // Too large a value may cause significant temperature undershoots
    // and discomfort/annoyance.
    static constexpr uint8_t DEFAULT_ANTISEEK_VALVE_REOPEN_DELAY_M =
        OTV0P2BASE::fnmax(DEFAULT_ANTISEEK_VALVE_RECLOSE_DELAY_M+1,
                          2*DEFAULT_MAX_RUN_ON_TIME_M);
    // Typical heat turn-down response time; in minutes, strictly positive.
    static constexpr uint8_t DEFAULT_TURN_DOWN_RESPONSE_TIME_M =
        (DEFAULT_ANTISEEK_VALVE_RECLOSE_DELAY_M + 3);

    // Assumed daily budget in cumulative (%) valve movement for battery-powered devices.
    // A run from one end-stop to the other is 100%; a full round-trip 200%.
    // DHD20171118 observed 600% more realistic target; was ~400% (DHD20141230).
    static constexpr uint16_t DEFAULT_MAX_CUMULATIVE_PC_DAILY_VALVE_MOVEMENT = 600;


    }


// Further notes on 'safe' heating and fuel poverty, eg:
//     http://newrytimes.com/2018/02/23/fuel-poverty-awareness-day-reduce-energy-costs-and-get-help/
//     "Fuel Poverty Awareness Day – reduce energy costs and get help"
//     Top tips for keeping warm at home:
//    Wear multiple layers of clothing and a hat and gloves, even indoors if it is cold;
//    Heat your main living room to around 18-21C (64-70F) and the rest of the house to at least 16C (61F);
//    Heat all the rooms you use in the day;
//    If you can’t heat all your rooms, make sure that you keep your living room warm throughout the day;
//    It is important to make sure your heating is safe and that your house is properly ventilated, to reduce the risk of carbon monoxide poisoning.
//    If you have electric controls for your heating, set the timer on your heating to come on before you get up and switch off when you go to bed;
//    In very cold weather set the heating to come on earlier, rather than turn the thermostat up, so you won’t be cold while you wait for your home to heat up.


#endif
