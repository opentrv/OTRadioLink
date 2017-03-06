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
*/

/*
 * Fixups to apply after loading the target config.
 *
 * Helps fix combinations of ENABLEs for module interdependencies.
 *
 * NOT to be included by ANY library routines,
 * though may be included by other application CONFIG heqders.
 *
 * A typical application will set up configuration with something like:
 *     #include <OTV0p2_valve_ENABLE_defaults.h>
 *     #define CONFIG_XX...
 *     #include <OTV0p2_CONFIG_REVXX.h>
 *     #include <OTV0p2_valve_ENABLE_fixups.h>
 *     ... residual fixups ....
 */

// The long-term target is for this to become empty.

#ifndef ARDUINO_LIB_OTV0P2_VALVE_ENABLE_FIXUPS_H
#define ARDUINO_LIB_OTV0P2_VALVE_ENABLE_FIXUPS_H


// If ENABLE_LEARN_BUTTON then in the absence of anything better ENABLE_SINGLETON_SCHEDULE should be supported.
#ifdef ENABLE_LEARN_BUTTON
#ifndef ENABLE_SINGLETON_SCHEDULE
#define ENABLE_SINGLETON_SCHEDULE
#endif
#endif

// For now (DHD20150927) allowing stats TX forces allowing JSON stats.
// IF DEFINED: allow TX of stats frames.
#ifdef ENABLE_STATS_TX
// IF DEFINED: allow JSON stats frames alongside binary ones.
#define ENABLE_JSON_OUTPUT
#endif

// If a stats or boiler hub, define ENABLE_HUB_LISTEN.
#if defined(ENABLE_BOILER_HUB) || defined(ENABLE_STATS_RX)
#define ENABLE_HUB_LISTEN
// Force-enable RX if not already so.
#define ENABLE_RADIO_RX
#endif

// If (potentially) needing to run in some sort of continuous RX mode, define a flag.
#if defined(ENABLE_HUB_LISTEN) || defined(ENABLE_DEFAULT_ALWAYS_RX)
#define ENABLE_CONTINUOUS_RX // was #define CONFIG_IMPLIES_MAY_NEED_CONTINUOUS_RX true
#endif

// By default (up to 2015), use the RFM22/RFM23 module to talk to an FHT8V wireless radiator valve.
#ifdef ENABLE_FHT8VSIMPLE
#define ENABLE_RADIO_RFM23B
#define ENABLE_FS20_CARRIER_SUPPORT
#define ENABLE_FS20_ENCODING_SUPPORT
// If this can be a hub, enable extra RX code.
#if defined(ENABLE_BOILER_HUB) || defined(ENABLE_STATS_RX)
#define ENABLE_FHT8VSIMPLE_RX
#endif // defined(ENABLE_BOILER_HUB) || defined(ENABLE_STATS_RX)
#if defined(ENABLE_STATS_RX)
#define ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX
#endif // defined(ENABLE_STATS_RX)
#endif // ENABLE_FHT8VSIMPLE



#endif
