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
 * Set of ENABLE_XXX flags and V0p2_REV for REV1.
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

#ifndef ARDUINO_LIB_OTV0P2_CONFIG_REV1_H
#define ARDUINO_LIB_OTV0P2_CONFIG_REV1_H


// For trial over winter of 2013--4, first round (REV1).

#ifdef CONFIG_Trial2013Winter_Round1_LVBHSH // REV1: local valve control, boiler hub, stats hub & TX.
#define CONFIG_Trial2013Winter_Round1 // Just like normal REV1 except...
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

#ifdef CONFIG_Trial2013Winter_Round1_BOILERHUB // REV1 as plain boiler hub + can TX stats.
#define CONFIG_Trial2013Winter_Round1 // Just like normal REV1 except...
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#define ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// IF DEFINED: this unit will act as a thermostat controlling a local TRV (and calling for heat from the boiler), else is a sensor/hub unit.
#undef ENABLE_LOCAL_TRV
#endif

#ifdef CONFIG_Trial2013Winter_Round1_STATSHUB // REV1 as stats hub.
#define CONFIG_Trial2013Winter_Round1 // Just like normal REV1 except...
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#define ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#undef ENABLE_STATS_TX // Don't allow it to TX its own...
#endif

#ifdef CONFIG_Trial2013Winter_Round1_NOHUB // REV1 as TX-only leaf node.
#define CONFIG_Trial2013Winter_Round1 // Just like normal REV2 except...
// IF DEFINED: this unit can act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF DEFINED: allow RX of stats frames.
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
#endif

#ifdef CONFIG_Trial2013Winter_Round1 // For trial over winter of 2013--4, first round (REV1).
// Revision REV1 of V0.2 board.
#define V0p2_REV 1
// TODO-264: Find out why IDLE seems to crash some REV1 boards.
#undef ENABLE_USE_OF_AVR_IDLE_MODE
#endif

//___________________________________

#ifdef CONFIG_BH_DHW // DHW on REV1 board.
// Revision of V0.2 board.
#define V0p2_REV 1
// Enable use of OneWire devices.
#define ENABLE_MINIMAL_ONEWIRE_SUPPORT
// Enable use of DS18B20 temp sensor.
#define ENABLE_PRIMARY_TEMP_SENSOR_DS18B20
// Select DHW temperatures by default.
#define DHW_TEMPERATURES
// Must minimise water flow.
#define TRV_SLEW_GLACIAL
// Set max percentage open: BH reports 30% to be (near) optimal 2015/03; BH requested 20% at 2015/10/15, 13% at 2016/01/19.
#define TRV_MAX_PC_OPEN 13
// IF UNDEFINED: this unit cannot act as boiler-control hub listening to remote thermostats, possibly in addition to controlling a local TRV.
#undef ENABLE_BOILER_HUB
// IF UNDEFINED: don't allow RX of stats frames (since there is no easy way to plug in a serial connection to relay them!)
#undef ENABLE_STATS_RX
// IF DEFINED: allow TX of stats frames.
#define ENABLE_STATS_TX
// TODO-264: Find out why IDLE seems to crash some REV1 boards.
#undef ENABLE_USE_OF_AVR_IDLE_MODE
// Override schedule on time to simple fixed value of 2h per BH request 2015/10/15.
#define LEARNED_ON_PERIOD_M 120 // Must be <= 255.
#define LEARNED_ON_PERIOD_COMFORT_M LEARNED_ON_PERIOD_M
// Bo just wants the timing for his DHW; no occupancy sensing.
// IF DEFINED: support for general timed and multi-input occupancy detection / use.
#undef ENABLE_OCCUPANCY_SUPPORT
// IF DEFINED: detect occupancy based on ambient light, if available.
#undef ENABLE_OCCUPANCY_DETECTION_FROM_AMBLIGHT
#endif


#endif

