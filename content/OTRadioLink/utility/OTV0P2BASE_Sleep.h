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
//
// It is also possible to save some power by slowing the CPU clock,
// though that may disrupt connected timing for I/O device such as the UART,
// and would possibly cause problems for ISRs invoked while the clock is slow.

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


// If CPU clock is 1MHz then *assume* that it is the 8MHz internal RC clock prescaled by 8 unless DEFAULT_CPU_PRESCALE is defined.
#if F_CPU == 1000000L
static const uint8_t DEFAULT_CPU_PRESCALE = 3;
#else
static const uint8_t DEFAULT_CPU_PRESCALE = 1;
#endif

static const clock_div_t MAX_CPU_PRESCALE = clock_div_256; // At least for the ATmega328P...
// Minimum scaled CPU clock speed; expected to be 31250Hz when driven from 8MHz RC clock.
#if F_CPU > 16000000L
static const uint32_t MIN_CPU_HZ = ((F_CPU) >> (((int) MAX_CPU_PRESCALE) - (DEFAULT_CPU_PRESCALE)));
#else
static const uint16_t MIN_CPU_HZ = ((F_CPU) >> (((int) MAX_CPU_PRESCALE) - (DEFAULT_CPU_PRESCALE)));
#endif

// Sleep for specified number of _delay_loop2() loops at minimum available CPU speed.
// Each loop takes 4 cycles at that minimum speed, but entry and exit overheads may take the equivalent of a loop or two.
// Note: inlining is prevented so as to avoid migrating anything into the section where the CPU is running slowly.
// Not recommended as-is as may interact badly with interrupts if used naively (eg ISR code runs very slowly).
// This may only be safe to use with interrupts disabled.
void _sleepLowPowerLoopsMinCPUSpeed(uint16_t loops) __attribute__ ((noinline));
// Sleep/spin for approx specified strictly-positive number of milliseconds, in as low-power mode as possible.
// This may be achieved in part by dynamically slowing the CPU clock if possible.
// Macro to allow some constant folding at compile time where the sleep-time argument is constant.
// Should be good for values up to at least 1000, ie 1 second.
// Assumes MIN_CPU_HZ >> 4000.
// TODO: break out to non-inlined routine where arg is not constant (__builtin_constant_p).
// Not recommended as-is as may interact badly with interrupts if used naively (eg ISR code runs very slowly).
static void inline _sleepLowPowerMs(const uint16_t ms) { _sleepLowPowerLoopsMinCPUSpeed((((MIN_CPU_HZ * (ms)) + 2000) / 4000) - ((MIN_CPU_HZ>=12000)?2:((MIN_CPU_HZ>=8000)?1:0))); }
// Sleep/spin for (typically a little less than) strictly-positive specified number of milliseconds, in as low-power mode as possible.
// This may be achieved in part by dynamically slowing the CPU clock if possible.
// Macro to allow some constant folding at compile time where the sleep-time argument is constant.
// Should be good for values up to at least 1000, ie 1 second.
// Uses formulation likely to be quicker than _sleepLowPowerMs() for non-constant argument values,
// and that results in a somewhat shorter sleep than _sleepLowPowerMs(ms).
// Assumes MIN_CPU_HZ >> 4000.
// TODO: break out to non-inlined routine where arg is not constant (__builtin_constant_p).
// Not recommended as-is as may interact badly with interrupts if used naively (eg ISR code runs very slowly).
static void inline _sleepLowPowerLessThanMs(const uint16_t ms) { _sleepLowPowerLoopsMinCPUSpeed(((MIN_CPU_HZ/4000) * (ms)) - ((MIN_CPU_HZ>=12000)?2:((MIN_CPU_HZ>=8000)?1:0))); }
// Sleep/spin for approx specified strictly-positive number of milliseconds, in as low-power mode as possible.
// Nap() may be more efficient for intervals of longer than 15ms.
// Interrupts are blocked for about 1ms at a time.
// Should be good for the full range of values and should take no time where 0ms is specified.
static void inline sleepLowPowerMs(uint16_t ms) { while(ms-- > 0) { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { _sleepLowPowerMs(1); } } }
// Sleep/spin for (typically a little less than) strictly-positive specified number of milliseconds, in as low-power mode as possible.
// Nap() may be more efficient for intervals of longer than 15ms.
// Interrupts are blocked for about 1ms at a time.
// Should be good for the full range of values and should take no time where 0ms is specified.
static void inline sleepLowPowerLessThanMs(uint16_t ms) { while(ms-- > 0) { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { _sleepLowPowerLessThanMs(1); } } }


}
#endif
