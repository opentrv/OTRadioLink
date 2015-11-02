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
 Routines for managing entropy for (crypto) random number generation.
 */

#include <util/crc16.h>
#include <Arduino.h>

#include "OTV0P2BASE_Entropy.h"

#include "OTV0P2BASE_QuickPRNG.h"
#include "OTV0P2BASE_ADC.h"


namespace OTV0P2BASE
{


// Extract and return a little entropy from clock jitter between CPU and 32768Hz RTC clocks; possibly up to 2 bits of entropy captured.
// Expensive in terms of CPU time and thus energy.
// TODO: may be able to reduce clock speed at little to lower energy cost while still detecting useful jitter
//   (but not below 131072kHz since CPU clock must be >= 4x RTC clock to stay on data-sheet and access TCNT2).
uint_fast8_t clockJitterRTC()
  {
  const uint8_t t0 = TCNT2;
  while(t0 == TCNT2) { }
  uint_fast8_t count = 0;
  const uint8_t t1 = TCNT2;
  while(t1 == TCNT2) { ++count; } // Effectively count CPU cycles in one RTC sub-cycle tick.
  return(count);
  }


// Counter to help whiten getSecureRandomByte() output.
static uint8_t count8;

// Generate 'secure' new random byte.
// This should be essentially all entropy and unguessable.
// Likely to be slow and may force some peripheral I/O.
// Runtime details are likely to be intimately dependent on hardware implementation.
// Not thread-/ISR- safe.
//  * whiten  if true whiten the output a little more, but little or no extra entropy is added;
//      if false then it is easier to test if the underlying source provides new entropy reliably
uint8_t getSecureRandomByte(const bool whiten)
  {
//#ifdef WAKEUP_32768HZ_XTAL
  // Use various real noise sources and whiten with PRNG and other counters.
  // Mix the bits also to help ensure good distribution.
  uint8_t w1 = OTV0P2BASE::clockJitterEntropyByte(); // Real noise.
//#else // WARNING: poor substitute if 32768Hz xtal not available.
//  uint8_t w1 = clockJitterWDT() + (::OTV0P2BASE::clockJitterWDT() << 5);
//  w1 ^= (w1 << 1); // Mix.
//  w1 ^= ::OTV0P2BASE::clockJitterWDT();
//  w1 ^= (w1 >> 2); // Mix.
//  w1 ^= ::OTV0P2BASE::clockJitterWDT();
//  w1 ^= (w1 << 2); // Mix.
//  w1 ^= ::OTV0P2BASE::clockJitterWDT();
//  w1 ^= (w1 >> 3); // Mix.
//  w1 ^= ::OTV0P2BASE::clockJitterWDT();
//  w1 ^= (w1 << 2); // Mix.
//  w1 ^= ::OTV0P2BASE::clockJitterWDT();
//  w1 ^= (w1 >> 1); // Mix.
//  w1 ^= ::OTV0P2BASE::clockJitterWDT();
//#endif
  const uint8_t v1 = w1;
  w1 ^= (w1 << 3); // Mix.
  w1 ^= OTV0P2BASE::noisyADCRead(true); // Some more real noise, possibly ~1 bit.
  w1 ^= (w1 << 4); // Mix.
  const uint8_t v2 = w1;
  w1 ^= OTV0P2BASE::clockJitterWDT(); // Possibly ~1 bit more of entropy.
  w1 ^= (w1 >> 4); // Mix.
  if(whiten)
    {
    w1 ^= OTV0P2BASE::randRNG8(); // Whiten.
    w1 ^= (w1 << 3); // Mix.
    w1 ^= _crc_ibutton_update(/*cycleCountCPU() ^ FIXME */ (uint8_t)(intptr_t)&v1, --count8 - (uint8_t)(intptr_t)&v2); // Whiten.
    }
  w1 ^= _crc_ibutton_update(v1, v2); // Complex hash.
  return(w1);
  }

// Add entropy to the pool, if any, along with an estimate of how many bits of real entropy are present.
//   * data   byte containing 'random' bits.
//   * estBits estimated number of truely securely random bits in range [0,8].
// Not thread-/ISR- safe.
void addEntropyToPool(const uint8_t data, const uint8_t /*estBits*/)
  {
  // TODO: no real entropy pool yet.
  //seedRNG8(data, cycleCountCPU(), getSubCycleTime()); // FIXME
  seedRNG8(data, --count8, TCNT2); // cycleCountCPU(), getSubCycleTime()); // FIXME
  }

// Capture a little system entropy, effectively based on call timing.
// This call should typically take << 1ms at 1MHz CPU.
// Does not change CPU clock speeds, mess with interrupts (other than possible brief blocking), or do I/O, or sleep.
// Should inject some noise into secure (TBD) and non-secure (RNG8) PRNGs.
void captureEntropy1()
//  { OTV0P2BASE::seedRNG8(_getSubCycleTime() ^ _adcNoise, cycleCountCPU() ^ Supply_mV.get(), _watchdogFired); } // FIXME
  { OTV0P2BASE::seedRNG8(TCNT2, 69 /* cycleCountCPU() ^ Supply_mV.get() */, 42 /*_watchdogFired*/); } // FIXME


}
