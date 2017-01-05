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

Author(s) / Copyright (s): Deniz Erbilgin 2016-2017
*/

/*
 * RESERVED CONFIG FOR TRV2
 *
 * Set of ENABLE_XXX flags and V0p2_REV for REV20.
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

#ifndef ARDUINO_LIB_OTV0P2_CONFIG_REV20_H
#define ARDUINO_LIB_OTV0P2_CONFIG_REV20_H

#if defined(CONFIG_TRV20_PROTO) // Generic all-in-one (REV7).
// GENERIC
// Revision REV20 of V0.2 board, all-in-one valve unit with local motor drive.
// Has no onboarfd radio.
// GENERIC
#define V0p2_REV 20
// IF DEFINED: simplified mode button behaviour: tapping button invokes BAKE, not mode cycling.
#define ENABLE_SIMPLIFIED_MODE_BAKE
// IF DEFINED: fast temp pot/dial sampling to partly compensate for less good mechanics (at some energy cost).
#define ENABLE_FAST_TEMP_POT_SAMPLING
// IF DEFINED: basic FROST/WARM temperatures are settable and stored in EEPROM.
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: support one on and one off time per day (possibly in conjunction with 'learn' button).
#undef ENABLE_SINGLETON_SCHEDULE
//// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.
//#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
//// IF DEFINED: try to trim memory (primarily RAM, also code/Flash) space used.
//#define ENABLE_TRIMMED_MEMORY
//// IF DEFINED: try to trim bandwidth as may be especially expensive/scarce.
//#define ENABLE_TRIMMED_BANDWIDTH
// IF DEFINED: initial direct motor drive design.
#define ENABLE_V1_DIRECT_MOTOR_DRIVE
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
#undef ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400  // FIXME Has alternative light sensor.
// IF DEFINED: detect occupancy based on relative humidity, if available.
// DHD20160101: seems to still be set off spuriously by fast drop in temp when rad turns off (TODO-696).
#undef ENABLE_OCCUPANCY_DETECTION_FROM_RH
// IF UNDEFINED: this unit cannot act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF UNDEFINED: do not allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow radio listen/RX.
#undef ENABLE_RADIO_RX
// IF DEFINED: forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#undef ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: allow JSON stats frames.
#define ENABLE_JSON_OUTPUT
// IF DEFINED: allow binary stats to be TXed.
#undef ENABLE_BINARY_STATS_TX
// IF DEFINED: enable support for FS20 carrier for RX of raw FS20 and piggybacked binary (non-JSON) stats.
#undef ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#define ENABLE_LOCAL_TRV
// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#define ENABLE_CLI
// IF DEFINED: there is run-time help available for the CLI.
#undef ENABLE_CLI_HELP
// IF DEFINED: enable a full OpenTRV CLI.
#define ENABLE_FULL_OT_CLI
// IF DEFINED: enable and extended CLI with a longer input buffer for example.
#undef ENABLE_EXTENDED_CLI
// IF DEFINED: enable a full OpenTRV UI with normal LEDs etc.
#define ENABLE_FULL_OT_UI
// IF DEFINED: enable use of second UI LED if available.
#undef ENABLE_UI_LED_2_IF_AVAILABLE
// IF DEFINED: reverse DORM1 motor with respect to very first samples.
#define ENABLE_DORM1_MOTOR_REVERSED


#define ENABLE_TRIMMED_MEMORY
// IF DEFINED: try to trim bandwidth as may be especially expensive/scarce.
#undef ENABLE_TRIMMED_BANDWIDTH
// If DEFINED: attempt proportional (rather than cruder, eg, on/off, control of TRV or other heat source).
#define ENABLE_PROPORTIONAL_VALVE_CONTROL
// IF DEFINED: support one on and one off time per day (possibly in conjunction with 'learn' button).
#undef ENABLE_SINGLETON_SCHEDULE
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.
#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
// IF DEFINED: allow periodic machine- and human- readable status report to serial, starting with "=".
#undef ENABLE_SERIAL_STATUS_REPORT
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
// IF DEFINED: enable periodic secure beacon broadcast.
#undef ENABLE_SECURE_RADIO_BEACON
// IF DEFINED: allow non-secure OpenTRV secure frame RX (as of 2015/12): DISABLED BY DEFAULT.
#undef ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED
// IF DEFINED: enable support for fast (>50kbps) packet-handling carrier (leading length byte).
#undef ENABLE_FAST_FRAMED_CARRIER_SUPPORT
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
// DHD20160214: costs 5866 bytes to enable vs 3426 for FS20 support.
#undef ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: always allow some kind of stats TX, whatever the privacy settings.
// OK IN THIS CASE BECAUSE ALL COMMS SECURE.
#define ENABLE_ALWAYS_TX_ALL_STATS
// IF DEFINED: allow setting of ID from CLI to replace devices in situ; not recommended by default to avoid confusion.
#define ENABLE_ID_SET_FROM_CLI
// IF DEFINED: enable a CLI-settable setback lockout (hours/days) to establish a baseline before engaging energy saving setbacks.
#define ENABLE_SETBACK_LOCKOUT_COUNTDOWN
#endif


#endif

