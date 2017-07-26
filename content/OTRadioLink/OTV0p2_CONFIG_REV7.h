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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2017
                           Deniz Erbilgin 2015--2016
*/

/*
 * Set of ENABLE_XXX flags and V0p2_REV for REV7.
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

#ifndef ARDUINO_LIB_OTV0P2_CONFIG_REV7_H
#define ARDUINO_LIB_OTV0P2_CONFIG_REV7_H


// Generic REV7/DORM1 valve unit settings to be expanded for more specific implementations.
#if defined(CONFIG_DORM1) || defined(CONFIG_DORM1_SECURE_AND_FS20) // Generic all-in-one (REV7).
// GENERIC
// Revision REV7 of V0.2 board, all-in-one valve unit with local motor drive.
// Does not ever need to act as a boiler hub nor to receive stats.
// GENERIC
#define V0p2_REV 7
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
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
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
#endif

#ifdef CONFIG_DORM1 // Additional settings for all-in-one (REV7) secure only.
// Revision REV7 of V0.2 board, all-in-one valve unit with local motor drive.
// Does not ever need to act as a boiler hub nor to receive stats.
// Although LEARN buttons are provided, by default they are disabled as is the scheduler.
// Fast temp dial sampling is forced on to help compensate for mechanical slop in early devices.
// Delayed activation is enabled.  (TODO-786)
// As of 2016/11/06 no DORM1/TRV1 was using shaft encoder so simple on/off valve control is appropriate.  (TODO-1038)
// IF DEFINED: try to trim memory (primarily RAM, also code/Flash) space used.
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
#define ENABLE_FAST_FRAMED_CARRIER_SUPPORT
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
// DHD20160214: costs 5866 bytes to enable vs 3426 for FS20 support.
#define ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: always allow some kind of stats TX, whatever the privacy settings.
// OK IN THIS CASE BECAUSE ALL COMMS SECURE.
#define ENABLE_ALWAYS_TX_ALL_STATS
// IF DEFINED: allow setting of ID from CLI to replace devices in situ; not recommended by default to avoid confusion.
#define ENABLE_ID_SET_FROM_CLI
// IF DEFINED: enable a CLI-settable setback lockout (hours/days) to establish a baseline before engaging energy saving setbacks.
#define ENABLE_SETBACK_LOCKOUT_COUNTDOWN
#endif

#ifdef CONFIG_REV7_AS_SECURE_SENSOR // REV7 as JSON-only stats/sensor leaf.
#define V0p2_REV 7
// IF DEFINED: simplified mode button behaviour: tapping button invokes BAKE, not mode cycling.
#define ENABLE_SIMPLIFIED_MODE_BAKE
// IF DEFINED: fast temp pot/dial sampling to partly compensate for less good mechanics (at some energy cost).
#define ENABLE_FAST_TEMP_POT_SAMPLING
//// IF DEFINED: initial direct motor drive design.
//#define ENABLE_V1_DIRECT_MOTOR_DRIVE
// IF DEFINED: reverse DORM1 motor with respect to very first samples.
#define ENABLE_DORM1_MOTOR_REVERSED
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// IF DEFINED: detect occupancy based on relative humidity, if available.
// IF UNDEFINED: this unit cannot act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF UNDEFINED: do not allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow radio listen/RX.
#undef ENABLE_RADIO_RX
// IF DEFINED: allow JSON stats frames.
#define ENABLE_JSON_OUTPUT
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
#define ENABLE_FAST_FRAMED_CARRIER_SUPPORT
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
// DHD20160214: costs 5866 bytes to enable vs 3426 for FS20 support.
#define ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: always allow some kind of stats TX, whatever the privacy settings.
// OK IN THIS CASE BECAUSE ALL COMMS SECURE.
#define ENABLE_ALWAYS_TX_ALL_STATS
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
#endif

#ifdef CONFIG_REV7_AS_SENSOR // REV7 as JSON-only stats/sensor leaf.
// As DORM1 but without local valve control or settable temperatures.
#define CONFIG_DORM1
// IF DEFINED: enable periodic secure beacon broadcast.
#undef ENABLE_SECURE_RADIO_BEACON
// IF DEFINED: allow non-secure OpenTRV secure frame RX (as of 2015/12): DISABLED BY DEFAULT.
#define ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
// DHD20160214: costs 5866 bytes to enable vs 3426 for FS20 support.
#undef ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: use the temperature-setting potentiometer/dial if present.
#undef ENABLE_TEMP_POT_IF_PRESENT
// IF DEFINED: basic FROST/WARM temperatures are settable.
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
#endif

#ifdef CONFIG_DORM1_MUT // REV7 / DORM1 Winter 2014/2015 minimal for unit testing.
// Revision REV7 of V0.2 board, all-in-one valve unit with local motor drive.
// Does not ever need to act as a boiler hub nor to receive stats.
#define V0p2_REV 7
// IF DEFINED: initial direct motor drive design.
#undef ENABLE_V1_DIRECT_MOTOR_DRIVE
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// IF UNDEFINED: this unit cannot act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF UNDEFINED: do not allow TX of stats frames.
#undef ENABLE_STATS_TX
// IF UNDEFINED: do not allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow JSON stats frames.
#undef ENABLE_JSON_OUTPUT
// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#undef ENABLE_CLI
// IF DEFINED: enable a full OpenTRV CLI.
#undef ENABLE_FULL_OT_CLI
// IF DEFINED: enable a full OpenTRV UI with normal LEDs etc.
#undef ENABLE_FULL_OT_UI
// IF DEFINED: enable and extended CLI with a longer input buffer for example.
#undef ENABLE_EXTENDED_CLI
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#define ENABLE_LOCAL_TRV
#endif

#ifdef CONFIG_REV7_EEPROM_TEST // Config for isolating key loss.
// GENERIC
// Revision REV7 of V0.2 board, all-in-one valve unit with local motor drive.
// Does not ever need to act as a boiler hub nor to receive stats.
// GENERIC
#define V0p2_REV 7
// IF DEFINED: simplified mode button behaviour: tapping button invokes BAKE, not mode cycling.
//#define ENABLE_SIMPLIFIED_MODE_BAKE
#undef ENABLE_SIMPLIFIED_MODE_BAKE
// IF DEFINED: fast temp pot/dial sampling to partly compensate for less good mechanics (at some energy cost).
//#define ENABLE_FAST_TEMP_POT_SAMPLING
#undef ENABLE_FAST_TEMP_POT_SAMPLING
// IF DEFINED: initial direct motor drive design.
//#define ENABLE_V1_DIRECT_MOTOR_DRIVE
#undef ENABLE_V1_DIRECT_MOTOR_DRIVE
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
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
//#define ENABLE_LOCAL_TRV
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#define ENABLE_CLI
// IF DEFINED: there is run-time help available for the CLI.
#undef ENABLE_CLI_HELP
// IF DEFINED: enable a full OpenTRV CLI.
#define ENABLE_FULL_OT_CLI
// IF DEFINED: enable and extended CLI with a longer input buffer for example.
#undef ENABLE_EXTENDED_CLI
// IF DEFINED: enable a full OpenTRV UI with normal LEDs etc.
//#define ENABLE_FULL_OT_UI
#undef ENABLE_FULL_OT_UI
// IF DEFINED: enable use of second UI LED if available.
#undef ENABLE_UI_LED_2_IF_AVAILABLE
// IF DEFINED: reverse DORM1 motor with respect to very first samples.
//#define ENABLE_DORM1_MOTOR_REVERSED
#undef ENABLE_DORM1_MOTOR_REVERSED

// Revision REV7 of V0.2 board, all-in-one valve unit with local motor drive.
// Does not ever need to act as a boiler hub nor to receive stats.
// Although LEARN buttons are provided, by default they are disabled as is the scheduler.
// Fast temp dial sampling is forced on to help compensate for mechanical slop in early devices.
// Delayed activation is enabled.  (TODO-786)
// IF DEFINED: try to trim memory (primarily RAM, also code/Flash) space used.
//#define ENABLE_TRIMMED_MEMORY
#undef ENABLE_TRIMMED_MEMORY
// IF DEFINED: try to trim bandwidth as may be especially expensive/scarce.
#undef ENABLE_TRIMMED_BANDWIDTH
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
#define ENABLE_FAST_FRAMED_CARRIER_SUPPORT
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
// DHD20160214: costs 5866 bytes to enable vs 3426 for FS20 support.
#define ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: always allow some kind of stats TX, whatever the privacy settings.
// OK IN THIS CASE BECAUSE ALL COMMS SECURE.
#define ENABLE_ALWAYS_TX_ALL_STATS
// IF DEFINED: enable a CLI-settable setback lockout (hours/days) to establish a baseline before engaging energy saving setbacks.
//#define ENABLE_SETBACK_LOCKOUT_COUNTDOWN
#undef ENABLE_SETBACK_LOCKOUT_COUNTDOWN

#endif


#ifdef CONFIG_REV7_2WAY // REV9 cut2, derived from REV4.
// Revision of V0.2 board.
#define V0p2_REV 7
// IF DEFINED: use (slow, low energy) 32768Hz-clock-based watchdog to recover from some software hangups.
#define ENABLE_WATCHDOG_SLOW
// Enable use of OneWire devices.
#undef ENABLE_MINIMAL_ONEWIRE_SUPPORT
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// IF DEFINED: enable use of additional (eg external) DS18B20 temp sensor(s).
#undef ENABLE_EXTERNAL_TEMP_SENSOR_DS18B20
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
#define ENABLE_STATS_TX
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#define ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: allow JSON stats frames alongside binary ones.
#define ENABLE_JSON_OUTPUT    // XXX
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#define ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
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
#define ENABLE_FHT8VSIMPLE  // XXX
// IF DEFINED: enable support for FS20 carrier for RX or TX.
#define ENABLE_FS20_CARRIER_SUPPORT  // XXX
// IF DEFINED: enable support for FS20 encoding/decoding, eg to send to FHT8V.
#define ENABLE_FS20_ENCODING_SUPPORT  // XXX
// IF DEFINED: act as CC1 simple relay node.
#define ALLOW_CC1_SUPPORT
#define ALLOW_CC1_SUPPORT_RELAY
#define ALLOW_CC1_SUPPORT_RELAY_IO // Direct addressing of LEDs, use of buttons, etc.
#endif

#endif

