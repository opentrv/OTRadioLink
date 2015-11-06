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
 EEPROM space allocation and utilities including some of the simple rolling stats management.

 NOTE: NO EEPROM ACCESS SHOULD HAPPEN FROM ANY ISR CODE ELSE VARIOUS FAILURE MODES ARE POSSIBLE
 */


#include <util/atomic.h>

#include "OTV0P2BASE_EEPROM.h"

#include "OTV0P2BASE_RTC.h"


namespace OTV0P2BASE
{


// Updates an EEEPROM byte iff not currently already at the specified target value.
// May be able to selectively erase or write (ie reduce wear) to reach the desired value.
// As with the AVR eeprom_XXX_byte() macros, not safe to use outside and within ISRs as-is.
// Returns true iff an erase and/or write was performed.
// TODO: make smarter, eg don't wait/read twice...
bool eeprom_smart_update_byte(uint8_t *p, uint8_t value)
  {
  // If target byte is 0xff then attempt smart erase rather than more generic write or erase+write.
  if((uint8_t) 0xff == value) { return(eeprom_smart_erase_byte(p)); }
  // More than an erase might be required...
  const uint8_t oldValue = eeprom_read_byte(p);
  if(value == oldValue) { return(false); } // No change needed.
#ifdef V0P2BASE_EEPROM_SPLIT_ERASE_WRITE // Can do selective write.
  if(value == (value & oldValue)) { return(eeprom_smart_clear_bits(p, value)); } // Can use pure write to clear bits to zero.
#endif
  eeprom_write_byte(p, value); // Needs to set some (but not all) bits to 1, so needs erase and write.
  return(true); // Performed an update.
  }

// Erases (sets to 0xff) the specified EEPROM byte, avoiding a following (redundant) write if possible.
// If the target byte is already 0xff then this does nothing at all beyond an initial read.
// This saves a bit of time and power and possibly a little EEPROM cell wear also.
// Without split erase/write this degenerates to a specialised eeprom_update_byte().
// As with the AVR eeprom_XXX_byte() macros, not safe to use outside and within ISRs as-is.
// Returns true iff an erase was performed.
bool eeprom_smart_erase_byte(uint8_t *p)
  {
#ifndef V0P2BASE_EEPROM_SPLIT_ERASE_WRITE // No split erase/write so do as a slightly smart update...
  if((uint8_t) 0xff == eeprom_read_byte(p)) { return(false); } // No change/erase needed.
  eeprom_write_byte(p, 0xff); // Set to 0xff.
  return(true); // Performed an erase (and probably a write, too).
#else

  // Wait until EEPROM is idle/ready.
  eeprom_busy_wait();

  // Following block equivalent to:
  //     if((uint8_t) 0xff == eeprom_read_byte(p)) { return; }
  // but leaves EEAR[L] set up appropriately for any erase or write.
#if E2END <= 0xFF
  EEARL = (uint8_t)p;
#else
  EEAR = (uint16_t)p;
#endif
  // Ignore problems that some AVRs have with EECR and STS instructions (ATmega64 errata).
  EECR = _BV(EERE); // Start EEPROM read operation.
  const uint8_t oldValue = EEDR; // Get old EEPROM value.
  if((uint8_t) 0xff != oldValue) // Needs erase...
    {
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE) // Avoid timing problems from interrupts.
      {
      // Erase to 0xff; no write needed.
      EECR = _BV(EEMPE) | _BV(EEPM0); // Set master write-enable bit and erase-only mode.
      EECR |= _BV(EEPE);  // Start erase-only operation.
      }
    return(true); // Performed the erase.
    }
  return(false);
#endif
  }

// ANDs the supplied mask into the specified EEPROM byte, avoiding an initial (redundant) erase if possible.
// This can be used to ensure that specific bits are 0 while leaving others untouched.
// If ANDing in the mask has no effect then this does nothing at all beyond an initial read.
// This saves a bit of time and power and possibly a little EEPROM cell wear also.
// Without split erase/write this degenerates to a specialised eeprom_smart_update_byte().
// As with the AVR eeprom_XXX_byte() macros, not safe to use outside and within ISRs as-is.
// Returns true iff a write was performed.
bool eeprom_smart_clear_bits(uint8_t *p, uint8_t mask)
  {
#ifndef V0P2BASE_EEPROM_SPLIT_ERASE_WRITE // No split erase/write so do as a slightly smart update...
  const uint8_t oldValue = eeprom_read_byte(p);
  const uint8_t newValue = oldValue & mask;
  if(oldValue == newValue) { return(false); } // No change/write needed.
  eeprom_write_byte(p, newValue); // Set to masked value.
  return(true); // Performed a write (and probably an erase, too).
#else

  // Wait until EEPROM is idle/ready.
  eeprom_busy_wait();

  // Following block equivalent to:
  //     oldValue = eeprom_read_byte(p);
  // and leaves EEAR[L] set up appropriately for any erase or write.
#if E2END <= 0xFF
  EEARL = (uint8_t)p;
#else
  EEAR = (uint16_t)p;
#endif
  // Ignore problems that some AVRs have with EECR and STS instructions (ATmega64 errata).
  EECR = _BV(EERE); // Start EEPROM read operation.
  const uint8_t oldValue = EEDR; // Get old EEPROM value.
  const uint8_t newValue = oldValue & mask;
  if(oldValue != newValue) // Write is needed...
    {
    // Do the write: no erase is needed.
    EEDR = newValue; // Set EEPROM data register to required new value.
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE) // Avoid timing problems from interrupts.
      {
      EECR = _BV(EEMPE) | _BV(EEPM1); // Set master write-enable bit and write-only mode.
      EECR |= _BV(EEPE);  // Start write-only operation.
      }
    return(true); // Performed the write.
    }
  return(false);
#endif
  }


// Get raw stats value for hour HH [0,23] from stats set N from non-volatile (EEPROM) store.
// A value of 0xff (255) means unset (or out of range); other values depend on which stats set is being used.
// The stats set is determined by the order in memory.
uint8_t getByHourStat(uint8_t hh, uint8_t statsSet)
  {
  if(statsSet > (V0P2BASE_EE_END_STATS - V0P2BASE_EE_START_STATS) / V0P2BASE_EE_STATS_SET_SIZE) { return(STATS_UNSET_BYTE); } // Invalid set.
  if(hh > 23) { return((uint8_t) 0xff); } // Invalid hour.
  return(eeprom_read_byte((uint8_t *)(V0P2BASE_EE_START_STATS + (statsSet * (int)V0P2BASE_EE_STATS_SET_SIZE) + (int)hh)));
  }

// Get minimum sample from given stats set ignoring all unset samples; STATS_UNSET_BYTE if all samples are unset.
uint8_t getMinByHourStat(const uint8_t statsSet)
  {
  if(statsSet > (V0P2BASE_EE_END_STATS - V0P2BASE_EE_START_STATS) / V0P2BASE_EE_STATS_SET_SIZE) { return(STATS_UNSET_BYTE); } // Invalid set.
  uint8_t result = STATS_UNSET_BYTE;
  for(uint8_t hh = 24; hh-- != 0; )
    {
    const uint8_t v = eeprom_read_byte((uint8_t *)(V0P2BASE_EE_START_STATS + (statsSet * (int)V0P2BASE_EE_STATS_SET_SIZE) + (int)hh));
    // Optimisation/cheat: all valid samples are less than STATS_UNSET_BYTE.
    if(v < result) { result = v; }
    }
  return(result);
  }

// Get maximum sample from given stats set ignoring all unset samples; STATS_UNSET_BYTE if all samples are unset.
uint8_t getMaxByHourStat(const uint8_t statsSet)
  {
  if(statsSet > (V0P2BASE_EE_END_STATS - V0P2BASE_EE_START_STATS) / V0P2BASE_EE_STATS_SET_SIZE) { return(STATS_UNSET_BYTE); } // Invalid set.
  uint8_t result = STATS_UNSET_BYTE;
  for(uint8_t hh = 24; hh-- != 0; )
    {
    const uint8_t v = eeprom_read_byte((uint8_t *)(V0P2BASE_EE_START_STATS + (statsSet * (int)V0P2BASE_EE_STATS_SET_SIZE) + (int)hh));
    if((STATS_UNSET_BYTE != v) &&
       ((STATS_UNSET_BYTE == result) || (v > result)))
      { result = v; }
    }
  return(result);
  }

// Returns true iff there is a full set of stats (none unset) and this 3/4s of the values are higher than the supplied sample.
// Always returns false if all samples are the same.
//   * s is start of (24) sample set in EEPROM
//   * sample to be tested for being in lower quartile
bool inBottomQuartile(const uint8_t *sE, const uint8_t sample)
  {
  uint8_t valuesHigher = 0;
  for(int8_t hh = 24; --hh >= 0; ++sE)
    {
    const uint8_t v = eeprom_read_byte(sE);
    if(OTV0P2BASE::STATS_UNSET_INT == v) { return(false); } // Abort if not a full set of stats (eg at least one full day's worth).
    if(v > sample) { if(++valuesHigher >= 18) { return(true); } } // Stop as soon as known to be in lower quartile.
    }
  return(false); // Not in lower quartile.
  }

// Returns true iff there is a full set of stats (none unset) and this 3/4s of the values are lower than the supplied sample.
// Always returns false if all samples are the same.
//   * s is start of (24) sample set in EEPROM
//   * sample to be tested for being in lower quartile
bool inTopQuartile(const uint8_t *sE, const uint8_t sample)
  {
  uint8_t valuesLower = 0;
  for(int8_t hh = 24; --hh >= 0; ++sE)
    {
    const uint8_t v = eeprom_read_byte(sE);
    if(OTV0P2BASE::STATS_UNSET_INT == v) { return(false); } // Abort if not a full set of stats (eg at least one full day's worth).
    if(v < sample) { if(++valuesLower >= 18) { return(true); } } // Stop as soon as known to be in upper quartile.
    }
  return(false); // Not in upper quartile.
  }

// Returns true if specified hour is (conservatively) in the specified outlier quartile for the specified stats set.
// Returns false if a full set of stats not available, eg including the specified hour.
// Always returns false if all samples are the same.
//   * inTop  test for membership of the top quartile if true, bottom quartile if false
//   * statsSet  stats set number to use.
//   * hour  hour of day to use or ~0 for current hour.
bool inOutlierQuartile(const uint8_t inTop, const uint8_t statsSet, const uint8_t hour)
  {
  if(statsSet >= V0P2BASE_EE_STATS_SETS) { return(false); } // Bad stats set number, ie unsafe.
  const uint8_t hh = (inOutlierQuartile_CURRENT_HOUR == hour) ? OTV0P2BASE::getHoursLT() :
    ((hour > 23) ? OTV0P2BASE::getNextHourLT() : hour);
  const uint8_t *ss = (uint8_t *)(V0P2BASE_EE_STATS_START_ADDR(statsSet));
  const uint8_t sample = eeprom_read_byte(ss + hh);
  if(OTV0P2BASE::STATS_UNSET_INT == sample) { return(false); }
  if(inTop) { return(inTopQuartile(ss, sample)); }
  return(inBottomQuartile(ss, sample));
  }


// Clear all collected statistics, eg when moving device to a new room or at a major time change.
// Requires 1.8ms per byte for each byte that actually needs erasing.
//   * maxBytesToErase limit the number of bytes erased to this; strictly positive, else 0 to allow 65536
// Returns true if finished with all bytes erased.
bool zapStats(uint16_t maxBytesToErase)
  {
  for(uint8_t *p = (uint8_t *)V0P2BASE_EE_START_STATS; p <= (uint8_t *)V0P2BASE_EE_END_STATS; ++p)
    { if(OTV0P2BASE::eeprom_smart_erase_byte(p)) { if(--maxBytesToErase == 0) { return(false); } } } // Stop if out of time...
  return(true); // All done.
  }


}
