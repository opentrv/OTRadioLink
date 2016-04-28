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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2015
*/

/*
 Real-time clock support AND RTC-connected watchdog/reset.
 */

#ifndef OTV0P2BASE_RTC_H
#define OTV0P2BASE_RTC_H

#include <stdint.h>


namespace OTV0P2BASE
{


// TODO: encapsulate state in a class for robustness, testability and optimisation.


// Select cadence of main system tick.
// Simple alternatives are 0.5Hz, 1Hz, 2Hz (based on async timer 2 clock).
// Slower may allow lower energy consumption.
// Faster may may some timing requirements, such as FS20 TX timing, easier.
// V0p2 boards have traditionally been on 0.5Hz (2s main loop time) cadence.
#define V0P2BASE_TWO_S_TICK_RTC_SUPPORT // Wake up every 2 seconds.


// Number of minutes per day.
static const uint16_t MINS_PER_DAY = 1440;

// Seconds for local time (and assumed UTC) in range [0,59].
// Volatile to allow for async update.
// Maintained locally or shadowed from external RTC.
// Read and write accesses assumed effectively atomic.
// NOT FOR DIRECT ACCESS OUTSIDE RTC ROUTINES.
extern volatile uint_fast8_t _secondsLT;

// Minutes since midnight for local time in range [0,1439].
// Must be accessed with interrupts disabled and as if volatile.
// Maintained locally or shadowed from external RTC.
// NOT FOR DIRECT ACCESS OUTSIDE RTC ROUTINES.
extern volatile uint_least16_t _minutesSinceMidnightLT;

// Whole days since the start of 2000/01/01 (ie the midnight between 1999 and 2000), local time.
// Must be accessed with interrupts disabled and as if volatile.
// This will roll in about 2179.
// NOT FOR DIRECT ACCESS OUTSIDE RTC ROUTINES.
extern volatile uint_least16_t _daysSince1999LT;




// Persist software RTC information to non-volatile (EEPROM) store.
// This does not attempt to store full precision of time down to seconds,
// but enough to help avoid the clock slipping too much during (say) a battery change.
// There is no point calling this more than (say) once per minute,
// though it will simply return relatively quickly from redundant calls.
// The RTC data is stored so as not to wear out AVR EEPROM for at least several years.
void persistRTC();

// Restore software RTC information from non-volatile (EEPROM) store, if possible.
// Returns true if the persisted data seemed valid and was restored, in full or part.
bool restoreRTC();


// Get local time seconds from RTC [0,59].
// Is as fast as reasonably practical.
// Thread-safe and ISR-safe: returns a consistent atomic snapshot.
static inline uint_fast8_t getSecondsLT() { return(_secondsLT); } // Assumed atomic.

// Get local time minutes from RTC [0,59].
// Relatively slow.
// Thread-safe and ISR-safe.
uint_least8_t getMinutesLT();

// Get local time hours from RTC [0,23].
// Relatively slow.
// Thread-safe and ISR-safe.
uint_least8_t getHoursLT();

// Get minutes since midnight local time [0,1439].
// Useful to fetch time atomically for scheduling purposes.
// Thread-safe and ISR-safe.
uint_least16_t getMinutesSinceMidnightLT();

// Get whole days since the start of 2000/01/01 (ie the midnight between 1999 and 2000), local time.
// This will roll in about 2179.
// Thread-safe and ISR-safe.
uint_least16_t getDaysSince1999LT();

// Get previous hour in current local time, wrapping round from 0 to 23.
uint_least8_t getPrevHourLT();
// Get next hour in current local time, wrapping round from 23 back to 0.
uint_least8_t getNextHourLT();


// Set time as hours [0,23] and minutes [0,59].
// Will ignore attempts to set bad values and return false in that case.
// Returns true if all OK and the time has been set.
// Does not attempt to set seconds.
// Thread/interrupt safe, but do not call this from an ISR.
// Will persist time to survive reset as necessary.
bool setHoursMinutesLT(uint8_t hours, uint8_t minutes);

// Set nominal seconds [0,59].
// Not persisted, may be offset from real time.
// Will ignore attempts to set bad values and return false in that case.
// Will drop the least significant bit if counting in 2s increments.
// Returns true if all OK and the time has been set.
// Thread/interrupt safe, but do not call this from an ISR.
bool setSeconds(uint8_t seconds);


// Length of main loop and wakeup cycle/tick in seconds.
#if defined(V0P2BASE_TWO_S_TICK_RTC_SUPPORT)
static const uint_fast8_t MAIN_TICK_S = 2;
#else
static const uint_fast8_t MAIN_TICK_S = 1;
#endif


// RTC-based watchdog, if enabled with enableRTCWatchdog(true),
// will force a reset if the resetRTCWatchDog() is not called
// between one RTC tick interrupt and the next.
//
// One possible usage: as start of each major tick in main loop,
// call resetRTCWatchDog() immediately followed by enableRTCWatchdog(true).
//
// If true, then enable the RTC-based watchdog; disable otherwise.
void enableRTCWatchdog(bool enable);
// Must be called between each 'tick' of the RTC clock if enabled, else system will reset.
void resetRTCWatchDog();


}
#endif
