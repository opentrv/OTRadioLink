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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2016
*/

/*
  Routines for sleeping for various times with particular trade-offs.
  Uses a combination of sleep modes, watchdog timer (WDT), and other techniques.

  Hardware specific.
  */

#ifdef ARDUINO_ARCH_AVR
#include <util/crc16.h>
#endif

#include "OTV0P2BASE_Sleep.h"


namespace OTV0P2BASE
{

#ifdef ARDUINO_ARCH_AVR

// Define macro to disable BOD during sleep here if not included in Arduino AVR toolset...
// This is only for "pico-power" variants, eg the "P" in ATmega328P.
#ifndef sleep_bod_disable
#define sleep_bod_disable() \
do { \
  uint8_t tempreg; \
  __asm__ __volatile__("in %[tempreg], %[mcucr]" "\n\t" \
                       "ori %[tempreg], %[bods_bodse]" "\n\t" \
                       "out %[mcucr], %[tempreg]" "\n\t" \
                       "andi %[tempreg], %[not_bodse]" "\n\t" \
                       "out %[mcucr], %[tempreg]" \
                       : [tempreg] "=&d" (tempreg) \
                       : [mcucr] "I" _SFR_IO_ADDR(MCUCR), \
                         [bods_bodse] "i" (_BV(BODS) | _BV(BODSE)), \
                         [not_bodse] "i" (~_BV(BODSE))); \
} while (0)
#endif

// Sleep with BOD disabled in power-save mode; will wake on any interrupt.
void sleepPwrSaveWithBODDisabled()
  {
  set_sleep_mode(SLEEP_MODE_PWR_SAVE); // Stop all but timer 2 and watchdog when sleeping.
  cli();
  sleep_enable();
  sleep_bod_disable();
  sei();
  sleep_cpu();
  sleep_disable();
  sei();
  }


// Set non-zero when the watchdog ISR is invoked, ie the watchdog timer has gone off.
// Cleared at the start of the watchdog sleep routine.
// May contain a little entropy concentrated in the least-significant bits, in part from WDT-vs-CPU-clock jitter, especially if not sleeping.
static volatile uint8_t _watchdogFired;
// Catch watchdog timer interrupt to automatically clear WDIE and WDIF.
// This allows use of watchdog for low-power timed sleep.
ISR(WDT_vect)
  {
  // WDIE and WDIF are cleared in hardware upon entering this ISR.
  wdt_disable();
  // Note: be careful of what is accessed from this ISR.
  // Capture some marginal entropy from the stack position.
  uint8_t x;
  _watchdogFired = ((uint8_t) 0x80) | ((uint8_t) (int) &x); // Ensure non-zero, retaining any entropy in ls bits.
  }


// Idle the CPU for specified time but leave everything else running (eg UART), returning on any interrupt or the watchdog timer.
//   * watchdogSleep is one of the WDTO_XX values from <avr/wdt.h>
// Should reduce power consumption vs spinning the CPU more than 3x, though not nearly as much as nap().
// True iff watchdog timer expired; false if something else woke the CPU.
// WARNING: DHD20150827: seems able to cause crash/reset of some REV0 and REV9 boards, eg called from CLI.
bool _idleCPU(const int_fast8_t watchdogSleep, const bool allowPrematureWakeup)
  {
  // Watchdog should (already) be disabled on entry.
  _watchdogFired = 0;
  wdt_enable(watchdogSleep);
  WDTCSR |= (1 << WDIE);
  // Keep sleeping until watchdog actually fires, unless premature return is permitted.
  for( ; ; )
    {
    set_sleep_mode(SLEEP_MODE_IDLE); // Leave everything running but the CPU...
    sleep_mode();
    const bool fired = (0 != _watchdogFired);
    if(fired || allowPrematureWakeup)
      {
      wdt_disable(); // Avoid spurious wakeup later.
      return(fired);
      }
    }
  }

// Sleep briefly in as lower-power mode as possible until the specified (watchdog) time expires.
//   * watchdogSleep is one of the WDTO_XX values from <avr/wdt.h>
// May be useful to call minimsePowerWithoutSleep() first, when not needing any modules left on.
// NOTE: will stop clocks for UART, etc.
void nap(const int_fast8_t watchdogSleep)
  {
  // Watchdog should (already) be disabled on entry.
  _watchdogFired = 0;
  wdt_enable(watchdogSleep);
  WDTCSR |= (1 << WDIE);
  // Keep sleeping until watchdog actually fires.
  for( ; ; )
    {
    sleepPwrSaveWithBODDisabled();
    if(0 != _watchdogFired)
      {
      wdt_disable(); // Avoid spurious wakeup later.
      return; // All done!
      }
    }
 }

// Sleep briefly in as lower-power mode as possible until the specified (watchdog) time expires, or another interrupt.
//   * watchdogSleep is one of the WDTO_XX values from <avr/wdt.h>
//   * allowPrematureWakeup if true then if woken before watchdog fires return false; default false
// Returns false if the watchdog timer did not go off, true if it did.
// May be useful to call minimsePowerWithoutSleep() first, when not needing any modules left on.
// NOTE: will stop clocks for UART, etc.
bool nap(const int_fast8_t watchdogSleep, const bool allowPrematureWakeup)
  {
  // Watchdog should (already) be disabled on entry.
  _watchdogFired = 0;
  wdt_enable(watchdogSleep);
  WDTCSR |= (1 << WDIE);
  // Keep sleeping until watchdog actually fires, unless premature return is permitted.
  for( ; ; )
    {
    sleepPwrSaveWithBODDisabled();
    const bool fired = (0 != _watchdogFired);
    if(fired || allowPrematureWakeup)
      {
      wdt_disable(); // Avoid spurious wakeup later.
      return(fired);
      }
    }
  }


// Sleep for specified number of _delay_loop2() loops at minimum available CPU speed.
// Each loop takes 4 cycles at that minimum speed, but entry and exit overheads may take the equivalent of a loop or two.
// Note: inlining is prevented so as to avoid migrating anything into the section where the CPU is running slowly.
//
// Note: may be dubious to run CPU clock less than 4x 32768Hz crystal speed,
// eg at 31250Hz for 8MHz RC clock and max prescale.
// Don't access timer 2 registers at low CPU speed, eg in ISRs.
//
// This may only be safe to use in practice with interrupts disabled.
__attribute__ ((noinline)) void _sleepLowPowerLoopsMinCPUSpeed(uint16_t loops)
  {
  const clock_div_t prescale = clock_prescale_get(); // Capture current prescale value.
  clock_prescale_set(MAX_CPU_PRESCALE); // Reduce clock speed (increase prescale) as far as possible.
  _delay_loop_2(loops); // Burn cycles...
  clock_prescale_set(prescale); // Restore clock prescale.
  }

// Sleep in reasonably low-power mode until specified target subcycle time.
// Returns true if OK, false if specified time already passed or significantly missed (eg by more than one tick).
// May use a combination of techniques to hit the required time.
// Requesting a sleep until at or near the end of the cycle risks overrun and may be unwise.
// Using this to sleep less then 2 ticks may prove unreliable as the RTC rolls on underneath...
// This is NOT intended to be used to sleep over the end of a minor cycle.
// May poll I/O.
bool sleepUntilSubCycleTime(const uint8_t sleepUntil)
  {
  for( ; ; )
    {
    const uint8_t now = OTV0P2BASE::getSubCycleTime();
    if(now == sleepUntil) { return(true); } // Done it!
    if(now > sleepUntil) { return(false); } // Too late...

    // Compute time left to sleep.
    // It is easy to sleep a bit more later if necessary, but oversleeping is bad.
    const uint8_t ticksLeft = sleepUntil - now;
    // Deal with shortest sleep specially to avoid missing target from overheads...
    if(1 == ticksLeft)
      {
      // Take a very short sleep, much less than half a tick,
      // eg as may be some way into this tick already.
      //burnHundredsOfCyclesProductively();
      OTV0P2BASE::sleepLowPowerLessThanMs(1);
      continue;
      }

    // Compute remaining time in milliseconds, rounded down...
    const uint16_t msLeft = ((uint16_t)OTV0P2BASE::SUBCYCLE_TICK_MS_RD) * ticksLeft;

    // If comfortably in the area of nap()s then use one of them for improved energy savings.
    // Allow for nap() to overrun a little as its timing can vary with temperature and supply voltage,
    // and the bulk of energy savings should still be available without pushing the timing to the wire.
    // Note that during nap() timer0 should be stopped and thus not cause premature wakeup (from overflow interrupt).
    if(msLeft >= 20)
      {
      if(msLeft >= 80)
        {
        if(msLeft >= 333)
          {
          ::OTV0P2BASE::nap(WDTO_250MS); // Nominal 250ms sleep.
          continue;
          }
        ::OTV0P2BASE::nap(WDTO_60MS); // Nominal 60ms sleep.
        continue;
        }
      ::OTV0P2BASE::nap(WDTO_15MS); // Nominal 15ms sleep.
      continue;
      }

    // Use low-power CPU sleep for residual time, but being very careful not to over-sleep.
    // Aim to sleep somewhat under residual time, eg to allow for overheads, interrupts, and other slippages.
    // Assumed to be > 1 else would have been special-cased above.
    // Assumed to be << 1s else a nap() would have been used above.
#ifdef DEBUG
    if((msLeft < 2) || (msLeft > 1000)) { panic(); }
#endif
    OTV0P2BASE::sleepLowPowerLessThanMs(msLeft - 1);
    }
  }


// Extract and return a little entropy from clock jitter between CPU and WDT clocks; possibly one bit of entropy captured.
// Expensive in terms of CPU time and thus energy.
// TODO: may be able to reduce clock speed to lower energy cost while still detecting useful jitter.
// NOTE: in this file to have direct access to WDT.
uint_fast8_t clockJitterWDT()
  {
  // Watchdog should be (already) be disabled on entry.
  _watchdogFired = false;
  wdt_enable(WDTO_15MS); // Set watchdog for minimum time.
  WDTCSR |= (1 << WDIE);
  uint_fast8_t count = 0;
  while(!_watchdogFired) { ++count; } // Effectively count CPU cycles until WDT fires.
  return(count);
  }

// Combined clock jitter techniques to return approximately 8 bits (the entire result byte) of entropy efficiently on demand.
// Expensive in terms of CPU time and thus energy, though possibly more efficient than basic clockJitterXXX() routines.
// Internally this uses a CRC as a relatively fast and hopefully effective hash over intermediate values.
// Note the that rejection of repeat values will be less effective with two interleaved gathering mechanisms
// as the interaction while not necessarily adding genuine entropy, will make counts differ between runs.
// DHD20130519: measured as taking ~63ms to run, ie ~8ms per bit gathered.
// NOTE: in this file to have direct access to WDT.
uint_fast8_t clockJitterEntropyByte()
  {
  uint16_t hash = 0;

  uint_fast8_t result = 0;
  uint_fast8_t countR = 0, lastCountR = 0;
  uint_fast8_t countW = 0, lastCountW = 0;

  const uint8_t t0 = TCNT2; // Wait for sub-cycle timer to roll.
  while(t0 == TCNT2) { ++hash; } // Possibly capture some entropy from recent program activity/timing.
  uint8_t t1 = TCNT2;

  _watchdogFired = 0;
  wdt_enable(WDTO_15MS); // Start watchdog, with minimum timeout.
  WDTCSR |= (1 << WDIE);
  int_fast8_t bitsLeft = 8; // Decrement when a bit is harvested...
  for( ; ; )
    {
    // Extract watchdog jitter vs CPU.
    if(!_watchdogFired) { ++countW; }
    else // Watchdog fired.
      {
      if(countW != lastCountW) // Got a different value from last; assume one bit of entropy.
        {
        hash = _crc_ccitt_update(hash, countW);
        result = (result << 1) ^ ((uint_fast8_t)hash); // Nominally capturing (at least) lsb of hash.
        if(--bitsLeft <= 0) { break; } // Got enough bits; stop now.
        lastCountW = countW;
        }
      countW = 0;
      _watchdogFired = 0;
      wdt_enable(WDTO_15MS); // Restart watchdog, with minimum timeout.
      WDTCSR |= (1 << WDIE);
      }

    // Extract RTC jitter vs CPU.
    if(t1 == TCNT2) { --countR; }
    else // Sub-cycle timer rolled.
      {
      if(countR != lastCountR) // Got a different value from last; assume one bit of entropy.
        {
        hash = _crc_ccitt_update(hash, countR);
        result = (result << 1) ^ ((uint_fast8_t)hash); // Nominally capturing (at least) lsb of hash.
        if(--bitsLeft <= 0) { break; } // Got enough bits; stop now.
        lastCountR = countR;
        }
      countR = 0;
      t1 = TCNT2; // Set to look for next roll.
      }
    }

  wdt_disable(); // Ensure no spurious WDT wakeup pending.
  return(result);
  }

#endif // ARDUINO_ARCH_AVR



}
