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

Author(s) / Copyright (s): Damon Hart-Davis 2016
*/

/*
 * OpenTRV radiator valve physical UI controls and output as an actuator.
 *
 * A base class, a null class, and one or more implementations are provided
 * for different stock behaviour with different hardware.
 *
 * A mixture of template and constructor parameters
 * is used to configure the different types.
 */

#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include <OTV0P2BASE_Actuator.h>

#include "OTV0P2BASE_ActuatorPhysicalUI.h"


// Use namespaces to help avoid collisions.
namespace OTV0P2BASE
    {


#ifdef ARDUINO_ARCH_AVR
// Use WDT-based timer for xxxPause() routines.
// Very tiny low-power sleep.
static const uint8_t VERYTINY_PAUSE_MS = 5;
static void inline veryTinyPause() { OTV0P2BASE::sleepLowPowerMs(VERYTINY_PAUSE_MS); }
// Tiny low-power sleep to approximately match the PICAXE V0.09 routine of the same name.
static const uint8_t TINY_PAUSE_MS = 15;
static void inline tinyPause() { OTV0P2BASE::nap(WDTO_15MS); } // 15ms vs 18ms nominal for PICAXE V0.09 impl.
// Small low-power sleep.
static const uint8_t SMALL_PAUSE_MS = 30;
static void inline smallPause() { OTV0P2BASE::nap(WDTO_30MS); }
// Medium low-power sleep to approximately match the PICAXE V0.09 routine of the same name.
// Premature wakeups MAY be allowed to avoid blocking I/O polling for too long.
static const uint8_t MEDIUM_PAUSE_MS = 60;
static void inline mediumPause() { OTV0P2BASE::nap(WDTO_60MS); } // 60ms vs 144ms nominal for PICAXE V0.09 impl.
// Big low-power sleep to approximately match the PICAXE V0.09 routine of the same name.
// Premature wakeups MAY be allowed to avoid blocking I/O polling for too long.
static const uint8_t BIG_PAUSE_MS = 120;
static void inline bigPause() { OTV0P2BASE::nap(WDTO_120MS); } // 120ms vs 288ms nominal for PICAXE V0.09 impl.

// Record local manual operation of a physical UI control, eg not remote or via CLI.
// Marks room as occupied amongst other things.
// To be thread-/ISR- safe, everything that this touches or calls must be.
// Thread-safe.
void ModeButtonAndPotActuatorPhysicalUI::markUIControlUsed()
    {
    statusChange = true; // Note user interaction with the system.
    uiTimeoutM = UI_DEFAULT_RECENT_USE_TIMEOUT_M; // Ensure that UI controls are kept 'warm' for a little while.
//  #if defined(ENABLE_UI_WAKES_CLI)
//    // Make CLI active for a while (at some slight possibly-significant energy cost).
//    resetCLIActiveTimer(); // Thread-safe.
//  #endif
    // User operation of controls locally is strong indication of presence.
//    Occupancy.markAsOccupied(); // Thread-safe.
    // Call occupancy callback if set.
    void (*occCallbackFN)() = occCallback;
    if(NULL != occCallbackFN) { occCallbackFN(); }
    }

// Call this nominally on even numbered seconds to allow the UI to operate.
// In practice call early once per 2s major cycle.
// Should never be skipped, so as to allow the UI to remain responsive.
// Runs in 350ms or less; usually takes only a few milliseconds or microseconds.
// Returns a non-zero value iff the user interacted with the system, and maybe caused a status change.
// NOTE: since this is on the minimum idle-loop code path, minimise CPU cycles, esp in frost mode.
// Replaces: bool tickUI(uint_fast8_t sec).
uint8_t ModeButtonAndPotActuatorPhysicalUI::read()
    {
    value = 0; // FIXME
    return(value);
    }

#endif


    }
