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
 Routines for managing entropy for (crypto) random number generation.

 Almost entirely specific to V0p2/AVR for now.
 */

#ifndef OTV0P2BASE_ENTROPY_H
#define OTV0P2BASE_ENTROPY_H

#include <stdint.h>


namespace OTV0P2BASE
{


// Note that implementation of routines declared here may be dispersed over multiple files
// to have access to some of the available entropy in the system.

// Extract and return a little entropy from clock jitter between CPU and WDT clocks; possibly one bit of entropy captured.
// Expensive in terms of CPU time and thus energy.
uint_fast8_t clockJitterWDT();

// Extract and return a little entropy from clock jitter between CPU and 32768Hz RTC clocks; possibly up to 2 bits of entropy captured.
// Expensive in terms of CPU time and thus energy.
uint_fast8_t clockJitterRTC();

// Combined clock jitter techniques to return approximately 8 bits (the entire result byte) of entropy efficiently on demand.
// Expensive in terms of CPU time and thus energy, though possibly more efficient than basic clockJitterXXX() routines.
uint_fast8_t clockJitterEntropyByte();

// Generate 'secure' new random byte.
// This should be essentially all entropy and unguessable.
// Likely to be slow and may force some I/O.
// Not thread-/ISR- safe.
//  * whiten  if true whiten the output a little more, but little or no extra entropy is added;
//      if false then it is easier to test if the underlying source provides new entropy reliably
uint8_t getSecureRandomByte(bool whiten = true);

// Add entropy to the pool, if any, along with an estimate of how many bits of real entropy are present.
//   * data   byte containing 'random' bits.
//   * estBits estimated number of truely securely random bits in range [0,8].
// Not thread-/ISR- safe.
void addEntropyToPool(uint8_t data, uint8_t estBits);

// Capture a little system entropy, effectively based on call timing.
// This call should typically take << 1ms at 1MHz CPU.
// Does not change CPU clock speeds, mess with interrupts (other than possible brief blocking), or do I/O, or sleep.
// Should inject some noise into secure (TBD) and non-secure (RNG8) PRNGs, or at least churn them.
void captureEntropy1();

// Compute a CRC of all of SRAM as a hash that should contain some entropy, especially after power-up.
uint16_t sramCRC();
// Compute a CRC of all of EEPROM as a hash that may contain some entropy, particularly across restarts.
uint16_t eeCRC();

// Seed PRNGs and entropy pool.
// Scrapes entropy from SRAM and EEPROM and some I/O (safely).
// Call this early in boot, but possibly after gathering initial data from some sensors,
// entropy from which can be scraped out of SRAM.
void seedPRNGs();


}
#endif
