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
                           Deniz Erbilgin 2016
*/

/*
 Simple rolling stats management.
 */

#ifndef OTV0P2BASE_STATS_H
#define OTV0P2BASE_STATS_H

#include <stdint.h>

#include "OTV0P2BASE_QuickPRNG.h"


namespace OTV0P2BASE
{


// 'Unset'/invalid stats values for stats:
// byte (eg raw EEPROM byte)
// and 2-byte signed int (eg after decompression).
// These are to be used where erased non-volatile (eg EEPROM) values are 0xff.
static const uint8_t STATS_UNSET_BYTE = 0xff;
static const int16_t STATS_UNSET_INT = 0x7fff;

// Special values indicating the current hour and the next hour, for stats.
static const uint8_t STATS_SPECIAL_HOUR_CURRENT_HOUR = uint8_t(~0 - 1);
static const uint8_t STATS_SPECIAL_HOUR_NEXT_HOUR = uint8_t(~0);

// Base for simple byte-wide non-volatile time-based (by hour) stats implementation.
// It is possible to encode/compand wider values into single stats byte values.
// Unset raw values are indicated by 0xff, ie map nicely to EEPROM.
// One implementation of this may map directly to underlying MCU EEPROM on some systems.
// This may also have wear-reducing implementations for (say) Flash.
#define NVByHourByteStatsBase_DEFINED
class NVByHourByteStatsBase
  {
  public:
    // 'Unset'/invalid stats values for stats:
    // byte (eg raw EEPROM byte)
    // and 2-byte signed int (eg after decompression).
    // These are to be used where erased non-volatile (eg EEPROM) values are 0xff.
    static constexpr uint8_t UNSET_BYTE = STATS_UNSET_BYTE;

    // Special values indicating the current hour and the next hour, for stats.
    static constexpr uint8_t SPECIAL_HOUR_CURRENT_HOUR = STATS_SPECIAL_HOUR_CURRENT_HOUR;
    static constexpr uint8_t SPECIAL_HOUR_NEXT_HOUR = STATS_SPECIAL_HOUR_NEXT_HOUR;

    // Clear all collected statistics fronted by this.
    // Use (eg) when moving device to a new room or at a major time change.
    // May require significant time (eg milliseconds) per byte for each byte that actually needs erasing.
    //   * maxBytesToErase limit the number of bytes erased to this; strictly positive, else 0 to allow 65536
    // Returns true if finished with all bytes erased.
    //
    // Optimisation note: this will not be called during most system executions,
    // and is not performance-critical (though must not cause overruns),
    // so may be usefully marked as "cold" or "optimise for space"
    // for most implementations/compilers.
    virtual bool zapStats(uint16_t maxBytesToErase = 0) = 0;

    // Get raw stats value for specified hour [0,23]/current/next from stats set N from non-volatile (EEPROM) store.
    // A value of STATS_UNSET_BYTE (0xff (255)) means unset (or out of range); other values depend on which stats set is being used.
    //   * hour  hour of day to use, or ~0/0xff for current hour (default), or >23 for next hour.
    virtual uint8_t getByHourStat(uint8_t statsSet, uint8_t hour = 0xff) const = 0;

    // Get minimum sample from given stats set ignoring all unset samples; STATS_UNSET_BYTE if all samples are unset and for invalid stats set.
    virtual uint8_t getMinByHourStat(uint8_t statsSet) const = 0;
    // Get maximum sample from given stats set ignoring all unset samples; STATS_UNSET_BYTE if all samples are unset and for invalid stats set.
    virtual uint8_t getMaxByHourStat(uint8_t statsSet) const = 0;

    // Returns true if specified hour is (conservatively) in the specified outlier quartile for specified stats set.
    // Returns false if at least a near-full set of stats not available, eg including the specified hour, and for invalid stats set.
    // Always returns false if all samples are the same.
    //   * inTop  test for membership of the top quartile if true, bottom quartile if false
    //   * statsSet  stats set number to use.
    //   * hour  hour of day to use or STATS_SPECIAL_HOUR_CURRENT_HOUR for current hour or STATS_SPECIAL_HOUR_NEXT_HOUR for next hour
    virtual bool inOutlierQuartile(bool inTop, uint8_t statsSet, uint8_t hour = STATS_SPECIAL_HOUR_CURRENT_HOUR) const = 0;

    // Compute the number of stats samples in specified set less than the specified value; returns STATS_UNSET_BYTE for invalid stats set.
    // (With the UNSET value specified, count will be of all samples that have been set, ie are not unset.)
    virtual uint8_t countStatSamplesBelow(uint8_t statsSet, uint8_t value) const = 0;

    ////// Utility values and routines.

    // The default STATS_SMOOTH_SHIFT is chosen to retain some reasonable precision within a byte and smooth over a weekly cycle.
    // Number of bits of shift for smoothed value: larger => larger time-constant; strictly positive.
    static constexpr uint8_t STATS_SMOOTH_SHIFT = 3;

    // Compute new linearly-smoothed value given old smoothed value and new value.
    // Guaranteed not to produce a value higher than the max of the old smoothed value and the new value.
    // Uses stochastic rounding to nearest to allow nominally sub-lsb values to have an effect over time.
    static uint8_t smoothStatsValue(const uint8_t oldSmoothed, const uint8_t newValue);
  };


// Null implementation that does nothing.
class NULLByHourByteStatsBase final : public NVByHourByteStatsBase
  {
  public:
    virtual bool zapStats(uint16_t = 0) override { return(true); } // No stats to erase, so all done.
    virtual uint8_t getByHourStat(uint8_t, uint8_t = 0xff) const override { return(UNSET_BYTE); }
    virtual uint8_t getMinByHourStat(uint8_t) const override { return(UNSET_BYTE); }
    virtual uint8_t getMaxByHourStat(uint8_t) const override { return(UNSET_BYTE); }
    virtual bool inOutlierQuartile(bool, uint8_t, uint8_t = STATS_SPECIAL_HOUR_CURRENT_HOUR) const override { return(false); }
    virtual uint8_t countStatSamplesBelow(uint8_t, uint8_t) const override { return(STATS_UNSET_BYTE); }
  };


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
static const uint8_t EEPROM_UNARY_1BYTE_MAX_VALUE = 8;
static const uint8_t EEPROM_UNARY_2BYTE_MAX_VALUE = 16;
inline uint8_t eeprom_unary_1byte_encode(uint8_t n) { return((n >= 8) ? 0 : uint8_t(0xffU << n)); }
inline uint16_t eeprom_unary_2byte_encode(uint8_t n) { return((n >= 16) ? 0 : uint16_t(0xffffU << n)); }
// Decode routines return -1 in case of unexpected/invalid input patterns.
// All other (valid non-negative) return values can be safely cast to unit8_t.
int8_t eeprom_unary_1byte_decode(uint8_t v);
int8_t eeprom_unary_2byte_decode(uint8_t vm, uint8_t vl);
// First arg is most significant byte.
inline int8_t eeprom_unary_2byte_decode(uint16_t v) { return(eeprom_unary_2byte_decode((uint8_t)(v >> 8), (uint8_t)v)); }


// Range-compress an signed int 16ths-Celsius temperature to a unsigned single-byte value < 0xff.
// This preserves at least the first bit after the binary point for all values,
// and three bits after binary point for values in the most interesting mid range around normal room temperatures,
// with transitions at whole degrees Celsius.
// Input values below 0C are treated as 0C, and above 100C as 100C, thus allowing air and DHW temperature values.
static const int16_t COMPRESSION_C16_FLOOR_VAL = 0; // Floor input value to compression.
static const int16_t COMPRESSION_C16_LOW_THRESHOLD = (16<<4); // Values in range [COMPRESSION_LOW_THRESHOLD_C16,COMPRESSION_HIGH_THRESHOLD_C16[ have maximum precision.
static const uint8_t COMPRESSION_C16_LOW_THR_AFTER = (COMPRESSION_C16_LOW_THRESHOLD>>3); // Low threshold after compression.
static const int16_t COMPRESSION_C16_HIGH_THRESHOLD = (24<<4);
static const uint8_t COMPRESSION_C16_HIGH_THR_AFTER = (COMPRESSION_C16_LOW_THR_AFTER + ((COMPRESSION_C16_HIGH_THRESHOLD-COMPRESSION_C16_LOW_THRESHOLD)>>1)); // High threshold after compression.
static const int16_t COMPRESSION_C16_CEIL_VAL = (100<<4); // Ceiling input value to compression.
static const uint8_t COMPRESSION_C16_CEIL_VAL_AFTER = (COMPRESSION_C16_HIGH_THR_AFTER + ((COMPRESSION_C16_CEIL_VAL-COMPRESSION_C16_HIGH_THRESHOLD) >> 3)); // Ceiling input value after compression.
uint8_t compressTempC16(int16_t tempC16);
// Reverses range compression done by compressTempC16(); results in range [0,100], with varying precision based on original value.
// 0xff (or other invalid) input results in STATS_UNSET_INT.
int16_t expandTempC16(uint8_t cTemp);

// Maximum valid encoded/compressed stats values.
static const uint8_t MAX_STATS_TEMP = COMPRESSION_C16_CEIL_VAL_AFTER; // Maximum valid compressed temperature value in stats.
static const uint8_t MAX_STATS_AMBLIGHT = 254; // Maximum valid ambient light value in stats (very top of range is compressed).


}
#endif
