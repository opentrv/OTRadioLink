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
 Simple rolling stats management
 and system stats display.
 */

#ifndef OTV0P2BASE_STATS_H
#define OTV0P2BASE_STATS_H

#include <stdint.h>
#include <string.h>

#include "OTV0P2BASE_Sensor.h"
#include "OTV0P2BASE_Util.h"


namespace OTV0P2BASE
{


// Base for simple byte-wide non-volatile time-based (by hour) stats implementation.
// It is possible to encode/compand wider values into single stats byte values.
// Unset raw values are indicated by 0xff, ie map nicely to EEPROM.
// One implementation of this may map directly to underlying MCU EEPROM.
// This may also have wear-reducing and page-aware implementations for eg Flash.
#define NVByHourByteStatsBase_DEFINED
class NVByHourByteStatsBase
{
public:
    // 'Unset'/invalid stats values for stats:
    // byte (eg raw EEPROM byte)
    // and 2-byte signed int (eg after decompression).
    // These are to be used where erased non-volatile (eg EEPROM) values are 0xff.
    static constexpr uint8_t UNSET_BYTE = 0xff;
    static constexpr int16_t UNSET_INT = 0x7fff;

    // Special values indicating the current hour and the next hour, for stats.
    static constexpr uint8_t SPECIAL_HOUR_CURRENT_HOUR = 0xff;
    static constexpr uint8_t SPECIAL_HOUR_NEXT_HOUR = 0xfe;
    static constexpr uint8_t SPECIAL_HOUR_PREV_HOUR = 0xfd;

    // Standard stats sets (and count).
    // Implementations are not necessarily obliged to provide this exact set.
    // Note that by convention the even-numbered sets are raw
    // and the following set is the smoothed (eg over one week) version.
    enum CommonStatsSets : uint8_t
      {
STATS_SET_TEMP_BY_HOUR              = 0,  // Last companded temperature samples in each hour in range [0,248].
STATS_SET_TEMP_BY_HOUR_SMOOTHED     = 1,  // Smoothed hourly companded temperature samples in range [0,248].
STATS_SET_AMBLIGHT_BY_HOUR          = 2,  // Last ambient light level samples in each hour in range [0,254].
STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED = 3,  // Smoothed ambient light level samples in each hour in range [0,254].
STATS_SET_OCCPC_BY_HOUR             = 4,  // Last hourly observed occupancy percentage [0,100].
STATS_SET_OCCPC_BY_HOUR_SMOOTHED    = 5,  // Smoothed hourly observed occupancy percentage [0,100].
STATS_SET_RHPC_BY_HOUR              = 6,  // Last hourly relative humidity % samples in range [0,100].
STATS_SET_RHPC_BY_HOUR_SMOOTHED     = 7,  // Smoothed hourly relative humidity % samples in range [0,100].
STATS_SET_CO2_BY_HOUR               = 8,  // Last hourly companded CO2 ppm samples in range [0,254].
STATS_SET_CO2_BY_HOUR_SMOOTHED      = 9,  // Smoothed hourly companded CO2 ppm samples in range [0,254].
STATS_SET_USER1_BY_HOUR             = 10, // Last hourly user-defined stats value in range [0,254].
STATS_SET_USER1_BY_HOUR_SMOOTHED    = 11, // Smoothed hourly user-defined stats value in range [0,254].
STATS_SET_USER2_BY_HOUR             = 12, // Last hourly user-defined stats value in range [0,254].
STATS_SET_USER2_BY_HOUR_SMOOTHED    = 13, // Smoothed hourly user-defined stats value in range [0,254].

      // Number of default stats sets.
      STATS_SETS_COUNT
      };

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

    // Get raw stats value for specified hour [0,23] from stats set N from non-volatile (EEPROM) store.
    // A return value of 0xff (255) means unset (or out of range); other values depend on which stats set is being used.
    // The stats set is determined by the order in memory.
    //   * hour  hour of day to use
    virtual uint8_t getByHourStatSimple(const uint8_t statsSet, const uint8_t hh) const = 0;
    // Set raw stats value for specified hour [0,23] from stats set N in non-volatile (EEPROM) store.
    // Not passing the value byte is equivalent to erasing the value, eg typically 0xff for EEPROM or similar backing store.
    // The stats set is determined by the order in memory.
    //   * hour  hour of day to use
    virtual void setByHourStatSimple(const uint8_t statsSet, const uint8_t hh, const uint8_t v = UNSET_BYTE) = 0;

    // Get raw stats value for specified hour [0,23]/current/next from stats set N from non-volatile (EEPROM) store.
    // A value of STATS_UNSET_BYTE (0xff (255)) means unset (or out of range, or invalid); other values depend on which stats set is being used.
    //   * hour  hour of day to use, or ~0/0xff for current hour (default), 0xfe for next hour, or 0xfd for the previous hour.
    //           If the hour is invalid, an UNSET_BYTE will be returned.
    // Note the two special values that implicitly make use of the RTC to select the hour to read.
    uint8_t getByHourStatRTC(uint8_t statsSet, uint8_t hour = SPECIAL_HOUR_CURRENT_HOUR) const;

    // Returns the internal view of the current hour in range [0,23].
    // This should be defined by the implementer for ease of unit testing.
    virtual uint8_t getHour() const = 0;

    ////// Utility values and routines.
    // Returns true iff there is a near-full set of stats (none unset) and 3/4s of the values are higher than the supplied sample.
    // Always returns false if all samples are the same or unset (or the stats set is invalid).
    //   * statsSet  stats set number to use.
    //   * sample to be tested for being in lower quartile
    bool inBottomQuartile(uint8_t statsSet, const uint8_t sample) const;

    // Returns true iff there is a near-full set of stats (none unset) and 3/4s of the values are lower than the supplied sample.
    // Always returns false if all samples are the same or unset (or the stats set is invalid).
    //   * statsSet  stats set number to use.
    //   * sample to be tested for being in lower quartile
    bool inTopQuartile(uint8_t statsSet, const uint8_t sample) const;

    // Returns true if specified hour is (conservatively) in the specified outlier quartile for specified stats set.
    // Returns false if at least a near-full set of stats not available, eg including the specified hour, and for invalid stats set.
    // Always returns false if all samples are the same.
    //   * inTop  test for membership of the top quartile if true, bottom quartile if false
    //   * statsSet  stats set number to use.
    //   * hour  hour of day to use or STATS_SPECIAL_HOUR_CURRENT_HOUR for current hour or STATS_SPECIAL_HOUR_NEXT_HOUR for next hour
    bool inOutlierQuartile(bool inTop, uint8_t statsSet, uint8_t hour = SPECIAL_HOUR_CURRENT_HOUR) const;

    // Get minimum sample from given stats set ignoring all unset samples; STATS_UNSET_BYTE if all samples are unset and for invalid stats set.
    uint8_t getMinByHourStat(uint8_t statsSet) const;
    // Get maximum sample from given stats set ignoring all unset samples; STATS_UNSET_BYTE if all samples are unset and for invalid stats set.
    uint8_t getMaxByHourStat(uint8_t statsSet) const;

    // Compute the number of stats samples in specified set less than the specified value; returns 0 for invalid stats set.
    // (With the UNSET value specified, count will be of all samples that have been set, ie are not unset.)
    uint8_t countStatSamplesBelow(uint8_t statsSet, uint8_t value) const;

    // The default STATS_SMOOTH_SHIFT is chosen to retain some reasonable precision within a byte and smooth over a weekly cycle.
    // Number of bits of shift for smoothed value: larger => larger time-constant; strictly positive.
    static constexpr uint8_t STATS_SMOOTH_SHIFT = 3;

    // Compute new linearly-smoothed value given old smoothed value and new value.
    // Guaranteed not to produce a value higher than the max of the old smoothed value and the new value.
    // Uses stochastic rounding to nearest to allow nominally sub-lsb values to have an effect over time.
    static uint8_t smoothStatsValue(const uint8_t oldSmoothed, const uint8_t newValue);
  };

// Null read-only implementation that holds no stats.
class NULLByHourByteStats final : public NVByHourByteStatsBase
{
public:
    virtual bool zapStats(uint16_t = 0) override { return(true); } // No stats to erase, so all done.
    virtual uint8_t getByHourStatSimple(uint8_t, uint8_t) const override { return(UNSET_BYTE); }
    virtual void setByHourStatSimple(uint8_t, uint8_t, uint8_t = UNSET_BYTE) override { }
    // TODO: Not sure exactly how this should behave, but it will work given
    // the behaviour of the rest of the methods.
    virtual uint8_t getHour() const override { return(0xff); }
};

// Trivial read-only implementation that returns hour value in each slot with getByHourStatSimple().
// Enough to test some stats against.
class HByHourByteStats final : public OTV0P2BASE::NVByHourByteStatsBase
{
public:
    virtual bool zapStats(uint16_t = 0) override { return(true); } // No stats to erase, so all done.
    virtual uint8_t getByHourStatSimple(uint8_t, uint8_t h) const override { return(h); }
    virtual void setByHourStatSimple(uint8_t, const uint8_t, uint8_t = UNSET_BYTE) override { }
    // TODO: Not sure exactly how this should behave, but it will work given
    // the behaviour of the rest of the methods.
    virtual uint8_t getHour() const override { return(0xff); 
};

// Simple mock read-write stats container with a full internal ephemeral backing store for tests.
// Can be extended for, for example, RTC callbacks.
class NVByHourByteStatsMock : public NVByHourByteStatsBase
{
private:
	// Slots/bytes in a stats set.
	static constexpr uint8_t setSlots = 24;

	// Backing store for the stats.
	uint8_t statsMemory[STATS_SETS_COUNT][setSlots];

	// Current hour of day, for getByHourRTC().
	uint8_t currentHour = 0;

public:
	// Create instance will all stats 'erased' to UNSET_BYTE values.
	NVByHourByteStatsMock() { zapStats(); }

	// Set current hour of day for getByHourRTC(); invalid value is ignored.
	void _setHour(const uint8_t hourNow) { if(hourNow < 24) { currentHour = hourNow; } }

	// Always succeeds in one pass in this implementation.
	virtual bool zapStats(uint16_t = 0) override { memset(statsMemory, UNSET_BYTE, sizeof(statsMemory)); return(true); }

	// Bounds-checked read access from backing store.
	virtual uint8_t getByHourStatSimple(uint8_t statsSet, uint8_t hh) const override
		{ return(((statsSet >= STATS_SETS_COUNT) || (hh > setSlots)) ? UNSET_BYTE : statsMemory[statsSet][hh]); }

	// Bounds-checked write access to backing store.
	virtual void setByHourStatSimple(const uint8_t statsSet, const uint8_t hh, uint8_t value = UNSET_BYTE) override
		{ if(!((statsSet >= STATS_SETS_COUNT) || (hh > setSlots))) { statsMemory[statsSet][hh] = value; } }

	// Current Hour-of-day (as set by _setHour()).
	virtual uint8_t getHour() const override
	{ return (currentHour); }
};


// Range-compress an signed int 16ths-Celsius temperature to a unsigned single-byte value < 0xff.
// This preserves at least the first bit after the binary point for all values,
// and three bits after binary point for values in the most interesting mid range around normal room temperatures,
// with transitions at whole degrees Celsius.
// Input values below 0C are treated as 0C, and above 100C as 100C, thus allowing air and DHW temperature values.
static constexpr int16_t COMPRESSION_C16_FLOOR_VAL = 0; // Floor input value to compression.
static constexpr int16_t COMPRESSION_C16_LOW_THRESHOLD = (16<<4); // Values in range [COMPRESSION_LOW_THRESHOLD_C16,COMPRESSION_HIGH_THRESHOLD_C16[ have maximum precision.
static constexpr uint8_t COMPRESSION_C16_LOW_THR_AFTER = (COMPRESSION_C16_LOW_THRESHOLD>>3); // Low threshold after compression.
static constexpr int16_t COMPRESSION_C16_HIGH_THRESHOLD = (24<<4);
static constexpr uint8_t COMPRESSION_C16_HIGH_THR_AFTER = (COMPRESSION_C16_LOW_THR_AFTER + ((COMPRESSION_C16_HIGH_THRESHOLD-COMPRESSION_C16_LOW_THRESHOLD)>>1)); // High threshold after compression.
static constexpr int16_t COMPRESSION_C16_CEIL_VAL = (100<<4); // Ceiling input value to compression.
static constexpr uint8_t COMPRESSION_C16_CEIL_VAL_AFTER = (COMPRESSION_C16_HIGH_THR_AFTER + ((COMPRESSION_C16_CEIL_VAL-COMPRESSION_C16_HIGH_THRESHOLD) >> 3)); // Ceiling input value after compression.
uint8_t compressTempC16(int16_t tempC16);
// Reverses range compression done by compressTempC16(); results in range [0,100], with varying precision based on original value.
// 0xff (or other invalid) input results in STATS_UNSET_INT.
int16_t expandTempC16(uint8_t cTemp);

// Maximum valid encoded/compressed stats values.
static constexpr uint8_t MAX_STATS_TEMP = COMPRESSION_C16_CEIL_VAL_AFTER; // Maximum valid compressed temperature value in stats.
static constexpr uint8_t MAX_STATS_AMBLIGHT = 254; // Maximum valid ambient light value in stats (very top of range is compressed).


class ByHourSimpleStatsUpdaterBase
{
public:
    virtual void reset() = 0;
    virtual uint8_t getMaxSamplesPerHour() = 0;
    virtual void sampleStats(const bool fullSample, const uint8_t hh) = 0;
};

// Class to handle updating stats periodically, ie 1 or more times per hour.
//   * stats  stats container; never NULL
//   * ambLightOpt  optional ambient light (uint8_t) sensor; can be NULL
//   * tempC16Opt  optional ambient temperature (int16_t) sensor; can be NULL
//   * maxSubSamples  maximum number of samples to take per hour,
//       1 or 2 are especially efficient and avoid overflow,
//       2 is probably most robust;
//       strictly positive
template
	<
	class stats_t /* = NVByHourByteStatsBase */, stats_t *stats,
	class occupancy_t = SimpleTSUint8Sensor /*PseudoSensorOccupancyTracker*/, const occupancy_t *occupancyOpt = NULL,
	class ambLight_t = SimpleTSUint8Sensor /*SensorAmbientLightBase*/, const ambLight_t *ambLightOpt = NULL,
	class tempC16_t = Sensor<int16_t> /*TemperatureC16Base*/, const tempC16_t *tempC16Opt = NULL,
	class humidity_t = SimpleTSUint8Sensor /*HumiditySensorBase*/, const humidity_t *humidityOpt = NULL,
	uint8_t maxSubSamples = 2
	>
class ByHourSimpleStatsUpdaterSampleStats final : public ByHourSimpleStatsUpdaterBase
{
public:
    // Maximum number of (sub-) samples to take per hour; strictly positive.
    static constexpr uint8_t maxSamplesPerHour = maxSubSamples;

protected:
    // Efficient division of an uint16_t or uint8_t total by a small positive count to give a uint8_t mean.
    //  * total running total, no higher than 255*sampleCount
    //  * sampleCount small (<128) strictly positive number, no larger than maxSamplesPerHour
    template <class T = uint16_t>
    static uint8_t smartDivToU8(const T total, const uint8_t sampleCount)
	{
      	static_assert(maxSubSamples > 0, "maxSamplesPerHour must be strictly positive");
#if 0 && defined(DEBUG) // Extra arg validation during dev.
		if(0 == sampleCount) { panic(); }
		if(maxSubSamples < sampleCount) { panic(); }
#endif
		if((1 == maxSubSamples) || (1 == sampleCount)) { return(total); } // No division required.

		// Handle arbitrary number of samples,
		// but likely inflates code size and run-time and overflow risk.
		// Code should not be generated if maxSubSamples <= 2.
		if((maxSubSamples > 2) && (sampleCount > 2))
			{ return((uint8_t) ((total + (sampleCount>>1)) / sampleCount)); }

		// Exactly 2 samples.
		return((uint8_t) ((total+1) >> 1)); // Fast shift for 2 samples instead of slow divide.
	}

    // Do simple update of last and smoothed stats numeric values.
    // This assumes that the 'last' set is followed by the smoothed set.
    // This autodetects unset values in the smoothed set and replaces them completely.
    //   * statsSet for raw/'last' value, with 'smoothed' set one higher
    //   * hh  hour of data; [0,23]
    //   * value  new stats value in range [0,254]
    static void simpleUpdateStatsPair(const uint8_t statsSet, const uint8_t hh, const uint8_t value)
	{
		// Update the last-sample slot using the mean samples value.
		stats->setByHourStatSimple(statsSet, hh, value);
		// If existing smoothed value unset or invalid, use new one as is, else fold in.
		const uint8_t smoothedStatsSet = statsSet + 1;
		const uint8_t smoothed = stats->getByHourStatSimple(smoothedStatsSet, hh);
		if(OTV0P2BASE::NVByHourByteStatsBase::UNSET_BYTE == smoothed) { stats->setByHourStatSimple(smoothedStatsSet, hh, value); }
		else { stats->setByHourStatSimple(smoothedStatsSet, hh, OTV0P2BASE::NVByHourByteStatsBase::smoothStatsValue(smoothed, value)); }
	}

    // Select the type of the accumulator for percentage-value stats [0,100].
    // Where there are no more than two samples being accumulated
    // the sum can be held in a uint8_t without possibility of overflow.
    template <bool Condition, typename TypeTrue, typename TypeFalse>
      struct typeIf;
    template <typename TypeTrue, typename TypeFalse>
      struct typeIf<true, TypeTrue, TypeFalse> { typedef TypeTrue t; };
    template <typename TypeTrue, typename TypeFalse>
      struct typeIf<false, TypeTrue, TypeFalse> { typedef TypeFalse t; };
    typedef typename typeIf<maxSubSamples <= 2, uint8_t, uint16_t>::t percentageStatsAccumulator_t;

    int16_t tempC16Total {};
    uint16_t ambLightTotal {};
    percentageStatsAccumulator_t occpcTotal {}; // TODO: as range is [0,100], up to 2 samples could fit a uint8_t instead.
    percentageStatsAccumulator_t rhpcTotal {}; // TODO: as range is [0,100], up to 2 samples could fit a uint8_t instead.
    uint8_t sampleCount {};

public:
    // Clear any partial internal state; primarily for unit tests.
    // Does no write to the backing stats store.
    void reset() override { sampleStats(false, 0xff); }

    // Virtual getter method for maxSamplesPerHour.
    uint8_t getMaxSamplesPerHour() override { return(maxSamplesPerHour); }

    // Sample statistics fully once per hour as background to simple monitoring and adaptive behaviour.
    // Call this once per hour with fullSample==true, as near the end of the hour as possible;
    // this will update the non-volatile stats record for the current hour.
    // Optionally call this at up to maxSubSamples evenly-spaced times throughout the hour
    // with fullSample==false for all but the last to sub-sample
    // (and these may receive lower weighting or be ignored).
    // (EEPROM wear in backing store should not be an issue at this update rate in normal use.)
    //
    //   * fullSample  if true then this is the final (and full) sample for the hour
    //   * hh  is the hour of day; [0,23]
    //
    // Note that hh is only used when the final/full sample is taken,
    // and is used to determine where (in which slot) to file the stats.
    //
    // Call with out-of-range hh to effectively discard any partial samples.
    void sampleStats(const bool fullSample, const uint8_t hh) override
	{
		// (Sub-)sample processing.
		// In general, keep running total of sub-samples in a way that should not overflow
		// and use the mean to update the non-volatile EEPROM values on the fullSample call.
		// General sub-sample count; initially zero after boot,
		// and zeroed after each full sample or when explicitly reset.
		// static uint8_t sampleCount;
		if(hh > 23) { sampleCount = 0; return; }

		// Reject excess early sub-samples before full/final one.
		static_assert(maxSubSamples > 0, "must allow at least one (ie final) sample!");
		if(!fullSample && (sampleCount >= maxSubSamples-1)) { return; }

		const bool firstSample = (0 == sampleCount++);
		// Capture sample count to use below.
		const uint8_t sc = sampleCount;

		// Update all the different stats in turn
		// if the relevant sensor objects are non NULL.
		// Since these are known at compile time,
		// unused/dead code should simply not be generated.

		if((NULL != ambLightOpt) && ambLightOpt->isAvailable()) {
			// Ambient light.
			const uint16_t ambLightV = OTV0P2BASE::fnmin(ambLightOpt->get(), (uint8_t)254); // Constrain value at top end to avoid 'not set' value.
			// static uint16_t ambLightTotal;
			ambLightTotal = firstSample ? ambLightV : (ambLightTotal + ambLightV);
			if(fullSample) { 
				simpleUpdateStatsPair(
					OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_AMBLIGHT_BY_HOUR, 
					hh, 
					smartDivToU8(ambLightTotal, sc));
			}
		}

		if((NULL != tempC16Opt) && tempC16Opt->isAvailable()) {
			// Ambient (eg room) temperature in C*16 units.
			const int16_t tempC16 = tempC16Opt->get();
			// static int16_t tempC16Total;
			tempC16Total = firstSample ? tempC16 : (tempC16Total + tempC16);
			if(fullSample) {
				// Scale and constrain last-read temperature to valid range for stats.
				const int16_t tempCTotal = (maxSamplesPerHour <= 2)
				? ((1==sc)?tempC16Total:((tempC16Total+1)>>1))
				: ((1==sc)?tempC16Total:
						((2==sc)?((tempC16Total+1)>>1):
								((tempC16Total + (sc>>1)) / sc)));
				const uint8_t temp = OTV0P2BASE::compressTempC16(tempCTotal);
				simpleUpdateStatsPair(OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_TEMP_BY_HOUR, hh, temp);
			}
		}

		if((NULL != occupancyOpt) && occupancyOpt->isAvailable()) {
			// Occupancy percentage.
			const uint8_t occpc = occupancyOpt->get();
			// static percentageStatsAccumulator_t occpcTotal; // TODO: as range is [0,100], up to 2 samples could fit a uint8_t instead.
			occpcTotal = firstSample ? occpc : (occpcTotal + occpc);
			if(fullSample) { 
				simpleUpdateStatsPair(
					OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_OCCPC_BY_HOUR, 
					hh, 
					smartDivToU8(occpcTotal, sc));
			}
		}

		if((NULL != humidityOpt) && (humidityOpt->isAvailable())) {
			// Relative humidity (RH%).
			const uint8_t rhpc = humidityOpt->get();
			rhpcTotal = firstSample ? rhpc : (rhpcTotal + rhpc);
			if(fullSample) { 
				simpleUpdateStatsPair(
					OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_RHPC_BY_HOUR, 
					hh, 
					smartDivToU8(rhpcTotal, sc));
			}
		}

		// TODO: other stats measures...

		if(!fullSample) { return; } // Only accumulate values cached until a full sample.
		// Reset generic sub-sample count to initial state after full sample.
		sampleCount = 0;
	}
  };

// Stats-, EEPROM- (and Flash-) friendly single-byte unary incrementable encoding.
// A single byte can be used to hold a single value [0,8]
// such that increment requires only a write of one bit (no erase)
// and in general increasing the value up to the maximum only requires
// a single byte write.
// An erase is required only to decrease the value (eg back to zero).
// An initial EEPROM (erased) value of 0xff is mapped to zero.
// The two byte version can hold values in the range [0,16].
// Corruption can be detected if an unexpected bit pattern is encountered
// at decode time.
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
static constexpr uint8_t EEPROM_UNARY_1BYTE_MAX_VALUE = 8;
static constexpr uint8_t EEPROM_UNARY_2BYTE_MAX_VALUE = 16;
inline constexpr uint8_t eeprom_unary_1byte_encode(uint8_t n) { return((n >= 8) ? 0 : uint8_t(0xffU << n)); }
inline constexpr uint16_t eeprom_unary_2byte_encode(uint8_t n) { return((n >= 16) ? 0 : uint16_t(0xffffU << n)); }
// Decode routines return -1 in case of unexpected/invalid input patterns.
// All other (valid non-negative) return values can be safely cast to unit8_t.
int8_t eeprom_unary_1byte_decode(uint8_t v);
int8_t eeprom_unary_2byte_decode(uint8_t vm, uint8_t vl);
// First arg is most significant byte.
inline int8_t eeprom_unary_2byte_decode(uint16_t v) { return(eeprom_unary_2byte_decode((uint8_t)(v >> 8), (uint8_t)v)); }


}
#endif
