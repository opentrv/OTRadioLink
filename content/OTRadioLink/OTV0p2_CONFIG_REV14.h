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
 * Set of ENABLE_XXX flags and V0p2_REV for REV14.
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

#ifndef ARDUINO_LIB_OTV0P2_CONFIG_REV14_H
#define ARDUINO_LIB_OTV0P2_CONFIG_REV14_H


#ifdef CONFIG_REV14_PROTO // Prototype REV14 w/ LoRa, TMP, SHT and QM-1

#define V0p2_REV 14
// IF DEFINED: basic FROST/WARM temperatures are settable.
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.  ***
#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
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
// IF DEFINED: allow binary stats to be TXed.
#undef ENABLE_BINARY_STATS_TX
// IF DEFINED: enable support for FS20 carrier for RX of raw FS20 and piggybacked binary (non-JSON) stats.
#undef ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX
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
//#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// IF DEFINED: enable use AVR's 'idle' mode to stop the CPU but leave I/O (eg Serial) running to save power.
// DHD20150920: CURRENTLY NOT RECOMMENDED AS STILL SEEMS TO CAUSE SOME BOARDS TO CRASH.
#undef ENABLE_USE_OF_AVR_IDLE_MODE
// IF DEFINED: enable a 'null' radio module; without this unit is stand-alone.
#define ENABLE_RADIO_NULL
// Secondary radio
#undef ENABLE_RADIO_RFM23B
#undef ENABLE_RADIO_PRIMARY_RFM23B
// IF DEFINED: enable a secondary (typically WAN-relay) radio module.
#define ENABLE_RADIO_SECONDARY_MODULE
#define ENABLE_RADIO_RN2483   // Enable RN2483
//#define RADIO_PRIMARY_RN2483 // Must be secondary to avoid sending preamble etc
#define ENABLE_RADIO_SECONDARY_RN2483
// Define voice module
#define ENABLE_VOICE_SENSOR
#define ENABLE_OCCUPANCY_DETECTION_FROM_VOICE
#define ENABLE_VOICE_STATS
#endif // CONFIG_REV14_PROTO

#ifdef CONFIG_REV14 // REV14 w/ light sensor, SHT21 and voice sensor.
// Measured current consumption (no QM-1 or mobdet): 100-200 uA when serial shut and not attempting TX.
// Revision REV14 of V0.2 board, sensor unit with LoRa module and voice detector.
// In this off-label mode being used as stats gatherers or simple hubs.
// DE20160221: I may have made a mistake with my oscilloscope setup doing the earlier readings. Revised values below:
// - QM-1 turned off, RN2483 in sleep mode, no mobdet:                  2-3 mA
// - QM-1 turned on but not capturing, RN2483 in sleep mode, no mobdet: ~10 mA
#define V0p2_REV 14
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// IF DEFINED: use the temperature-setting potentiometer/dial if present.
#undef ENABLE_TEMP_POT_IF_PRESENT
// IF DEFINED: basic FROST/WARM temperatures are settable.
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB // NO BOILER CODE
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF DEFINED: always allow some kind of stats TX, whatever the privacy settings.
#define ENABLE_ALWAYS_TX_ALL_STATS
// IF DEFINED: allow JSON stats frames.
#define ENABLE_JSON_OUTPUT
// IF DEFINED: allow binary stats to be TXed.
#undef ENABLE_BINARY_STATS_TX
// IF DEFINED: enable support for FS20 carrier for RX of raw FS20 and piggybacked binary (non-JSON) stats.
#undef ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX
//#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// IF DEFINED: enable use AVR's 'idle' mode to stop the CPU but leave I/O (eg Serial) running to save power.
// DHD20150920: CURRENTLY NOT RECOMMENDED AS STILL SEEMS TO CAUSE SOME BOARDS TO CRASH.
#define ENABLE_USE_OF_AVR_IDLE_MODE
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.  ***
#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
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
// IF DEFINED: enable a 'null' radio module; without this unit is stand-alone.
#define ENABLE_RADIO_NULL
// Secondary radio
#undef ENABLE_RADIO_RFM23B
#undef ENABLE_RADIO_PRIMARY_RFM23B
// IF DEFINED: enable a secondary (typically WAN-relay) radio module.
#define ENABLE_RADIO_SECONDARY_MODULE
#define ENABLE_RADIO_RN2483   // Enable RN2483
//#define RADIO_PRIMARY_RN2483 // Must be secondary to avoid sending preamble etc
#define ENABLE_RADIO_SECONDARY_RN2483
//#define ENABLE_FREQUENT_STATS_TX
#define ENABLE_JSON_STATS_LEN_CAP 49
// Define voice module
//#define ENABLE_VOICE_SENSOR
//#define ENABLE_OCCUPANCY_DETECTION_FROM_VOICE
//#define ENABLE_VOICE_STATS
#endif

#ifdef CONFIG_REV14_WORKSHOP // REV14 w/ light sensor and SHT21 for Launchpad final workshop.
// Measured current consumption (no QM-1 or mobdet): 100-200 uA when serial shut and not attempting TX.
// Revision REV14 of V0.2 board, sensor unit with LoRa module and voice detector.
// In this off-label mode being used as stats gatherers or simple hubs.
// DE20160221: I may have made a mistake with my oscilloscope setup doing the earlier readings. Revised values below:
// - QM-1 turned off, RN2483 in sleep mode, no mobdet:                  2-3 mA
// - QM-1 turned on but not capturing, RN2483 in sleep mode, no mobdet: ~10 mA
#define V0p2_REV 14
// IF DEFINED: enable use of on-board SHT21 RH and temp sensor (in lieu of TMP112).
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// Using RoHS-compliant phototransistor in place of LDR.
#define ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
// IF DEFINED: use the temperature-setting potentiometer/dial if present.
#undef ENABLE_TEMP_POT_IF_PRESENT
// IF DEFINED: basic FROST/WARM temperatures are settable.
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB // NO BOILER CODE
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF DEFINED: always allow some kind of stats TX, whatever the privacy settings.
#define ENABLE_ALWAYS_TX_ALL_STATS
// IF DEFINED: allow JSON stats frames.
#define ENABLE_JSON_OUTPUT
// IF DEFINED: allow binary stats to be TXed.
#undef ENABLE_BINARY_STATS_TX
// IF DEFINED: enable support for FS20 carrier for RX of raw FS20 and piggybacked binary (non-JSON) stats.
#undef ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX
// IF DEFINED: enable use of on-board SHT21 primary temperature and RH% sensor (in lieu of default TMP112).
#define ENABLE_PRIMARY_TEMP_SENSOR_SHT21
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.  ***
#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
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
// IF DEFINED: enable a 'null' radio module; without this unit is stand-alone.
#define ENABLE_RADIO_NULL
// Secondary radio
#undef ENABLE_RADIO_RFM23B
#undef ENABLE_RADIO_PRIMARY_RFM23B
// IF DEFINED: enable a secondary (typically WAN-relay) radio module.
#define ENABLE_RADIO_SECONDARY_MODULE
#define ENABLE_RADIO_RN2483   // Enable RN2483
//#define RADIO_PRIMARY_RN2483 // Must be secondary to avoid sending preamble etc
#define ENABLE_RADIO_SECONDARY_RN2483
//#define ENABLE_FREQUENT_STATS_TX
// IF DEFINED: try to trim bandwidth as may be especially expensive/scarce.
#define ENABLE_TRIMMED_BANDWIDTH
// IF DEFINED: the (>>8) value of this flag is the maximum JSON frame size allowed (bytes).
#define ENABLE_JSON_STATS_LEN_CAP 34 // Should allow LoRa SF12 and 4 min TX intervals.
// IF DEFINED: unconditionally suppress the "@" ID field (carrier supplies it or equiv) to save bandwidth.
#define ENABLE_JSON_SUPPRESSED_ID
// IF DEFINED: unconditionally suppress the "+" ID field and aim for minimum JSON frame size, for poor/noisy comms channels.
// NOTE: minimising the JSON frame with this will overall *reduce* bandwidth efficiency and ability to diagnose TX problems.
#define ENABLE_JSON_FRAME_MINIMISED
#endif

#endif

