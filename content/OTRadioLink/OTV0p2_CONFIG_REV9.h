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
 * Set of ENABLE_XXX flags and V0p2_REV for REV9.
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

#ifndef ARDUINO_LIB_OTV0P2_CONFIG_REV9_H
#define ARDUINO_LIB_OTV0P2_CONFIG_REV9_H


// COHEAT radio relay.

#ifdef CONFIG_REV9_cut1
#define V0p2_REV 9 // Just like cut2 but with some bugs...
// For 1st-cut REV9 boards phototransistor was accidentally pulling down not up.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400_WRONG_WAY
#endif

#ifdef CONFIG_REV9_SECURE
// Secure COHEAT REV9 relay just like normal one except ...
#define CONFIG_REV9
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
#define ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
#endif

#ifdef CONFIG_REV9 // REV9 cut2, derived from REV4.
// Revision of V0.2 board.
#define V0p2_REV 9
// Enable use of OneWire devices.
#define ENABLE_MINIMAL_ONEWIRE_SUPPORT
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// IF DEFINED: enable use of additional (eg external) DS18B20 temp sensor(s).
#define ENABLE_EXTERNAL_TEMP_SENSOR_DS18B20
// IF DEFINED: allow use of ambient light sensor.
#define ENABLE_AMBLIGHT_SENSOR
// IF DEFINED: allow for less light on sideways-pointing ambient light sensor, eg on cut4 2014/03/17 REV2 boards (TODO-209).
#undef ENABLE_AMBLIGHT_EXTRA_SENSITIVE
// IF DEFINED: use RoHS-compliant phototransistor in place of default LDR.
#undef ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// IF UNDEFINED: this unit cannot act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#undef ENABLE_STATS_TX
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#define ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: allow JSON stats frames alongside binary ones.
#undef ENABLE_JSON_OUTPUT
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#define ENABLE_SLAVE_TRV
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#define ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: enable a full OpenTRV CLI.
#undef ENABLE_FULL_OT_CLI
// IF DEFINED: enable a full OpenTRV UI with normal LEDs etc.
#undef ENABLE_FULL_OT_UI
// IF DEFINED: basic FROST/WARM temperatures are settable.
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#define ENABLE_CLI
// IF DEFINED: enable and extended CLI with a longer input buffer for example.
#define ENABLE_EXTENDED_CLI
// IF DEFINED: support for general timed and multi-input occupancy detection / use.
#undef ENABLE_OCCUPANCY_SUPPORT // No direct occupancy tracking at relay unit itself.
// IF DEFINED: for REV9 boards window sensor(s).
#define ENABLE_LEARN_BUTTON
// IF DEFINED: use FHT8V wireless radio module/valve, eg to control FHT8V local valve.
#define ENABLE_FHT8VSIMPLE
// IF DEFINED: enable support for FS20 carrier for RX or TX.
#define ENABLE_FS20_CARRIER_SUPPORT
// IF DEFINED: enable support for FS20 encoding/decoding, eg to send to FHT8V.
#define ENABLE_FS20_ENCODING_SUPPORT
// IF DEFINED: act as CC1 simple relay node.
#define ALLOW_CC1_SUPPORT
#define ALLOW_CC1_SUPPORT_RELAY
#define ALLOW_CC1_SUPPORT_RELAY_IO // Direct addressing of LEDs, use of buttons, etc.
#endif

#ifdef CONFIG_REV9_STATS // REV9 cut2, derived from REV4, as stats node, for testing.
#define V0p2_REV 9
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF DEFINED: allow JSON stats frames alongside binary ones.
#define ENABLE_JSON_OUTPUT
// Anticipation logic not yet ready for prime-time.
//#define ENABLE_ANTICIPATION
// IF UNDEFINED: this unit cannot act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
//#undef ENABLE_BOILER_HUB
#endif


#endif

