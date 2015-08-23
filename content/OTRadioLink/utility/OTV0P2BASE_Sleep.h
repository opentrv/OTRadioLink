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

Author(s) / Copyright (s): Damon Hart-Davis 2015
*/

/*
  Routines for sleeping for various times with particular trade-offs.
  Uses a combination of sleep modes, watchdog timer (WDT), and other techniques.
  */

#ifndef OTV0P2BASE_SLEEP_H
#define OTV0P2BASE_SLEEP_H

#include <stdint.h>
#include <util/atomic.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <util/delay_basic.h>
#include <Arduino.h>


namespace OTV0P2BASE
{


// Normal power drain ignoring I/O (typ 0.3mA @ 1MHz CPU, 2V)
// idleCPU() routines put the AVR into idle mode with WDT wake-up (typ 40uA @ 1MHz CPU, 2V; 3x--10x savings);
//   all clocks except CPU run so, for example, Serial should still function.
// nap() routines put the AVR into power-save mode with WTD wake-up (typ 0.8uA+ @1.8V);
//   stops I/O clocks and all timers except timer 2 (for the RTC).
// Sleeping in power save mode as per napXXX() waits for timer 2 or external interrupt (typ 0.8uA+ @1.8V).

// Sleep with BOD disabled in power-save mode; will wake on any interrupt.
// This particular API is not guaranteed to be maintained: please use sleepUntilInt() instead.
void sleepPwrSaveWithBODDisabled();

// Sleep indefinitely in as lower-power mode as possible until a specified watchdog time expires, or another interrupt.
// May be useful to call minimsePowerWithoutSleep() first, when not needing any modules left on.
static inline void sleepUntilInt() { sleepPwrSaveWithBODDisabled(); }

// Idle the CPU for specified time but leave everything else running (eg UART), returning on any interrupt or the watchdog timer.
//   * watchdogSleep is one of the WDTO_XX values from <avr/wdt.h>
//   * allowPrematureWakeup if true then if woken before watchdog fires return false (default false)
// Should reduce power consumption vs spinning the CPU more than 3x, though not nearly as much as nap().
// True iff watchdog timer expired; false if something else woke the CPU.
// Only use this if not disallowed for board type, eg with ENABLE_USE_OF_AVR_IDLE_MODE.
bool idleCPU(int_fast8_t watchdogSleep, bool allowPrematureWakeup = false);

// Sleep briefly in as lower-power mode as possible until the specified (watchdog) time expires.
//   * watchdogSleep is one of the WDTO_XX values from <avr/wdt.h>
// May be useful to call minimsePowerWithoutSleep() first, when not needing any modules left on.
// NOTE: will stop clocks for UART, etc.
void nap(int_fast8_t watchdogSleep);

// Sleep briefly in as lower-power mode as possible until the specified (watchdog) time expires, or another interrupt.
//   * watchdogSleep is one of the WDTO_XX values from <avr/wdt.h>
//   * allowPrematureWakeup if true then if woken before watchdog fires return false
// Returns false if the watchdog timer did not go off, true if it did.
// May be useful to call minimsePowerWithoutSleep() first, when not needing any modules left on.
// NOTE: will stop clocks for UART, etc.
bool nap(int_fast8_t watchdogSleep, bool allowPrematureWakeup);


}
#endif
