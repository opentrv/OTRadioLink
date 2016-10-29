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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2016
*/

/*
 EEPROM space allocation and utilities including some of the simple rolling stats management.

 NOTE: NO EEPROM ACCESS SHOULD HAPPEN FROM ANY ISR CODE ELSE VARIOUS FAILURE MODES ARE POSSIBLE

 Mainly V0p2/AVR for now.
 */

#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#endif

#include "OTV0P2BASE_EEPROM.h"

#include "OTV0P2BASE_RTC.h"


namespace OTV0P2BASE
{


#ifdef NVByHourByteStatsBase_DEFINED
// Compute new linearly-smoothed value given old smoothed value and new value.
// Guaranteed not to produce a value higher than the max of the old smoothed value and the new value.
// Uses stochastic rounding to nearest to allow nominally sub-lsb values to have an effect over time.
uint8_t NVByHourByteStatsBase::smoothStatsValue(const uint8_t oldSmoothed, const uint8_t newValue)
  {
  // Optimisation: smoothed value is unchanged if new value is the same as extant.
  if(oldSmoothed == newValue) { return(oldSmoothed); }
  // Compute and update with new stochastically-rounded exponentially-smoothed ("Brown's simple exponential smoothing") value.
  // Stochastic rounding allows sub-lsb values to have an effect over time.
  const uint8_t stocAdd = OTV0P2BASE::randRNG8() & ((1 << STATS_SMOOTH_SHIFT) - 1);
  // Do arithmetic in 16 bits to avoid over-/under- flows.
  return((uint8_t) (((((uint16_t) oldSmoothed) << STATS_SMOOTH_SHIFT) - ((uint16_t)oldSmoothed) + ((uint16_t)newValue) + stocAdd) >> STATS_SMOOTH_SHIFT));
  }
#endif


#ifdef ARDUINO_ARCH_AVR

// Updates an EEPROM byte iff not currently already at the specified target value.
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

    // Wait until EEPROM is idle/ready again,
    // ie so that the operation should be complete before returning.
    // This is important in case (eg) clocks may be meddled with,
    // the MCU put to sleep, etc.
    //
    // In the case of back-to-back operations
    // this should not actually add any delay.
    eeprom_busy_wait();

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

    // Wait until EEPROM is idle/ready again,
    // ie so that the operation should be complete before returning.
    // This is important in case (eg) clocks may be meddled with,
    // the MCU put to sleep, etc.
    //
    // In the case of back-to-back operations
    // this should not actually add any delay.
    eeprom_busy_wait();

    return(true); // Performed the write.
    }
  return(false);
#endif
  }

// EEPROM- (and Flash-) friendly single-byte unary incrementable encoding.
// A single byte can be used to hold a single value [0,8]
// such that increment requires only a write of one bit (no erase)
// and in general increasing the value up to the maximum only requires a single write.
// An erase is required only to decrease the value (eg back to zero).
// An initial EEPROM (erased) value of 0xff is mapped to zero.
// The two byte version can hold values in the range [0,16].
// Corruption can be detected if an unexpected bit pattern is encountered on decode.
// For the single byte versions, encodings are:
//  0 -> 0xff
//  1 -> 0xfe
//  2 -> 0xfc
//  3 -> 0xf8
//  4 -> 0xf0
//  5 -> 0xe0
//  6 -> 0xc0
//  7 -> 0x80
//  8 -> 0x00
//
// Decode routines return -1 in case of unexpected/invalid input patterns.
// All other (valid non-negative) return values can be safely cast to unit8_t.
int8_t eeprom_unary_1byte_decode(const uint8_t v)
    {
    switch(v)
        {
        case 0xff: return(0);
        case 0xfe: return(1);
        case 0xfc: return(2);
        case 0xf8: return(3);
        case 0xf0: return(4);
        case 0xe0: return(5);
        case 0xc0: return(6);
        case 0x80: return(7);
        case 0x00: return(8);
        default: return(-1); // ERROR
        }
    }
// Decode routines return -1 in case of unexpected/invalid input patterns.
int8_t eeprom_unary_2byte_decode(const uint8_t vm, const uint8_t vl)
    {
    if(0xff == vm) { return(eeprom_unary_1byte_decode(vl)); }
    else if(0 == vl) { return(eeprom_unary_1byte_decode(vm) + 8); }
    return(-1);
    }
//int8_t eeprom_unary_2byte_decode(uint16_t v)
//    {
//    switch(v)
//        {
//        case 0xffff: return(0);
//        case 0xfffe: return(1);
//        case 0xfffc: return(2);
//        case 0xfff8: return(3);
//        case 0xfff0: return(4);
//        case 0xffe0: return(5);
//        case 0xffc0: return(6);
//        case 0xff80: return(7);
//        case 0xff00: return(8);
//        case 0xfe00: return(9);
//        case 0xfc00: return(10);
//        case 0xf800: return(11);
//        case 0xf000: return(12);
//        case 0xe000: return(13);
//        case 0xc000: return(14);
//        case 0x8000: return(15);
//        case 0x0000: return(16);
//        default: return(-1); // ERROR
//        }
//    }


// Get raw stats value for specified hour [0,23]/current/next from stats set N from non-volatile (EEPROM) store.
// A value of 0xff (255) means unset (or out of range); other values depend on which stats set is being used.
// The stats set is determined by the order in memory.
//   * hour  hour of day to use, or ~0 for current hour, or >23 for next hour.
uint8_t getByHourStat(const uint8_t statsSet, const uint8_t hour)
  {
  if(statsSet > (V0P2BASE_EE_END_STATS - V0P2BASE_EE_START_STATS) / V0P2BASE_EE_STATS_SET_SIZE) { return(STATS_UNSET_BYTE); } // Invalid set.
//  if(hh > 23) { return((uint8_t) 0xff); } // Invalid hour.
  const uint8_t hh = (STATS_SPECIAL_HOUR_CURRENT_HOUR == hour) ? OTV0P2BASE::getHoursLT() :
    ((hour > 23) ? OTV0P2BASE::getNextHourLT() : hour);
  return(eeprom_read_byte((uint8_t *)(V0P2BASE_EE_START_STATS + (statsSet * (int)V0P2BASE_EE_STATS_SET_SIZE) + (int)hh)));
  }

// Compute the number of stats samples in specified set less than the specified value; returns -1 for invalid stats set.
// (With the UNSET value specified, count will be of all samples that have been set, ie are not unset.)
int8_t countStatSamplesBelow(const uint8_t statsSet, const uint8_t value)
  {
  if(statsSet > (V0P2BASE_EE_END_STATS - V0P2BASE_EE_START_STATS) / V0P2BASE_EE_STATS_SET_SIZE) { return(-1); } // Invalid set.
  if(0 == value) { return(0); } // Optimisation for common value.
  const uint8_t *sE = (uint8_t *)(V0P2BASE_EE_STATS_START_ADDR(statsSet));
  int8_t result = 0;
  for(int8_t hh = 24; --hh >= 0; ++sE)
    {
    const uint8_t v = eeprom_read_byte(sE);
    if(v < value) { ++result; }
    }
  return(result);
  }

// Get minimum sample from given stats set ignoring all unset samples; STATS_UNSET_BYTE if all samples are unset.
uint8_t getMinByHourStat(const uint8_t statsSet)
  {
  if(statsSet > (V0P2BASE_EE_END_STATS - V0P2BASE_EE_START_STATS) / V0P2BASE_EE_STATS_SET_SIZE) { return(STATS_UNSET_BYTE); } // Invalid set.
  uint8_t result = STATS_UNSET_BYTE;
  for(int8_t hh = 24; --hh >= 0; )
    {
    // FIXME: optimise (move multiplication out of loop)
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
  for(int8_t hh = 24; --hh >= 0; )
    {
    // FIXME: optimise (move multiplication out of loop)
    const uint8_t v = eeprom_read_byte((uint8_t *)(V0P2BASE_EE_START_STATS + (statsSet * (int)V0P2BASE_EE_STATS_SET_SIZE) + (int)hh));
    if((STATS_UNSET_BYTE != v) &&
       ((STATS_UNSET_BYTE == result) || (v > result)))
      { result = v; }
    }
  return(result);
  }

// Returns true iff there is a near-full set of stats (none unset) and 3/4s of the values are higher than the supplied sample.
// Always returns false if all samples are the same.
//   * s is start of (24) sample set in EEPROM
//   * sample to be tested for being in lower quartile
bool inBottomQuartile(const uint8_t *sE, const uint8_t sample)
  {
  uint8_t valuesHigher = 0;
  for(int8_t hh = 24; --hh >= 0; ++sE)
    {
    const uint8_t v = eeprom_read_byte(sE);
    if(OTV0P2BASE::STATS_UNSET_BYTE == v) { return(false); } // Abort if not a full set of stats (eg at least one full day's worth).
    if(v > sample) { if(++valuesHigher >= 18) { return(true); } } // Stop as soon as known to be in lower quartile.
    }
  return(false); // Not in lower quartile.
  }

// Returns true iff there is a near-full set of stats (none unset) and 3/4s of the values are lower than the supplied sample.
// Always returns false if all samples are the same.
//   * s is start of (24) sample set in EEPROM
//   * sample to be tested for being in lower quartile
bool inTopQuartile(const uint8_t *sE, const uint8_t sample)
  {
  uint8_t valuesLower = 0;
  for(int8_t hh = 24; --hh >= 0; ++sE)
    {
    const uint8_t v = eeprom_read_byte(sE);
    if(OTV0P2BASE::STATS_UNSET_BYTE == v) { return(false); } // Abort if not a full set of stats (eg at least one full day's worth).
    if(v < sample) { if(++valuesLower >= 18) { return(true); } } // Stop as soon as known to be in upper quartile.
    }
  return(false); // Not in upper quartile.
  }

// Returns true if specified hour is (conservatively) in the specified outlier quartile for the specified stats set.
// Returns false if a full set of stats not available, eg including the specified hour.
// Always returns false if all samples are the same.
//   * inTop  test for membership of the top quartile if true, bottom quartile if false
//   * statsSet  stats set number to use.
//   * hour  hour of day to use, or ~0 for current hour, or >23 for next hour.
bool inOutlierQuartile(const bool inTop, const uint8_t statsSet, const uint8_t hour)
  {
//  if(statsSet >= V0P2BASE_EE_STATS_SETS) { return(false); } // Bad stats set number, ie unsafe.
//  const uint8_t hh = (STATS_SPECIAL_HOUR_CURRENT_HOUR == hour) ? OTV0P2BASE::getHoursLT() :
//    ((hour > 23) ? OTV0P2BASE::getNextHourLT() : hour);
//  const uint8_t sample = eeprom_read_byte(ss + hh);
  // Rely on getByHourStat() to validate statsSet, returning UNSET if invalid,
  // and to deal with current/next hour if specified.
  const uint8_t sample = getByHourStat(statsSet, hour);
  if(OTV0P2BASE::STATS_UNSET_BYTE == sample) { return(false); }
  const uint8_t *ss = (uint8_t *)(V0P2BASE_EE_STATS_START_ADDR(statsSet));
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

#endif // ARDUINO_ARCH_AVR


// Range-compress an signed int 16ths-Celsius temperature to a unsigned single-byte value < 0xff.
// This preserves at least the first bit after the binary point for all values,
// and three bits after binary point for values in the most interesting mid range around normal room temperatures,
// with transitions at whole degrees Celsius.
// Input values below 0C are treated as 0C, and above 100C as 100C, thus allowing air and DHW temperature values.
uint8_t compressTempC16(const int16_t tempC16)
  {
  if(tempC16 <= 0) { return(0); } // Clamp negative values to zero.
  if(tempC16 < COMPRESSION_C16_LOW_THRESHOLD) { return(tempC16 >> 3); } // Preserve 1 bit after the binary point (0.5C precision).
  if(tempC16 < COMPRESSION_C16_HIGH_THRESHOLD)
    { return(((tempC16 - COMPRESSION_C16_LOW_THRESHOLD) >> 1) + COMPRESSION_C16_LOW_THR_AFTER); }
  if(tempC16 < COMPRESSION_C16_CEIL_VAL)
    { return(((tempC16 - COMPRESSION_C16_HIGH_THRESHOLD) >> 3) + COMPRESSION_C16_HIGH_THR_AFTER); }
  return(COMPRESSION_C16_CEIL_VAL_AFTER);
  }

// Reverses range compression done by compressTempC16(); results in range [0,100], with varying precision based on original value.
// 0xff (or other invalid) input results in STATS_UNSET_INT.
int16_t expandTempC16(const uint8_t cTemp)
  {
  if(cTemp < COMPRESSION_C16_LOW_THR_AFTER) { return(cTemp << 3); }
  if(cTemp < COMPRESSION_C16_HIGH_THR_AFTER)
    { return(((cTemp - COMPRESSION_C16_LOW_THR_AFTER) << 1) + COMPRESSION_C16_LOW_THRESHOLD); }
  if(cTemp <= COMPRESSION_C16_CEIL_VAL_AFTER)
    { return(((cTemp - COMPRESSION_C16_HIGH_THR_AFTER) << 3) + COMPRESSION_C16_HIGH_THRESHOLD); }
  return(OTV0P2BASE::STATS_UNSET_INT); // Invalid/unset input.
  }


}
