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
 Simple rolling stats management.
 */

#include "OTV0P2BASE_Stats.h"

#include "OTV0P2BASE_QuickPRNG.h"


namespace OTV0P2BASE
{

// Helper functions
namespace {
// Check if we hare dealing with a special hour, and resolve it to the correct hour.
// Note this does not deal with invalid values of hour/currentHour
uint8_t getSpecialHour(uint8_t hour, uint8_t currentHour) {
    switch (hour) {
        case (NVByHourByteStatsBase::SPECIAL_HOUR_CURRENT_HOUR): {
            return currentHour;
        }
        case (NVByHourByteStatsBase::SPECIAL_HOUR_NEXT_HOUR): {
            // Taken from logic in OTV0P2BASE::getNextHourLT()
            return (currentHour >= 23) ? 0 : (currentHour + 1);
        }
        case (NVByHourByteStatsBase::SPECIAL_HOUR_PREV_HOUR): {
            // Taken from logic in OTV0P2BASE::getPrevHourLT()
            return (0 == currentHour) ? 23 : (currentHour - 1);
        }
        default: break;
    }
    return (hour);
}
}
// Get raw stats value for specified hour [0,23]/current/next from stats set N from non-volatile (EEPROM) store.
// A value of STATS_UNSET_BYTE (0xff (255)) means unset (or out of range, or invalid); other values depend on which stats set is being used.
//   * hour  hour of day to use, or ~0/0xff for current hour (default), 0xfe for next hour, or 0xfd for the previous hour.
//           If the hour is invalid, an UNSET_BYTE will be returned.
// Note the three special values that implicitly make use of the RTC to select the hour to read.
uint8_t NVByHourByteStatsBase::getByHourStatRTC(uint8_t statsSet, uint8_t hour) const
{
    const uint8_t hh {getSpecialHour(hour, getHour())};
    // The invalid cases for statsSet and hh are checked in getByHourStatSimple.
    return(getByHourStatSimple(statsSet, hh));
}


// Compute new linearly-smoothed value given old smoothed value and new value.
// Guaranteed not to produce a value higher than the max of the old smoothed value and the new value.
// Uses stochastic rounding to nearest to allow nominally sub-lsb values to have an effect over time.
uint8_t NVByHourByteStatsBase::smoothStatsValue(const uint8_t oldSmoothed, const uint8_t newValue)
  {
  // Optimisation: smoothed value is unchanged if new value is same as extant.
  if(oldSmoothed == newValue) { return(oldSmoothed); }
  // Compute and update with new stochastically-rounded exponentially-smoothed ("Brown's simple exponential smoothing") value.
  // Stochastic rounding allows sub-lsb values to have an effect over time.
  const uint8_t stocAdd = OTV0P2BASE::randRNG8() & ((1 << STATS_SMOOTH_SHIFT) - 1);
  // Do arithmetic in 16 bits to avoid over-/under- flows.
  return((uint8_t) (((((uint16_t) oldSmoothed) << STATS_SMOOTH_SHIFT) - ((uint16_t)oldSmoothed) + ((uint16_t)newValue) + stocAdd) >> STATS_SMOOTH_SHIFT));
  }

// Compute the number of stats samples in specified set less than the specified value; returns 0 for invalid stats set.
// (With the UNSET value specified, count will be of all samples that have been set, ie are not unset.)
uint8_t NVByHourByteStatsBase::countStatSamplesBelow(const uint8_t statsSet, const uint8_t value) const
  {
  if(0 == value) { return(0); } // Optimisation for common value.
  uint8_t result = 0;
  for(int8_t hh = 24; --hh >= 0; )
    {
    const uint8_t v = getByHourStatSimple(statsSet, hh);
    // Implicitly, since UNSET_BYTE is max uint8_t value, no unset values get counted.
    if(v < value) { ++result; }
    }
  return(result);
  }

// Get minimum sample from given stats set ignoring all unset samples; STATS_UNSET_BYTE if all samples are unset.
uint8_t NVByHourByteStatsBase::getMinByHourStat(const uint8_t statsSet) const
  {
  uint8_t result = UNSET_BYTE;
  for(int8_t hh = 24; --hh >= 0; )
    {
    const uint8_t v = getByHourStatSimple(statsSet, hh);
    // Optimisation/cheat: all valid samples are less than STATS_UNSET_BYTE.
    if(v < result) { result = v; }
    }
  return(result);
  }

// Get maximum sample from given stats set ignoring all unset samples; STATS_UNSET_BYTE if all samples are unset.
uint8_t NVByHourByteStatsBase::getMaxByHourStat(const uint8_t statsSet) const
  {
  uint8_t result = UNSET_BYTE;
  for(int8_t hh = 24; --hh >= 0; )
    {
    const uint8_t v = getByHourStatSimple(statsSet, hh);
    if((UNSET_BYTE != v) &&
       ((UNSET_BYTE == result) || (v > result)))
      { result = v; }
    }
  return(result);
  }

// Returns true iff there is a near-full set of stats (none unset) and 3/4s of the values are higher than the supplied sample.
// Always returns false if all samples are the same or unset (or the stats set is invalid).
//   * sample to be tested for being in lower quartile; if UNSET_BYTE routine returns false
bool NVByHourByteStatsBase::inBottomQuartile(const uint8_t statsSet, const uint8_t sample) const
  {
  // Optimisation for size: explicit test not needed // if(UNSET_BYTE == sample) { return(false); }
  uint8_t valuesHigher = 0;
  for(int8_t hh = 24; --hh >= 0; )
    {
    const uint8_t v = getByHourStatSimple(statsSet, hh); // const uint8_t v = eeprom_read_byte(sE);
    if(UNSET_BYTE == v) { return(false); } // Abort if not a full set of stats (eg at least one full day's worth).
    if(v > sample) { if(++valuesHigher >= 18) { return(true); } } // Stop as soon as known to be in lower quartile.
    }
  return(false); // Not in lower quartile.
  }

// Returns true iff there is a near-full set of stats (none unset) and 3/4s of the values are lower than the supplied sample.
// Always returns false if all samples are the same or unset (or the stats set is invalid).
//   * sample to be tested for being in lower quartile;  if UNSET_BYTE routine returns false
bool NVByHourByteStatsBase::inTopQuartile(const uint8_t statsSet, const uint8_t sample) const
  {
  if(UNSET_BYTE == sample) { return(false); }
  uint8_t valuesLower = 0;
  for(int8_t hh = 24; --hh >= 0; )
    {
    const uint8_t v = getByHourStatSimple(statsSet, hh); // const uint8_t v = eeprom_read_byte(sE);
    if(UNSET_BYTE == v) { return(false); } // Abort if not a full set of stats (eg at least one full day's worth).
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
bool NVByHourByteStatsBase::inOutlierQuartile(const bool inTop, const uint8_t statsSet, const uint8_t hh) const
  {
  // Rely on getByHourStatXXX() to validate statsSet,
  // returning UNSET if invalid or if the sample is unset.
  const uint8_t sample = (hh > 23) ? getByHourStatRTC(statsSet, hh) : getByHourStatSimple(statsSet, hh);
  if(UNSET_BYTE == sample) { return(false); } // Abort if not a valid/set sample.
  if(inTop) { return(inTopQuartile(statsSet, sample)); }
  return(inBottomQuartile(statsSet, sample));
  }


// Stats-, EEPROM- (and Flash-) friendly single-byte unary incrementable encoding.
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


// Range-compress an signed int 16ths-Celsius temperature to a unsigned single-byte value < 0xff.
// This preserves at least the first bit after the binary point for all values,
// and three bits after binary point for values in the most interesting mid range around normal room temperatures,
// with transitions at whole degrees Celsius.
// Input values below 0C are treated as 0C, and above 100C as 100C, thus allowing air and DHW temperature values.
uint8_t compressTempC16(const int16_t tempC16)
  {
  if(tempC16 <= 0) { return(0); } // Clamp negative values to zero.
  if(tempC16 < COMPRESSION_C16_LOW_THRESHOLD) { return(uint8_t(tempC16 >> 3)); } // Preserve 1 bit after the binary point (0.5C precision).
  if(tempC16 < COMPRESSION_C16_HIGH_THRESHOLD)
    { return(uint8_t(((tempC16 - COMPRESSION_C16_LOW_THRESHOLD) >> 1) + COMPRESSION_C16_LOW_THR_AFTER)); }
  if(tempC16 < COMPRESSION_C16_CEIL_VAL)
    { return(uint8_t(((tempC16 - COMPRESSION_C16_HIGH_THRESHOLD) >> 3) + COMPRESSION_C16_HIGH_THR_AFTER)); }
  return(COMPRESSION_C16_CEIL_VAL_AFTER);
  }

// Reverses range compression done by compressTempC16(); results in range [0,100], with varying precision based on original value.
// 0xff (or other invalid) input results in STATS_UNSET_INT.
int16_t expandTempC16(const uint8_t cTemp)
  {
  if(cTemp < COMPRESSION_C16_LOW_THR_AFTER) { return(int16_t(cTemp << 3)); }
  if(cTemp < COMPRESSION_C16_HIGH_THR_AFTER)
    { return(int16_t(((cTemp - COMPRESSION_C16_LOW_THR_AFTER) << 1) + COMPRESSION_C16_LOW_THRESHOLD)); }
  if(cTemp <= COMPRESSION_C16_CEIL_VAL_AFTER)
    { return(int16_t(((cTemp - COMPRESSION_C16_HIGH_THR_AFTER) << 3) + COMPRESSION_C16_HIGH_THRESHOLD)); }
  return(OTV0P2BASE::NVByHourByteStatsBase::UNSET_INT); // Invalid/unset input.
  }


}
