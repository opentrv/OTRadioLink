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
 * Set of ENABLE_XXX flags and V0p2_REV for REV2.
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

#ifndef ARDUINO_LIB_OTV0P2_CONFIG_REV2_H
#define ARDUINO_LIB_OTV0P2_CONFIG_REV2_H


// For trial over winter of 2013--4, second round (REV2).

#ifdef CONFIG_Trial2013Winter_Round2_LVBHSH // REV2 cut4: local valve control, boiler hub, stats hub & TX.
#define CONFIG_Trial2013Winter_Round2 // Just like normal REV2 except...
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#define ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#define ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#define ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.
#define ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#define ENABLE_CLI
// IF DEFINED: support for general timed and multi-input occupancy detection / use.
#define ENABLE_OCCUPANCY_SUPPORT
#endif

#ifdef CONFIG_Trial2013Winter_Round2_LVBH // REV2 cut4: local valve control, boiler hub & TX.
#define CONFIG_Trial2013Winter_Round2 // Just like normal REV2 except...
// IF DEFINED: try to trim memory (primarily RAM, also code/Flash) space used.
#define ENABLE_TRIMMED_MEMORY
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#define ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#define ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.
#define ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
// IF DEFINED: this unit supports CLI over the USB/serial connection, eg for run-time reconfig.
#define ENABLE_CLI
// IF DEFINED: support for general timed and multi-input occupancy detection / use.
#define ENABLE_OCCUPANCY_SUPPORT
#endif

#ifdef CONFIG_Trial2013Winter_Round2_BOILERHUB // For trial over winter of 2013--4, second round (REV2), as pure boiler hub + can TX stats.
#define CONFIG_Trial2013Winter_Round2 // Just like normal REV2 except...
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#define ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
#endif

#ifdef CONFIG_Trial2013Winter_Round2_STATSHUB // For trial over winter of 2013--4, second round (REV2), as stats hub.
#define CONFIG_Trial2013Winter_Round2 // Just like normal REV2 except...
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#define ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#undef ENABLE_STATS_TX // Don't allow it to TX its own...
#endif

#ifdef CONFIG_Trial2013Winter_Round2_NOHUB // REV2 cut4 as TX-only leaf node.
#define CONFIG_Trial2013Winter_Round2 // Just like normal REV2 except...
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
#endif

#ifdef CONFIG_Trial2013Winter_Round2_CC1HUB_SECURE
// Secure COHEAT REV2 hub just like normal one except ...
#define CONFIG_Trial2013Winter_Round2_CC1HUB
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
#define ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
#endif

#ifdef CONFIG_Trial2013Winter_Round2_CC1HUB // REV2 cut4 as CC1 hub.
#define CONFIG_Trial2013Winter_Round2 // Just like normal REV2 except...
// IF DEFINED: use (slow, low energy) 32768Hz-clock-based watchdog to recover from some software hangups.
#define ENABLE_WATCHDOG_SLOW
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX // Not needed for CC1 frames.
// IF DEFINED: allow TX of stats frames.
#undef ENABLE_STATS_TX
// IF DEFINED: enable support for FS20 carrier for RX of raw FS20 and piggybacked binary (non-JSON) stats.
#undef ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX
// IF DEFINED: enable support for FS20 encoding/decoding, eg to send to FHT8V.
#undef ENABLE_FS20_ENCODING_SUPPORT
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#define ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV // THESE HUB UNITS DO NOT manage a local TRV.
// IF DEFINED: allow JSON stats frames alongside binary ones.
#undef ENABLE_JSON_OUTPUT
// IF DEFINED: enable a full OpenTRV CLI.
#undef ENABLE_FULL_OT_CLI
// IF DEFINED: enable a full OpenTRV UI with normal LEDs etc.
#undef ENABLE_FULL_OT_UI
// IF DEFINED: basic FROST/WARM temperatures are settable.
#undef ENABLE_SETTABLE_TARGET_TEMPERATURES
// IF DEFINED: enable and extended CLI with a longer input buffer for example.
#define ENABLE_EXTENDED_CLI
// IF DEFINED: support for general timed and multi-input occupancy detection / use.
#undef ENABLE_OCCUPANCY_SUPPORT // None of that logic required at hub.
// IF DEFINED: act as CC1 simple hub node.
#define ALLOW_CC1_SUPPORT
#define ALLOW_CC1_SUPPORT_HUB
#endif

#ifdef CONFIG_Trial2013Winter_Round2_BHR // REV2 cut4: boiler hub and stats relay.
#define CONFIG_Trial2013Winter_Round2 // Just like normal REV2 except...
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
// Chose NullRadio as secondary.
#define RADIO_SECONDARY_NULL
// IF DEFINED: allow periodic machine- and human- readable status report to serial, starting with "="/
//#undef ENABLE_SERIAL_STATUS_REPORT
#endif

#ifdef CONFIG_Trial2013Winter_Round2_SECURE_NOHUB
#define CONFIG_Trial2013Winter_Round2 // Just like normal REV2 except...
// IF DEFINED: try to trim bandwidth as may be especially expensive/scarce.
#undef ENABLE_TRIMMED_BANDWIDTH
// IF DEFINED: support one on and one off time per day (possibly in conjunction with 'learn' button).
#undef ENABLE_SINGLETON_SCHEDULE
// IF DEFINED: use active-low LEARN button(s).  Needs ENABLE_SINGLETON_SCHEDULE.
#undef ENABLE_LEARN_BUTTON // OPTIONAL ON V0.09 PCB1
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF DEFINED: allow radio listen/RX.
#undef ENABLE_RADIO_RX
// IF DEFINED: forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#undef ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: allow JSON stats frames alongside binary ones.
#define ENABLE_JSON_OUTPUT
// IF DEFINED: allow binary stats to be TXed.
#undef ENABLE_BINARY_STATS_TX
// IF DEFINED: enable support for FS20 carrier for RX or TX.
#undef ENABLE_FS20_CARRIER_SUPPORT
// IF DEFINED: use FHT8V wireless radio module/valve.
#undef ENABLE_FHT8VSIMPLE
// IF DEFINED: enable support for fast (>50kbps) packet-handling carrier (leading length byte).
#define ENABLE_FAST_FRAMED_CARRIER_SUPPORT
// IF DEFINED: enable support for FS20 carrier for TX specifically (to allow RX-only).
#undef ENABLE_FS20_CARRIER_SUPPORT_TX
// IF DEFINED: enable support for FS20 encoding/decoding, eg to send to FHT8V.
#undef ENABLE_FS20_ENCODING_SUPPORT
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
#define ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: allow non-secure OpenTRV secure frame RX (as of 2015/12): DISABLED BY DEFAULT.
#undef ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED
// IF DEFINED: enable a CLI-settable setback lockout (hours/days) to establish a baseline before engaging energy saving setbacks.
#define ENABLE_SETBACK_LOCKOUT_COUNTDOWN
#endif


#ifdef CONFIG_Trial2013Winter_Round2_SECURE_STATSHUB
#define CONFIG_Trial2013Winter_Round2 // Just like normal REV2 except...
// IF DEFINED: there is run-time help available for the CLI.
#undef ENABLE_CLI_HELP
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
// IF DEFINED: this unit controls a valve, but provides slave valve control only.
#undef ENABLE_SLAVE_TRV
// IF DEFINED: (default) forced always-on radio listen/RX, eg not requiring setup to explicitly enable.
#define ENABLE_DEFAULT_ALWAYS_RX
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#define ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#undef ENABLE_STATS_TX
// IF DEFINED: allow JSON stats frames alongside binary ones.
#undef ENABLE_JSON_OUTPUT
// IF DEFINED: enable periodic secure beacon broadcast.
#undef ENABLE_SECURE_RADIO_BEACON
// IF DEFINED: allow binary stats to be TXed.
#undef ENABLE_BINARY_STATS_TX
// IF DEFINED: enable support for fast (>50kbps) packet-handling carrier (leading length byte).
#define ENABLE_FAST_FRAMED_CARRIER_SUPPORT
// IF DEFINED: enable support for FS20 carrier for RX or TX.
#undef ENABLE_FS20_CARRIER_SUPPORT
// IF DEFINED: use FHT8V wireless radio module/valve.
#undef ENABLE_FHT8VSIMPLE
// IF DEFINED: enable support for FS20 carrier for TX specifically (to allow RX-only).
#undef ENABLE_FS20_CARRIER_SUPPORT_TX
// IF DEFINED: enable support for FS20 encoding/decoding, eg to send to FHT8V.
#undef ENABLE_FS20_ENCODING_SUPPORT
// IF DEFINED: enable OpenTRV secure frame encoding/decoding (as of 2015/12).
#define ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
// IF DEFINED: allow non-secure OpenTRV secure frame RX (as of 2015/12): DISABLED BY DEFAULT.
#undef ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED
#endif

#ifdef CONFIG_Trial2013Winter_Round2 // For trial over winter of 2013--4, second round (REV2).
// Revision REV2 (cut4+) of V0.2 board.
// All these (developer) devices can have IDs set in situ.
#define V0p2_REV 2
// IF DEFINED: allow for less light on sideways-pointing LDR on cut4 2014/03/17 REV2 boards (TODO-209).
#define ENABLE_AMBLIGHT_EXTRA_SENSITIVE
// IF DEFINED: allow setting of ID from CLI to replace devices in situ; not recommended by default to avoid confusion.
#define ENABLE_ID_SET_FROM_CLI
#endif


#endif
