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

#ifdef ARDUINO_ARCH_AVR
#include <util/crc16.h>
#endif

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "OTV0P2BASE_Entropy.h"

#include "OTV0P2BASE_ADC.h"
#include "OTV0P2BASE_QuickPRNG.h"
#include "OTV0P2BASE_Sleep.h"

#include "OTV0P2BASE_Serial_IO.h"


namespace OTV0P2BASE
{


#ifdef ARDUINO_ARCH_AVR

// Extract and return a little entropy from clock jitter between CPU and 32768Hz RTC clocks; possibly up to 2 bits of entropy captured.
// Expensive in terms of CPU time and thus energy.
// TODO: may be able to reduce clock speed a little to lower energy cost while still detecting useful jitter
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
  seedRNG8(data ^ ++count8, getCPUCycleCount(), getSubCycleTime());
  }

// Capture a little system entropy, effectively based on call timing.
// This call should typically take << 1ms at 1MHz CPU.
// Does not change CPU clock speeds, mess with interrupts (other than possible brief blocking), or do I/O, or sleep.
// Should inject some noise into secure (TBD) and non-secure (RNG8) PRNGs.
void captureEntropy1()
//  { OTV0P2BASE::seedRNG8(_getSubCycleTime() ^ _adcNoise, getCPUCycleCount() ^ Supply_mV.get(), _watchdogFired); } // FIXME
  { OTV0P2BASE::seedRNG8(TCNT2, getCPUCycleCount() /* ^ Supply_mV.get() */, 42 /*_watchdogFired*/); } // FIXME


// Compute a CRC of all of SRAM as a hash that should contain some entropy, especially after power-up.
#if !defined(RAMSTART)
#define RAMSTART (0x100)
#endif
uint16_t sramCRC()
  {
  uint16_t result = ~0U;
  for(uint8_t *p = (uint8_t *)RAMSTART; p <= (uint8_t *)RAMEND; ++p)
    { result = _crc_ccitt_update(result, *p); }
  return(result);
  }

// Compute a CRC of all of EEPROM as a hash that may contain some entropy, particularly across restarts.
uint16_t eeCRC()
  {
  uint16_t result = ~0U;
  for(uint8_t *p = (uint8_t *)0; p <= (uint8_t *)E2END; ++p)
    {
    const uint8_t v = eeprom_read_byte(p);
    result = _crc_ccitt_update(result, v);
    }
  return(result);
  }

// Seed PRNGs and entropy pool.
// Scrapes entropy from SRAM and EEPROM and some I/O (safely).
// Call this early in boot, but possibly after gathering initial data from some sensors,
// entropy from which can be scraped out of SRAM.
void seedPRNGs()
  {
  // Seed PRNG(s) with available environmental values and clock time/jitter for some entropy.
  // Also sweeps over SRAM and EEPROM (see RAMEND and E2END), especially for non-volatile state and uninitialised areas of SRAM.
  // TODO: add better PRNG with entropy pool (eg for crypto).
  // TODO: add RFM22B WUT clock jitter, RSSI, temperature and battery voltage measures.
  const uint16_t srseed = OTV0P2BASE::sramCRC();
  const uint16_t eeseed = OTV0P2BASE::eeCRC();
//  // DHD20130430: maybe as much as 16 bits of entropy on each reset in seed1, when all sensor inputs used, concentrated in the least-significant bits.
//  const uint16_t s16 = (__DATE__[5]) ^
////                       Vcc ^
////                       (intTempC16 << 1) ^
////#if !defined(ALT_MAIN_LOOP)
////                       (heat << 2) ^
////#if defined(TEMP_POT_AVAILABLE)
////                       ((((uint16_t)tempPot) << 3) + tempPot) ^
////#endif
////                       (AmbLight.get() << 4) ^
////#if defined(HUMIDITY_SENSOR_SUPPORT)
////                       ((((uint16_t)rh) << 8) - rh) ^
////#endif
////#endif
//                       (OTV0P2BASE::getMinutesSinceMidnightLT() << 5) ^
//                       (((uint16_t)OTV0P2BASE::getSubCycleTime()) << 6);
  const uint8_t s8 = (__DATE__[5]) ^
                       OTV0P2BASE::getMinutesSinceMidnightLT() ^
                       ((uint16_t)OTV0P2BASE::getSubCycleTime());
  // Seed simple/fast/small built-in PRNG.  (Smaller and faster than srandom()/random().)
  const uint8_t nar1 = OTV0P2BASE::noisyADCRead();
  OTV0P2BASE::seedRNG8(nar1 ^ s8, /* oldResetCount - */ (uint8_t)(eeseed >> 8), ::OTV0P2BASE::clockJitterWDT() ^ (uint8_t)srseed);
#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("nar ");
  DEBUG_SERIAL_PRINTFMT(nar1, BIN);
  DEBUG_SERIAL_PRINTLN();
#endif
  // TODO: seed other/better PRNGs.
  // Feed in mainly persistent/non-volatile state explicitly.
  OTV0P2BASE::addEntropyToPool(eeseed, 0);
  OTV0P2BASE::addEntropyToPool(s8, 0);
  for(uint8_t i = 0; i < V0P2BASE_EE_LEN_SEED; ++i)
    { OTV0P2BASE::addEntropyToPool(eeprom_read_byte((uint8_t *)(V0P2BASE_EE_START_SEED + i)), 0); }
  OTV0P2BASE::addEntropyToPool(OTV0P2BASE::noisyADCRead(), 1); // Conservative first push of noise into pool.
  // Carry a few bits of entropy over a reset by picking one of the four designated EEPROM bytes at random;
  // if zero, erase to 0xff, else AND in part of the seed including some of the previous EEPROM hash (and write).
  // This amounts to about a quarter of an erase/write cycle per reset/restart per byte, or 400k restarts endurance!
  // These 4 bytes should be picked up as part of the hash/CRC of EEPROM above, next time,
  // essentially forming a longish-cycle (poor) PRNG even with little new real entropy each time.
  uint8_t *const erp = (uint8_t *)(V0P2BASE_EE_START_SEED + (3&((s8)^((eeseed>>9)+(__TIME__[7]))))); // Use some new and some eeseed bits to choose which byte to update.
  const uint8_t erv = eeprom_read_byte(erp);
  if(0 == erv) { OTV0P2BASE::eeprom_smart_erase_byte(erp); }
  else
    {
    OTV0P2BASE::eeprom_smart_clear_bits(erp,
//#if !defined(NO_clockJitterEntropyByte)
      ::OTV0P2BASE::clockJitterEntropyByte()
//#else
//      (::OTV0P2BASE::clockJitterWDT() ^ nar1) // Less good fall-back when clockJitterEntropyByte() not available with more actual entropy.
//#endif
      ^ ((uint8_t)eeseed)); // Nominally include disjoint set of eeseed bits in choice of which to clear.
    }

//  serialPrintAndFlush(F("srseed "));
//  serialPrintAndFlush(srseed, BIN);
//  serialPrintlnAndFlush();

#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("srseed ");
  DEBUG_SERIAL_PRINTFMT(srseed, BIN);
  DEBUG_SERIAL_PRINTLN();
  DEBUG_SERIAL_PRINT_FLASHSTRING("eeseed ");
  DEBUG_SERIAL_PRINTFMT(eeseed, BIN);
  DEBUG_SERIAL_PRINTLN();
  DEBUG_SERIAL_PRINT_FLASHSTRING("RNG8 ");
  DEBUG_SERIAL_PRINTFMT(randRNG8(), BIN);
  DEBUG_SERIAL_PRINTLN();
  DEBUG_SERIAL_PRINT_FLASHSTRING("erv ");
  DEBUG_SERIAL_PRINTFMT(erv, BIN);
  DEBUG_SERIAL_PRINTLN();
#endif
  }

#endif // ARDUINO_ARCH_AVR


}
