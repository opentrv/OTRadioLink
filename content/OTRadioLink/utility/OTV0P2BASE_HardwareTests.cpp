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
 Hardware tests for general POST (power-on self tests)
 and for detailed hardware diagnostics.

 Some are generic such as testing clock behaviour,
 others will be very specific to some board revisions
 (eg looking for shorts or testing expected attached hardware).

 Most should return true on success and false on failure.

 Some may require being passed a Print reference
 (which will often be an active hardware serial connection)
 to dump diagnostics to.
 */

#ifndef OTV0P2BASE_HARDWARETESTS_H
#define OTV0P2BASE_HARDWARETESTS_H

#ifdef ARDUINO_ARCH_AVR
#include "util/atomic.h"
#endif

#include "OTV0P2BASE_Entropy.h"
#include "OTV0P2BASE_Sleep.h"

#include "OTV0P2BASE_HardwareTests.h"


namespace OTV0P2BASE
{
namespace HWTEST
{


#ifdef ARDUINO_ARCH_AVR
// Returns true if the 32768Hz low-frequency async crystal oscillator appears to be running.
// This means the the Timer 2 clock needs to be running
// and have an acceptable frequency compared to the CPU clock (1MHz).
// Uses nap, and needs the Timer 2 to have been set up in async clock mode.
// In passing gathers some entropy for the system.
bool check32768HzOsc()
    {
    // Check that the 32768Hz async clock is actually running at least somewhat.
    const uint8_t initialSCT = OTV0P2BASE::getSubCycleTime();

    // Allow time for 32768Hz crystal to start reliably, see: http://www.atmel.com/Images/doc1259.pdf
#if 0 && defined(DEBUG)
    DEBUG_SERIAL_PRINTLN_FLASHSTRING("Sleeping to let 32768Hz clock start...");
#endif
    // Time spent here should not be a whole multiple of basic cycle time
    // to avoid a spuriously-stationary async clock reading!
    // Allow several seconds (~3s+) to start.
    // Attempt to capture some entropy while waiting,
    // implicitly from oscillator start-up time if nothing else.
    for(uint8_t i = 255; --i > 0; )
        {
        const uint8_t sct = OTV0P2BASE::getSubCycleTime();
        OTV0P2BASE::addEntropyToPool(sct, 0);
        // If counter has incremented/changed (twice) then assume probably OK.
        if((sct != initialSCT) && (sct != (uint8_t)(initialSCT+1))) { return(true); }
        // Ensure lower bound of ~3s until loop finishes.
        OTV0P2BASE::nap(WDTO_15MS);
        }

#if 0 && defined(DEBUG)
    DEBUG_SERIAL_PRINTLN_FLASHSTRING("32768Hz clock may not be running!");
#endif
    return(false); // FAIL // panic(F("Xtal")); // Async clock not running.
    }
#endif
#endif

#ifdef ARDUINO_ARCH_AVR
// Returns true if the 32768Hz low-frequency async crystal oscillator appears to be running and sane.
// Performs an extended test that the CPU (RC) and crystal frequencies are in a sensible ratio.
// This means the the Timer 2 clock needs to be running
// and have an acceptable frequency compared to the CPU clock (1MHz).
// Uses nap, and needs the Timer 2 to have been set up in async clock mode.
// In passing gathers some entropy for the system.
bool check32768HzOscExtended()
    {
    // Check that the slow clock appears to be running.
    if(!check32768HzOsc()) { return(false); }

    // Test low frequency oscillator vs main CPU clock oscillator (at 1MHz).
    // Tests clock frequency between 15 ms naps between for up to 30 cycles and fails if not within bounds.
    // As of 2016-02-16, all working REV7s give count >= 120 and that fail to program via bootloader give count <= 119
    // REV10 gives 119-120 (only one tested though).
    static const uint8_t optimalLFClock = 122; // May be optimal...
    static const uint8_t errorLFClock = 4; // Max drift from allowed value.
    uint8_t count = 0;
    for(uint8_t i = 0; ; i++)
        {
        ::OTV0P2BASE::nap(WDTO_15MS);
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
            // Wait for edge on xtal counter edge.
            // Start counting cycles.
            // On next edge, stop.
            const uint8_t t0 = TCNT2;
            while(t0 == TCNT2) {}
            const uint8_t t01 = TCNT0;
            const uint8_t t1 = TCNT2;
            while(t1 == TCNT2) {}
            const uint8_t t02 = TCNT0;
            count = t02-t01;
            }
      // Check end conditions.
      if((count < optimalLFClock+errorLFClock) & (count > optimalLFClock-errorLFClock)) { break; }
      if(i > 30) { return(false); } // FAIL { panic(F("xtal")); }
      // Capture some entropy from the (chaotic?) clock wobble, but don't claim any.  (TODO-800)
      OTV0P2BASE::addEntropyToPool(count, 0);

#if 0 && defined(DEBUG)
      // Optionally print value to debug.
      DEBUG_SERIAL_PRINT_FLASHSTRING("Xtal freq check: ");
      DEBUG_SERIAL_PRINT(count);
      DEBUG_SERIAL_PRINTLN();
#endif
      }

    return(true); // Success!
    }
#endif // ARDUINO_ARCH_AVR

#ifdef ARDUINO_ARCH_AVR

/**
 * @brief	Calibrate the internal RC oscillator against and external crystal oscillator or resonator.
 * @param   todo do we want settable stuff, e.g. ext osc rate, internal osc rate, etc?
 * @retval  True on calibration success.
 */
bool calibrateInternalOscWithExtOsc()
{
    // todo these should probably go somewhere else but not sure where.
	const constexpr uint8_t maxTries = 128;  // Maximum number of values to attempt.
	const constexpr uint8_t initOscCal = 0;  // Initial oscillator calibration value to start from.
	// TCNT2 overflows every 2 seconds. One tick is 2000/256 = 7.815 ms, or 7815 clock cycles at 1 MHz.
	// Minimum number of cycles we want per count is (7815*1.1)/255 = 34, to give some play in case the clock is too fast.
	const constexpr uint16_t cyclesPerTick = 7815;
	const constexpr uint8_t innerLoopTime = 36;  // the number of cycles the inner loop takes to execute.
	const constexpr uint8_t targetCount = cyclesPerTick/innerLoopTime;  // The number of counts we are aiming for.

    // Check that the slow clock appears to be running.
    if(!check32768HzOsc()) { return(false); }

    // Set initial calibration value and wait to settle.
    OSCCAL = initOscCal; // todo think about what happens if oscillator has previously been calibrated! unlikely to have wandered too much.
    _delay_x4cycles(2); // > 8 us. max oscillator settling time is 5 us.

    // Calibration routine
    for(uint8_t i = 0; i < maxTries; i++)
	{
    	uint8_t count = 0;

        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			// Wait for edge on xtal counter edge.
			const uint8_t t0 = TCNT2;
			const uint8_t t1 = TCNT2 + 1;
			while(t0 == TCNT2) {}
			// Start counting cycles.
			// todo Count the number of cycles this loop takes! Assuming 40 for now.
			do {
				count++; // 2 cycles?
				// 8*4 = 32 cycles per count. fixme (DE20161021) I don't think this takes register setup into account.
				_delay_x4cycles(8);
            // Repeat loop until TCNT2 increments.
			} while (TCNT2 == t1); // 2 cycles?
		}
        // Set new calibration value.
        if(count > targetCount) OSCCAL--;
        else if(count < targetCount) OSCCAL++;
        else return true;
        // Wait for oscillator to settle.
        _delay_x4cycles(2);

	}
}
#endif // ARDUINO_ARCH_AVR

}
}
