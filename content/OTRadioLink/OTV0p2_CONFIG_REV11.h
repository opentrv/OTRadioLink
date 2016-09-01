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
 * Set of ENABLE_XXX flags and V0p2_REV for REV11.
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

#ifndef ARDUINO_LIB_OTV0P2_CONFIG_REV11_H
#define ARDUINO_LIB_OTV0P2_CONFIG_REV11_H

#if defined(CONFIG_REV11_SENSOR) || defined(CONFIG_REV11_SECURE_SENSOR) || defined(CONFIG_REV11_SECURE_STATSHUB) || defined(CONFIG_REV11_STATSHUB)
// GENERIC
// Revision REV11 of V0.2 board, Generic sensor leaf/stats hub.
// This is a base config upon which to build other configs.
// Behaves as a secure sensor leaf by default?
// GENERIC
#define V0p2_REV 11
// Onboard Hardware Config:
// IF DEFINED: use the temperature-setting potentiometer/dial if present.
#undef ENABLE_TEMP_POT_IF_PRESENT
// IF DEFINED: fast temp pot/dial sampling to partly compensate for less good mechanics (at some energy cost).
#undef ENABLE_FAST_TEMP_POT_SAMPLING
// IF DEFINED: initial direct motor drive design.
#undef ENABLE_V1_DIRECT_MOTOR_DRIVE
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
// IF UNDEFINED: this unit cannot act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.
#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
// IF DEFINED: enable use of second UI LED if available.
#undef ENABLE_UI_LED_2_IF_AVAILABLE
// Radio Config
// IF DEFINED: forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#undef ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: enable support for FS20 carrier for RX of raw FS20 and piggybacked binary (non-JSON) stats.
#undef ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX
// IF DEFINED: allow binary stats to be TXed.
#undef ENABLE_BINARY_STATS_TX
// IF DEFINED: enable support for FS20 carrier for RX or TX.
#undef ENABLE_FS20_CARRIER_SUPPORT
// IF DEFINED: use FHT8V wireless radio module/valve.
#undef ENABLE_FHT8VSIMPLE
// IF DEFINED: enable support for FS20 carrier for TX specifically (to allow RX-only).
#undef ENABLE_FS20_CARRIER_SUPPORT_TX
// IF DEFINED: enable raw preamble injection/framing eg for FS20 over RFM23B.
#undef ENABLE_RFM23B_FS20_RAW_PREAMBLE
// IF DEFINED: enable support for FS20 encoding/decoding, eg to send to FHT8V.
#undef ENABLE_FS20_ENCODING_SUPPORT
// IF DEFINED: enable support for fast (>50kbps) packet-handling carrier (leading length byte).
#define ENABLE_FAST_FRAMED_CARRIER_SUPPORT
// Other Config
// IF DEFINED: simplified mode button behaviour: tapping button invokes BAKE, not mode cycling.
#undef ENABLE_SIMPLIFIED_MODE_BAKE
// IF DEFINED: detect occupancy based on relative humidity, if available.
// DHD20160101: seems to still be set off spuriously by fast drop in temp when rad turns off (TODO-696).
#undef ENABLE_OCCUPANCY_DETECTION_FROM_RH
// IF DEFINED: enable a full OpenTRV UI with normal LEDs etc.
#undef ENABLE_FULL_OT_UI
// IF DEFINED: try to trim memory (primarily RAM, also code/Flash) space used.
#define ENABLE_TRIMMED_MEMORY
// IF DEFINED: support one on and one off time per day (possibly in conjunction with 'learn' button).
#undef ENABLE_SINGLETON_SCHEDULE
// IF DEFINED: allow periodic machine- and human- readable status report to serial, starting with "=".
//#undef ENABLE_SERIAL_STATUS_REPORT
// IF DEFINED: enable a CLI-settable setback lockout (hours/days) to establish a baseline before engaging energy saving setbacks.
#undef ENABLE_SETBACK_LOCKOUT_COUNTDOWN
// IF DEFINED: basic FROST/WARM temperatures are settable.
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
#endif //  CONFIG_REV11_SENSOR

// REV4 (ie SHT21 sensor and phototransistor) + PCB antenna + PCB battery pack (probably AAA), see TODO-566
#ifdef CONFIG_REV11_RFM23BTEST
// Revision of V0.2 board.
#define V0p2_REV 11 // REV11 covers first sensor only board.
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// Anticipation logic not yet ready for prime-time.
//#define ENABLE_ANTICIPATION
// IF UNDEFINED: this unit cannot act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
//#undef ENABLE_BOILER_HUB
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
#undef ENABLE_LOCAL_TRV
#endif // CONFIG_REV11_RFM23BTEST

#ifdef CONFIG_REV11_SECURE_SENSOR // REV11 as raw JSON-only stats/sensor leaf.
// IF DEFINED: allow non-secure OpenTRV secure frame RX (as of 2015/12): DISABLED BY DEFAULT.
#undef ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
// DHD20160214: costs 5866 bytes to enable vs 3426 for FS20 support.
#define ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: always allow some kind of stats TX, whatever the privacy settings.
// OK IN THIS CASE BECAUSE ALL COMMS SECURE.
#define ENABLE_ALWAYS_TX_ALL_STATS
// IF DEFINED: allow radio listen/RX.
#undef ENABLE_RADIO_RX
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
#endif

#ifdef CONFIG_REV11_SENSOR // REV11 as raw JSON-only stats/sensor leaf.
// IF DEFINED: allow non-secure OpenTRV secure frame RX (as of 2015/12): DISABLED BY DEFAULT.
#define ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
// DHD20160214: costs 5866 bytes to enable vs 3426 for FS20 support.
#define ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: always allow some kind of stats TX, whatever the privacy settings.
#define ENABLE_ALWAYS_TX_ALL_STATS
// IF DEFINED: allow radio listen/RX.
#undef ENABLE_RADIO_RX
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
#endif

#ifdef CONFIG_REV11_SECURE_STATSHUB // REV11 stats hub with authentication
// IF DEFINED: allow radio listen/RX.
#define ENABLE_RADIO_RX
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#define ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: allow RX of stats frames.
#define ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#undef ENABLE_STATS_TX
// IF DEFINED: allow JSON stats frames alongside binary ones.
#undef ENABLE_JSON_OUTPUT
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
#define ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: allow non-secure OpenTRV secure frame RX (as of 2015/12): DISABLED BY DEFAULT.
#undef ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED
#endif

#ifdef CONFIG_REV11_STATSHUB // REV11 insecure stats hub
// IF DEFINED: allow radio listen/RX.
#define ENABLE_RADIO_RX
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#define ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: allow RX of stats frames.
#define ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#undef ENABLE_STATS_TX
// IF DEFINED: allow JSON stats frames alongside binary ones.
//#undef ENABLE_JSON_OUTPUT
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
#undef ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: allow non-secure OpenTRV secure frame RX (as of 2015/12): DISABLED BY DEFAULT.
#define ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED
#endif

#endif

