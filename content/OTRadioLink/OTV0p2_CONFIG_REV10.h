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
 * Set of ENABLE_XXX flags and V0p2_REV for REV10.
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

#ifndef ARDUINO_LIB_OTV0P2_CONFIG_REV10_H
#define ARDUINO_LIB_OTV0P2_CONFIG_REV10_H


#ifdef CONFIG_REV10 // REV10 base config
#define V0p2_REV 10
// IF DEFINED: support one on and one off time per day (possibly in conjunction with 'learn' button).
#undef ENABLE_SINGLETON_SCHEDULE
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.
#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
// IF DEFINED: support for general timed and multi-input occupancy detection / use.
#undef ENABLE_OCCUPANCY_SUPPORT
// IF DEFINED: try to trim memory (primarily RAM, also code/Flash) space used.
#define ENABLE_TRIMMED_MEMORY
// IF DEFINED: try to trim bandwidth as may be especially expensive/scarce.
#undef ENABLE_TRIMMED_BANDWIDTH
// IF DEFINED: allow periodic machine- and human- readable status report to serial, starting with "=".
#undef ENABLE_SERIAL_STATUS_REPORT
// IF DEFINED: allow use of ambient light sensor.
#undef ENABLE_AMBLIGHT_SENSOR
// IF DEFINED: allow for less light on sideways-pointing ambient light sensor, eg on cut4 2014/03/17 REV2 boards (TODO-209).
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// IF DEFINED: basic FROST/WARM temperatures are settable.
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
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
#define ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED
// IF DEFINED: enable support for fast (>50kbps) packet-handling carrier (leading length byte).
#define ENABLE_FAST_FRAMED_CARRIER_SUPPORT
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
// DHD20160214: costs 5866 bytes to enable vs 3426 for FS20 support.
#undef ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: enable support for FS20 carrier for RX of raw FS20 and piggybacked binary (non-JSON) stats.
#undef ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX
// IF DEFINED: allow RX of stats frames.
#define ENABLE_STATS_RX
// IF DEFINED: allow TX of local stats frames (should not affect relaying).
#define ENABLE_STATS_TX
// IF DEFINED: allow radio listen/RX.
#define ENABLE_RADIO_RX
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#define ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: allow local generation of JSON stats frames (this may not affect relaying).
#define ENABLE_JSON_OUTPUT
/// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#define ENABLE_CLI
// IF DEFINED: there is run-time help available for the CLI.
#undef ENABLE_CLI_HELP
// IF DEFINED: enable a full OpenTRV CLI.
#define ENABLE_FULL_OT_CLI
// IF DEFINED: enable and extended CLI with a longer input buffer for example.
#undef ENABLE_EXTENDED_CLI
// IF DEFINED: enable a full OpenTRV UI with normal LEDs etc.
#undef ENABLE_FULL_OT_UI
// IF DEFINED: provide CLI read/write access to generic parameter block.
#undef ENABLE_GENERIC_PARAM_CLI_ACCESS
// IF DEFINED: enable a secondary (typically WAN-relay) radio module.
#define ENABLE_RADIO_SECONDARY_MODULE
// IF DEFINED: enable a WAN-relay radio module, primarily to relay stats outbound.
#define ENABLE_RADIO_SECONDARY_MODULE_AS_RELAY
// SIM900 relay.
#define ENABLE_RADIO_SIM900   // Enable SIM900
#define ENABLE_RADIO_SECONDARY_SIM900  // Assign SIM900
// IF DEFINED: always allow some kind of stats TX, whatever the privacy settings.
// HAS HUGE PRIVACY IMPLICATIONS: DO NOT ENABLE UNNECESSARILY!
// DHD20160214: makes sense to ensure that the relay unit is always talkative, at least initially.
#define ENABLE_ALWAYS_TX_ALL_STATS
#endif // CONFIG_REV10

#ifdef CONFIG_REV10_STRIPBOARD // REV10-based stripboard precursor for bus shelters
#define V0p2_REV 10
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit *can* act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.  ***
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable. ***
#undef ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF DEFINED: allow minimal binary format in addition to more generic one: ~400 bytes code cost.
#undef ENABLE_MINIMAL_STATS_TXRX
// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#define ENABLE_CLI
// IF DEFINED: enable a full OpenTRV CLI.
#define ENABLE_FULL_OT_CLI
// IF DEFINED: enable a full OpenTRV UI with normal LEDs etc. ***
#undef ENABLE_FULL_OT_UI
// IF DEFINED: enable and extended CLI with a longer input buffer for example.
#undef ENABLE_EXTENDED_CLI
// IF DEFINED: minimise boot effort and energy eg for intermittently-powered energy-harvesting applications.  ***
#undef ENABLE_MIN_ENERGY_BOOT
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).   ***
#undef ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// IF DEFINED: enable use AVR's 'idle' mode to stop the CPU but leave I/O (eg Serial) running to save power.
// DHD20150920: CURRENTLY NOT RECOMMENDED AS STILL SEEMS TO CAUSE SOME BOARDS TO CRASH.
#undef ENABLE_USE_OF_AVR_IDLE_MODE
// IF DEFINED: support for general timed and multi-input occupancy detection / use.
#define ENABLE_OCCUPANCY_SUPPORT
// IF DEFINED: enable a 'null' radio module; without this unit is stand-alone.
#define ENABLE_RADIO_NULL
// Secondary radio
#undef ENABLE_RADIO_RFM23B
#undef ENABLE_RADIO_PRIMARY_RFM23B
// IF DEFINED: enable a secondary (typically WAN-relay) radio module.
#define ENABLE_RADIO_SECONDARY_MODULE
#define ENABLE_RADIO_SIM900   // Enable SIM900
//#define RADIO_PRIMARY_SIM900  // Assign SIM900
#define ENABLE_RADIO_SECONDARY_SIM900  // Assign SIM900
// Enable use of OneWire devices.
#define ENABLE_MINIMAL_ONEWIRE_SUPPORT
// Enable use of DS18B20 temp sensor.
#define ENABLE_PRIMARY_TEMP_SENSOR_DS18B20
// Define voice module
#define ENABLE_VOICE_SENSOR
#define ENABLE_OCCUPANCY_DETECTION_FROM_VOICE
#define ENABLE_VOICE_STATS
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.  ***
#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1  UI_Minimal.cpp:1180:32: error: 'handleLEARN' was not declared in this scope
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
#endif // CONFIG_REV10_STRIPBOARD

#ifdef CONFIG_REV10_AS_GSM_RELAY_ONLY // REV10: stats relay.
#define V0p2_REV 10
// IF DEFINED: support one on and one off time per day (possibly in conjunction with 'learn' button).
#undef ENABLE_SINGLETON_SCHEDULE
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.
#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
// IF DEFINED: support for general timed and multi-input occupancy detection / use.
#undef ENABLE_OCCUPANCY_SUPPORT
// IF DEFINED: try to trim memory (primarily RAM, also code/Flash) space used.
#define ENABLE_TRIMMED_MEMORY
// IF DEFINED: try to trim bandwidth as may be especially expensive/scarce.
#undef ENABLE_TRIMMED_BANDWIDTH
// IF DEFINED: allow periodic machine- and human- readable status report to serial, starting with "=".
#undef ENABLE_SERIAL_STATUS_REPORT
// IF DEFINED: allow use of ambient light sensor.
#undef ENABLE_AMBLIGHT_SENSOR
// IF DEFINED: allow for less light on sideways-pointing ambient light sensor, eg on cut4 2014/03/17 REV2 boards (TODO-209).
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// IF DEFINED: basic FROST/WARM temperatures are settable.
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
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
// DE20160411:  costs ~10KB on both Arduino 1.6.5 and 1.6.7 for me. (just over 32KB down to about 22 KB)
#define ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: enable support for FS20 carrier for RX of raw FS20 and piggybacked binary (non-JSON) stats.
#undef ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX
// IF DEFINED: allow RX of stats frames.
#define ENABLE_STATS_RX
// IF DEFINED: allow TX of local stats frames (should not affect relaying).
#define ENABLE_STATS_TX
// IF DEFINED: allow radio listen/RX.
#define ENABLE_RADIO_RX
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#define ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: allow local generation of JSON stats frames (this may not affect relaying).
#define ENABLE_JSON_OUTPUT
/// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#define ENABLE_CLI
// IF DEFINED: there is run-time help available for the CLI.
#undef ENABLE_CLI_HELP
// IF DEFINED: enable a full OpenTRV CLI.
#define ENABLE_FULL_OT_CLI
// IF DEFINED: enable and extended CLI with a longer input buffer for example.
#undef ENABLE_EXTENDED_CLI
// IF DEFINED: enable a full OpenTRV UI with normal LEDs etc.
#undef ENABLE_FULL_OT_UI
// IF DEFINED: provide CLI read/write access to generic parameter block.
#undef ENABLE_GENERIC_PARAM_CLI_ACCESS
// IF DEFINED: enable a secondary (typically WAN-relay) radio module.
#define ENABLE_RADIO_SECONDARY_MODULE
// IF DEFINED: enable a WAN-relay radio module, primarily to relay stats outbound.
#define ENABLE_RADIO_SECONDARY_MODULE_AS_RELAY
// SIM900 relay.
#define ENABLE_RADIO_SIM900   // Enable SIM900
#define ENABLE_RADIO_SECONDARY_SIM900  // Assign SIM900
// IF DEFINED: always allow some kind of stats TX, whatever the privacy settings.
// HAS HUGE PRIVACY IMPLICATIONS: DO NOT ENABLE UNNECESSARILY!
// DHD20160214: makes sense to ensure that the relay unit is always talkative, at least initially.
#define ENABLE_ALWAYS_TX_ALL_STATS
#endif // CONFIG_REV10_AS_GSM_RELAY_ONLY

#ifdef CONFIG_REV10_BHR // REV10: boiler hub and stats relay.
#define V0p2_REV 10
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// IF DEFINED: basic FROST/WARM temperatures are settable.
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#define ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#define ENABLE_BOILER_HUB
// IF DEFINED: allow binary stats to be TXed.
#undef ENABLE_BINARY_STATS_TX
// IF DEFINED: enable support for FS20 carrier for RX of raw FS20 and piggybacked binary (non-JSON) stats.
#undef ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX
// IF DEFINED: allow RX of stats frames.
#define ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.
#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#define ENABLE_CLI
// IF DEFINED: support for general timed and multi-input occupancy detection / use.
#undef ENABLE_OCCUPANCY_SUPPORT
// IF DEFINED: enable a secondary (typically WAN-relay) radio module.
#define ENABLE_RADIO_SECONDARY_MODULE
// IF DEFINED: enable a WAN-relay radio module, primarily to relay stats outbound.
#define ENABLE_RADIO_SECONDARY_MODULE_AS_RELAY
// SIM900 relay.
#define ENABLE_RADIO_SIM900   // Enable SIM900
#define ENABLE_RADIO_SECONDARY_SIM900  // Assign SIM900
#endif


#endif

