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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2016
                           Deniz Erbilgin 2015--2016
*/

/*
 * Set of ENABLE_XXX flags and V0p2_REV for REV0.
 *
 * This should define (or #undef) ONLY symbols with names starting "ENABLE_"
 * and V0p2_REV.
 *
 * Specific sets by date may also be available.
 *
 * These are meant be be fairly stable over time;
 * it is more likely that new ENABLE_ flags may be introduced
 * than old ones change their status.
 *
 * Values here that are #undef are to show
 * that they are available to be defined in some configs.
 *
 * NOT to be included by ANY library routines,
 * only other CONFIG includes.
 */

#ifndef ARDUINO_LIB_OTV0P2_CONFIG_REV0_H
#define ARDUINO_LIB_OTV0P2_CONFIG_REV0_H


// Breadboard/stripboard/minimal designs.

#ifdef CONFIG_DHD_TESTLAB_REV0 // DHD's test lab breadboard with TRV.
// Revision of V0.2 board.
#define V0p2_REV 0 // REV0 covers DHD's breadboard (was first V0.2 PCB).
// IF DEFINED: minimise boot effort and energy eg for intermittently-powered energy-harvesting applications.
#define ENABLE_MIN_ENERGY_BOOT
//// Enable use of DS18B20 temp sensor.
//#define ENABLE_PRIMARY_TEMP_SENSOR_DS18B20
//// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
//#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// Anticipation logic not yet ready for prime-time.
//#define ENABLE_ANTICIPATION
//// Enable experimental voice detection.
//#define ENABLE_VOICE_SENSOR
// IF UNDEFINED: this unit cannot act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF UNDEFINED: don't allow RX of stats frames (since there is no easy way to plug in a serial connection to relay them!)
#undef ENABLE_STATS_RX
// IF DEFINED: initial direct motor drive design.
//#define ENABLE_V1_DIRECT_MOTOR_DRIVE
#endif

#ifdef CONFIG_BAREBONES
// use alternative loop
#define ALT_MAIN_LOOP
#define V0p2_REV 0
// Defaults for V0.2; have to be undefined if not required.  ***
// May require limiting clock speed and using some alternative peripherals/sensors.
#define ENABLE_SUPPLY_VOLTAGE_LOW_2AA
// Provide software RTC support by default.
#define ENABLE_RTC_INTERNAL_SIMPLE
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit *can* act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.  ***
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#undef ENABLE_STATS_TX
// IF DEFINED: allow minimal binary format in addition to more generic one: ~400 bytes code cost.
#undef ENABLE_MINIMAL_STATS_TXRX
// IF DEFINED: allow JSON stats frames alongside binary ones.
#undef ENABLE_JSON_OUTPUT
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable. ***
#undef ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#define ENABLE_CLI
// IF DEFINED: enable a full OpenTRV CLI.
#define ENABLE_FULL_OT_CLI
// IF DEFINED: enable a full OpenTRV UI with normal LEDs etc. ***
//#define ENABLE_FULL_OT_UI
// IF DEFINED: enable and extended CLI with a longer input buffer for example.
#undef ENABLE_EXTENDED_CLI
// IF DEFINED: minimise boot effort and energy eg for intermittently-powered energy-harvesting applications.  ***
#undef ENABLE_MIN_ENERGY_BOOT
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).   ***
#undef ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// IF DEFINED: enable use AVR's 'idle' mode to stop the CPU but leave I/O (eg Serial) running to save power.
// DHD20150920: CURRENTLY NOT RECOMMENDED AS STILL SEEMS TO CAUSE SOME BOARDS TO CRASH.
#define ENABLE_USE_OF_AVR_IDLE_MODE
// IF DEFINED: Use OTNullRadioLink instead of a radio module
// Undefine other radio //FIXME make this a part of the automatic stuff
#define USE_NULLRADIO
//#define USE_MODULE_SIM900
// things that break
// IF DEFINED: basic FROST/WARM temperatures are settable.
//#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.  ***
//#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1  UI_Minimal.cpp:1180:32: error: 'handleLEARN' was not declared in this scope
#define ENABLE_FHT8VSIMPLE //Control.cpp:1322:27: error: 'localFHT8VTRVEnabled' was not declared in this scope
// If LDR is not to be used then specifically define OMIT_... as below.
//#undef ENABLE_OCCUPANCY_DETECTION_FROM_AMBLIGHT //  LDR 'occupancy' sensing irrelevant for DHW. Messaging.cpp:232:87: error: 'class AmbientLight' has no member named 'getRaw
#endif


// -------------------------
#ifdef CONFIG_DE_TESTLAB
// use alternative loop
#define ALT_MAIN_LOOP
#define V0p2_REV 0
// Defaults for V0.2; have to be undefined if not required.  ***
// May require limiting clock speed and using some alternative peripherals/sensors.
#define ENABLE_SUPPLY_VOLTAGE_LOW_2AA
// Provide software RTC support by default.
#define ENABLE_RTC_INTERNAL_SIMPLE
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit *can* act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.  ***
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF DEFINED: allow minimal binary format in addition to more generic one: ~400 bytes code cost.
#undef ENABLE_MINIMAL_STATS_TXRX
// IF DEFINED: allow JSON stats frames alongside binary ones.
//#undef ENABLE_JSON_OUTPUT
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable. ***
#undef ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#define ENABLE_CLI
// IF DEFINED: enable a full OpenTRV CLI.
#define ENABLE_FULL_OT_CLI
// IF DEFINED: enable a full OpenTRV UI with normal LEDs etc. ***
//#define ENABLE_FULL_OT_UI
// IF DEFINED: enable and extended CLI with a longer input buffer for example.
#undef ENABLE_EXTENDED_CLI
// IF DEFINED: minimise boot effort and energy eg for intermittently-powered energy-harvesting applications.  ***
#undef ENABLE_MIN_ENERGY_BOOT
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).   ***
#undef ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// IF DEFINED: enable use AVR's 'idle' mode to stop the CPU but leave I/O (eg Serial) running to save power.
// DHD20150920: CURRENTLY NOT RECOMMENDED AS STILL SEEMS TO CAUSE SOME BOARDS TO CRASH.
#define ENABLE_USE_OF_AVR_IDLE_MODE
// IF DEFINED: Use OTNullRadioLink instead of a radio module
// Undefine other radio //FIXME make this a part of the automatic stuff
//#define USE_NULLRADIO
#define USE_MODULE_SIM900
// Define voice module
#define ENABLE_VOICE_SENSOR
// Enable use of OneWire devices.
#define ENABLE_MINIMAL_ONEWIRE_SUPPORT
// Enable use of DS18B20 temp sensor.
#define ENABLE_PRIMARY_TEMP_SENSOR_DS18B20

// things that break
// IF DEFINED: basic FROST/WARM temperatures are settable.
//#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.  ***
//#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1  UI_Minimal.cpp:1180:32: error: 'handleLEARN' was not declared in this scope
#define ENABLE_FHT8VSIMPLE //Control.cpp:1322:27: error: 'localFHT8VTRVEnabled' was not declared in this scope
// If LDR is not to be used then specifically define OMIT_... as below.
//#undef ENABLE_OCCUPANCY_DETECTION_FROM_AMBLIGHT //  LDR 'occupancy' sensing irrelevant for DHW. Messaging.cpp:232:87: error: 'class AmbientLight' has no member named 'getRaw

#endif // CONFIG_DE_TESTLAB


#endif

