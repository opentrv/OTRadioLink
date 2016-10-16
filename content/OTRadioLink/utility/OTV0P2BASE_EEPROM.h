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
 EEPROM space allocation and utilities including some of the simple rolling stats management.

 NOTE: NO EEPROM ACCESS SHOULD HAPPEN FROM ANY ISR CODE ELSE VARIOUS FAILURE MODES ARE POSSIBLE

 Mainly V0p2/AVR for now.
 */

#ifndef OTV0P2BASE_EEPROM_H
#define OTV0P2BASE_EEPROM_H

#include <stdint.h>

#ifdef ARDUINO_ARCH_AVR
#include <avr/eeprom.h>
#endif


namespace OTV0P2BASE
{


// 'Unset'/invalid stats values for byte (eg raw EEPROM byte)
// and 2-byte signed int (eg after decompression).
// These are to be used where erased non-volatile (eg EEPROM) values are 0xff.
static const uint8_t STATS_UNSET_BYTE = 0xff;
static const int16_t STATS_UNSET_INT = 0x7fff;


#ifdef ARDUINO_ARCH_AVR

// ATmega328P has 1kByte of EEPROM, with an underlying page size (datasheet section 27.5) of 4 bytes for wear purposes.
// Endurance may be per page (or per bit-change), rather than per byte, eg: http://www.mail-archive.com/avr-libc-dev@nongnu.org/msg02456.html
// Also see AVR101: High Endurance EEPROM Storage: http://www.atmel.com/Images/doc2526.pdf
// Also see AVR103: Using the EEPROM Programming Modes http://www.atmel.com/Images/doc2578.pdf
// Note that with split erase/program operations specialised bitwise programming can be achieved with lower wear.
#if defined(__AVR_ATmega328P__)
#define V0P2BASE_EEPROM_SIZE 1024
#define V0P2BASE_EEPROM_PAGE_SIZE 4
#define V0P2BASE_EEPROM_SPLIT_ERASE_WRITE // Separate erase and write are possible.
#endif

// Updates an EEPROM byte iff not currently at the specified target value.
// May be able to selectively erase or write (ie reduce wear) to reach the desired value.
// As with the AVR eeprom_XXX_byte() macros, not safe to use outside and within ISRs as-is.
// Returns true iff an erase and/or write was performed.
bool eeprom_smart_update_byte(uint8_t *p, uint8_t value);

// Erases (sets to 0xff) the specified EEPROM byte, avoiding a (redundant) write if possible.
// If the target byte is already 0xff then this does nothing at all beyond an initial read.
// This saves a bit of time and power and possibly a little wear also.
// Without split erase/write this degenerates to a specialised eeprom_update_byte().
// As with the AVR eeprom_XXX_byte() macros, not safe to use outside and within ISRs as-is.
// Returns true iff an erase was performed.
bool eeprom_smart_erase_byte(uint8_t *p);

// ANDs the supplied mask into the specified EEPROM byte, avoiding an initial (redundant) erase if possible.
// This can be used to ensure that specific bits are 0 while leaving others untouched.
// If ANDing in the mask has no effect then this does nothing at all beyond an initial read.
// This saves a bit of time and power and possibly a little EEPROM cell wear also.
// Without split erase/write this degenerates to a specialised eeprom_smart_update_byte().
// As with the AVR eeprom_XXX_byte() macros, not safe to use outside and within ISRs as-is.
// Returns true iff a write was performed.
bool eeprom_smart_clear_bits(uint8_t *p, uint8_t mask);


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
static const uint8_t EEPROM_UNARY_1BYTE_MAX_VALUE = 8;
static const uint8_t EEPROM_UNARY_2BYTE_MAX_VALUE = 16;
inline uint8_t eeprom_unary_1byte_encode(uint8_t n) { return((n >= 8) ? 0 : (0xffU << n)); }
inline uint16_t eeprom_unary_2byte_encode(uint8_t n) { return((n >= 16) ? 0 : (0xffffU << n)); }
// Decode routines return -1 in case of unexpected/invalid input patterns.
// All other (valid non-negative) return values can be safely cast to unit8_t.
int8_t eeprom_unary_1byte_decode(uint8_t v);
int8_t eeprom_unary_2byte_decode(uint8_t vm, uint8_t vl);
// First arg is most significant byte.
inline int8_t eeprom_unary_2byte_decode(uint16_t v) { return(eeprom_unary_2byte_decode((uint8_t)(v >> 8), (uint8_t)v)); }


// Unit test location for erase/write.
// Also may be more vulnerable to damage during resets/brown-outs.
#define V0P2BASE_EE_START_TEST_LOC 0 // 1-byte test location.
// Second unit test location for erase/write.
#define V0P2BASE_EE_START_TEST_LOC2 1 // 1-byte test location.
// Store a few bits of (non-secure) random seed/entropy from one run to another.
// Used in a way that increases likely EEPROM endurance.
// Deliberately crosses an EEPROM page boundary.
#define V0P2BASE_EE_START_SEED 2 // *4-byte* store for part of (non-crypto) persistent random seed.
#define V0P2BASE_EE_LEN_SEED 4 // *4-byte* store for part of (non-crypto) persistent random seed.

// Reset/restart count (least-significant 2 bytes) for diagnostic and crypto (eg nonce) purposes.
#define V0P2BASE_EE_START_RESET_COUNT 6 // Modulo-256 reset count (ls byte), for diagnostic and crypto purposes.
#define V0P2BASE_EE_START_RESET_COUNT2 7 // Second byte of reset count, for diagnostic and crypto purposes..

// Space for RTC to persist current day/date.
#define V0P2BASE_EE_START_RTC_DAY_PERSIST 8 // 2-byte store for RTC to persist day/date.
// Space for RTC to persist current time in 15-minute increments with a low-wear method.
// Nothing else receiving frequent updates should go in this EEPROM page if possible.
#define V0P2BASE_EE_START_RTC_HHMM_PERSIST 10 // 1-byte store for RTC to persist time of day.  Not in same page as anything else updated frequently.
#define V0P2BASE_EE_START_RTC_RESERVED 11 // Reserved byte for future use.  (Could store real minutes of power-fail on last gasp.)

// 2-byte block to support primary simple 7-day schedule, if in use.
#define V0P2BASE_EE_START_SIMPLE_SCHEDULE0_ON 12 // 1-byte encoded 'minutes after midnight' / 6 primary on-time, if any.
#define V0P2BASE_EE_START_MAX_SIMPLE_SCHEDULES 2 // Maximum number of 'ON' schedules that can be stored, starting with schedule 0.

// For overrides of default FROST and WARM target values (in C); 0xff means 'use defaults'.
#define V0P2BASE_EE_START_FROST_C 14 // Override for FROST target/threshold.
#define V0P2BASE_EE_START_WARM_C 15 // Override for WARM target/threshold.

// For FHT8V wireless radiator valve control.
#define V0P2BASE_EE_START_FHT8V_HC1 16 // (When controlling FHT8V rad valve): 1-byte value for house-code 1, 0xff if not in use.
#define V0P2BASE_EE_START_FHT8V_HC2 17 // (When controlling FHT8V rad valve): 1-byte value for house-code 2, 0xff if not in use.

// One byte BITWISE-INVERTED minimum number of minutes of boiler time ON; ~0 (erased/default) means NOT in hub/boiler mode.
// Bitwise-inverted so that erased/unset 0xff (~0) value leaves boiler mode disabled.
#define V0P2BASE_EE_START_MIN_BOILER_ON_MINS_INV 18 // Bitwise inverted minimum boiler on-time or zero if not in central boiler hub mode.

// Minimum (percentage) threshold that local radiator valve is considered open.
#define V0P2BASE_EE_START_MIN_VALVE_PC_REALLY_OPEN 19 // Ignored entirely if outside range [1,100], eg if default/unprogrammed 0xff.

// Generic (8-byte) node ID, of which usually only 2 first bytes are used in OpenTRV-native messages.
// All valid ID bytes have the high bit set but are not 0xff, ie in the range [128,254].
// An 0xff value indicates not set and the system may automatically generate a new ID byte.
// Note that in the presence of (say) a house-code, that takes precedence;
// Since FHT8V house codes are in the range [0,99] there is no ambiguity between these values.
#define V0P2BASE_EE_START_ID 20 // (When controlling FHT8V rad valve): 1-byte value for house-code 1, 0xff if not in use.
#define V0P2BASE_EE_LEN_ID 8 // 64-bit unique ID.  Only first 2 bytes generally used in OpenTRV-native messages.

// 1-byte value used to enable/disable stats transmissions.
// A combination of this value and available channel security determines how much is transmitted.
#define V0P2BASE_EE_START_STATS_TX_ENABLE 28 // 0xff disables all avoidable stats transmissions; 0 enables all.
// A 1-byte overrun counter, inverted so 0xff means 0.
#define V0P2BASE_EE_START_OVERRUN_COUNTER 29

// Maximum (percentage) that local radiator value is allowed to open.
#define V0P2BASE_EE_START_MAX_VALVE_PC_OPEN 30 // Ignored entirely if outside range [1,100], eg if default/unprogrammed 0xff.

// Minimum (total percentage across all rads) that all rads should be on before heating should fire.
#define V0P2BASE_EE_START_MIN_TOTAL_VALVE_PC_OPEN 31 // Ignored entirely if outside range [1,100], eg if default/unprogrammed 0xff.


// GENERIC STORAGE AREA.
// Lowest EEPROM address allowed for raw inspect/set.
// Items beyond this may be particularly security-sensitive, eg secret keys.
static const intptr_t V0P2BASE_EE_START_RAW_INSPECTABLE = 32;
// Length of generic storage area.
static const uint8_t V0P2BASE_EE_LEN_RAW_INSPECTABLE = 32;
// Highest EEPROM address allowed for raw inspect/set.
// Items beyond this may be particularly security-sensitive, eg secret keys.
static const intptr_t V0P2BASE_EE_END_RAW_INSPECTABLE = V0P2BASE_EE_START_RAW_INSPECTABLE + V0P2BASE_EE_LEN_RAW_INSPECTABLE - 1;
// Lockout time before energy-saving setbacks are enabled (if not 0), stored inverted.
// For release V1.0.4 2016/03/15 the units were hours, allowing for ~10 days max lockout.
// Subsequent use prefers days, allowing more than enough to cover any part of a heating season.
// Stored inverted so that a default erased (0xff) value will be seen as 0, so no lockout and thus normal behaviour.
static const intptr_t V0P2BASE_EE_START_SETBACK_LOCKOUT_COUNTDOWN_D_INV = 0 + V0P2BASE_EE_START_RAW_INSPECTABLE;


// TX message counter (most-significant) persistent reboot/restart 3 bytes.  (TODO-728)
// Nominally the counter associated with the primary TX key,
// which may be the primary building key for simple configurations,
// though can nominally be shared for TX with multiple keys
// with the risk of confusing sequence numbers for receivers using a subset of those keys.
// Stored inverted (so initially nominal all zeros counter MSB values,
// though least significant byte(s) may be randomised to widen initial IV space).
// All bytes are duplicated to detect failure and the higher of each is used.
// The least-significant byte pair is incremented alternately to reduce write load.
// Two further bytes are reserved for (for example) checksums/CRCs.
static const intptr_t VOP2BASE_EE_START_PERSISTENT_MSG_RESTART_CTR = 104; // Primary set.
static const intptr_t VOP2BASE_EE_START_PERSISTENT_MSG_RESTART_CTR_ALTERNATE = 108; // Alternate set.
static const uint8_t VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR = 8;
// 16 Byte primary building (secret, symmetric) key.  (TODO-793)
static const intptr_t VOP2BASE_EE_START_16BYTE_PRIMARY_BUILDING_KEY = 112;
static const uint8_t VOP2BASE_EE_LEN_16BYTE_PRIMARY_BUILDING_KEY  = 16;


// Start area of non-volatile radio config, eg for GSM APN and target IP address.
static const intptr_t V0P2BASE_EE_START_RADIO = 128; // INCLUSIVE START OF RADIO CONFIG AREA.
static const uint8_t V0P2BASE_EE_LEN_RADIO  = 128; // SIZE OF RADIO CONFIG AREA (bytes).
static const intptr_t V0P2BASE_EE_END_RADIO = 255;


// Bulk data storage: should fit within 1kB EEPROM of ATmega328P or 512B of ATmega164P.
#define V0P2BASE_EE_START_STATS 256 // INCLUSIVE START OF BULK STATS AREA.
#define V0P2BASE_EE_STATS_SET_SIZE 24 // Size in entries/bytes of one normal EEPROM-resident hour-of-day stats set.

// Stats set numbers, 0 upwards, contiguous.
// Generally even-numbered values are 'last' values and odd-numbered are 'smoothed' nominally over a week.
#define V0P2BASE_EE_STATS_SET_TEMP_BY_HOUR               0  // Last companded temperature samples in each hour in range [0,248].
#define V0P2BASE_EE_STATS_SET_TEMP_BY_HOUR_SMOOTHED      1  // Smoothed hourly companded temperature samples in range [0,248].
#define V0P2BASE_EE_STATS_SET_AMBLIGHT_BY_HOUR           2  // Last ambient light level samples in each hour in range [0,254].
#define V0P2BASE_EE_STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED  3  // Smoothed ambient light level samples in each hour in range [0,254].
#define V0P2BASE_EE_STATS_SET_OCCPC_BY_HOUR              4  // Last hourly observed occupancy percentage [0,100].
#define V0P2BASE_EE_STATS_SET_OCCPC_BY_HOUR_SMOOTHED     5  // Smoothed hourly observed occupancy percentage [0,100].
#define V0P2BASE_EE_STATS_SET_RHPC_BY_HOUR               6  // Last hourly relative humidity % samples in range [0,100].
#define V0P2BASE_EE_STATS_SET_RHPC_BY_HOUR_SMOOTHED      7  // Smoothed hourly relative humidity % samples in range [0,100].
#define V0P2BASE_EE_STATS_SET_CO2_BY_HOUR                8  // Last hourly companded CO2 ppm samples in range [0,254].
#define V0P2BASE_EE_STATS_SET_CO2_BY_HOUR_SMOOTHED       9  // Smoothed hourly companded CO2 ppm samples in range [0,254].
#define V0P2BASE_EE_STATS_SET_USER1_BY_HOUR              10 // Last hourly user-defined stats value in range [0,254].
#define V0P2BASE_EE_STATS_SET_USER1_BY_HOUR_SMOOTHED     11 // Smoothed hourly user-defined stats value in range [0,254].
#define V0P2BASE_EE_STATS_SET_USER2_BY_HOUR              12 // Last hourly user-defined stats value in range [0,254].
#define V0P2BASE_EE_STATS_SET_USER2_BY_HOUR_SMOOTHED     13 // Smoothed hourly user-defined stats value in range [0,254].
//#define V0P2BASE_EE_STATS_SET_WARMMODE_BY_HOUR_OF_WK    10  // Each sample is last 7 days' WARM mode bitset by hour. [0,127].

#define V0P2BASE_EE_STATS_SETS 14 // Number of stats sets in range [0,V0P2BASE_EE_STATS_SETS-1].

// Compute start of stats set n (in range [0,V0P2BASE_EE_STATS_SETS-1]) in EEPROM.
// Eg use as V0P2BASE_EE_STATS_START_ADDR(V0P2BASE_EE_STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED) in
//   const uint8_t smoothedAmbLight = eeprom_read_byte((uint8_t *)(V0P2BASE_EE_STATS_START_ADDR(V0P2BASE_EE_STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED) + hh));
#define V0P2BASE_EE_STATS_START_ADDR(n) (V0P2BASE_EE_START_STATS + V0P2BASE_EE_STATS_SET_SIZE*(n))
// INCLUSIVE END OF BULK STATS AREA: must point to last byte used.
#define V0P2BASE_EE_END_STATS (V0P2BASE_EE_STATS_START_ADDR(V0P2BASE_EE_STATS_SETS+1)-1)

//#if V0P2BASE_EE_END_HUB_HC_FILTER >= V0P2BASE_EE_START_STATS
//#error EEPROM allocation problem: filter overlaps with stats
//#endif


// Node security association storage.
// (ID plus permanent message counter for RX.)
// Can fit 8 nodes within 256 bytes of EEPROM with 24 bytes of related data.  (TODO-793)
//
// Working area ahead for updating node associations.
static const intptr_t V0P2BASE_EE_START_NODE_ASSOCIATIONS_WORK_START = 704;
static const uint8_t V0P2BASE_EE_START_NODE_ASSOCIATIONS_WORK_LEN = 64;
//
// Note that all valid entries/associations are contiguous at the start of the area.
// The first (invalid) node ID starting with 0xff indicates that it and all subsequent entries are empty.
// On writing a new entry all bytes after the ID must be erased to 0xff.
static const intptr_t V0P2BASE_EE_START_NODE_ASSOCIATIONS = 768;  // Inclusive start of node associations.
static const uint8_t V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE = 32; // Size in bytes of one Node association entry.
//
// Node association fields, 0 upwards, contiguous.
// Offset of full-byte ID in table row.
static const uint8_t V0P2BASE_EE_NODE_ASSOCIATIONS_8B_ID_OFFSET = 0; // 8 Byte node ID.
static const uint8_t V0P2BASE_EE_NODE_ASSOCIATIONS_8B_ID_LENGTH = 8; // 8 Byte node ID.
// Primary RX message counter (6 bytes + other support) offset.
static const uint8_t V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_0_OFFSET = 8;
// Secondary RX message counter (6 bytes + other support) offset.
static const uint8_t V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_1_OFFSET = 16;
// Reserved starting offset in table row.
static const uint8_t V0P2BASE_EE_NODE_ASSOCIATIONS_RESERVED_OFFSET = 24;
//
// Maximum possible node associations, ie nodes that can be securely received from.
// Where more than this are needed then this device can be in pass-through mode
// for more powerful back-end server to filter/auth/decrypt wanted traffic.
static const uint8_t V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS = 8;
//
// Compute start of node association set (in range [0,V0P2BASE_EE_NODE_ASSOCIATIONS_SETS-1]) in EEPROM.
// static const uint16_t V0P2BASE_EE_NODE_ASSOCIATIONS_START_ADDR
// INCLUSIVE END OF NODE ASSOCIATIONS AREA: must point to last byte used.
static const intptr_t V0P2BASE_EE_END_NODE_ASSOCIATIONS = ((V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS * V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE)-1);

// Special values indicating the current hour and the next hour, for stats.
static const uint8_t STATS_SPECIAL_HOUR_CURRENT_HOUR = ~0 - 1;
static const uint8_t STATS_SPECIAL_HOUR_NEXT_HOUR = ~0;

// Clear all collected statistics, eg when moving device to a new room or at a major time change.
// Requires 1.8ms per byte for each byte that actually needs erasing.
//   * maxBytesToErase limit the number of bytes erased to this; strictly positive, else 0 to allow 65536
// Returns true if finished with all bytes erased.
bool zapStats(uint16_t maxBytesToErase = 0);

// Get raw stats value for specified hour [0,23]/current/next from stats set N from non-volatile (EEPROM) store.
// A value of STATS_UNSET_BYTE (0xff (255)) means unset (or out of range); other values depend on which stats set is being used.
//   * hour  hour of day to use, or ~0/0xff for current hour (default), or >23 for next hour.
uint8_t getByHourStat(uint8_t statsSet, uint8_t hour = 0xff);

// Get minimum sample from given stats set ignoring all unset samples; STATS_UNSET_BYTE if all samples are unset.
uint8_t getMinByHourStat(uint8_t statsSet);
// Get maximum sample from given stats set ignoring all unset samples; STATS_UNSET_BYTE if all samples are unset.
uint8_t getMaxByHourStat(uint8_t statsSet);

// Returns true if specified hour is (conservatively) in the specified outlier quartile for specified stats set.
// Returns false if at least a near-full set of stats not available, eg including the specified hour.
// Always returns false if all samples are the same.
//   * inTop  test for membership of the top quartile if true, bottom quartile if false
//   * statsSet  stats set number to use.
//   * hour  hour of day to use or STATS_SPECIAL_HOUR_CURRENT_HOUR for current hour or STATS_SPECIAL_HOUR_NEXT_HOUR for next hour
bool inOutlierQuartile(bool inTop, uint8_t statsSet, uint8_t hour = STATS_SPECIAL_HOUR_CURRENT_HOUR);

// Compute the number of stats samples in specified set less than the specified value; returns -1 for invalid stats set.
// (With the UNSET value specified, count will be of all samples that have been set, ie are not unset.)
int8_t countStatSamplesBelow(uint8_t statsSet, uint8_t value);

#endif // ARDUINO_ARCH_AVR


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
