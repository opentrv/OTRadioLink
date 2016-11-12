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
  return(OTV0P2BASE::STATS_UNSET_INT); // Invalid/unset input.
  }


}
