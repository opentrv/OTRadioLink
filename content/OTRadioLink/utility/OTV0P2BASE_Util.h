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

Author(s) / Copyright (s): Damon Hart-Davis 2014--2016
                           Deniz Erbilgin 2016
*/

/*
 Simple utilities.
 */

#ifndef OTV0P2BASE_UTIL_H
#define OTV0P2BASE_UTIL_H

#include <stdint.h>
#include <stddef.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <util/atomic.h>
extern uint8_t _end;
#include "OTV0P2BASE_Sleep.h"
#endif


namespace OTV0P2BASE
{


// Templated function versions of min()/max() that do not evaluate the arguments twice.
template <class T> constexpr const T& fnmin(const T& a, const T& b) { return((a>b)?b:a); }
template <class T> constexpr const T& fnmax(const T& a, const T& b) { return((a<b)?b:a); }

// Compatible non-macro constexpr fn replacement for Arduino constrain().
// Constrains x to inclusive range [l,h].
template <class T> constexpr const T& fnconstrain(const T& x, const T& l, const T& h) { return((x<l)?l:((x>h)?h:x)); }

// Absolute difference replacement for abs() macro.
// Requires < and - to work.
template <class T> constexpr const T fnabs(const T& a, const T& b) { return((a<b)?(b-a):(a-b)); }


// Extract ASCII hex digit in range [0-9][a-f] (ie lowercase) from bottom 4 bits of argument.
// Eg, passing in 0xa (10) returns 'a'.
// The top 4 bits are ignored.
inline char hexDigit(const uint8_t value) { const uint8_t v = 0xf&value; if(v<10) { return(char('0'+v)); } return(char('a'+(v-10))); }
//static inline char hexDigit(const uint8_t value) { const uint8_t v = *("0123456789abcdef" + (0xf&value)); }
// Fill in the first two bytes of buf with the ASCII hex digits of the value passed.
// Eg, passing in a value 0x4e sets buf[0] to '4' and buf[1] to 'e'.
inline void hexDigits(const uint8_t value, char * const buf) { buf[0] = hexDigit(value>>4); buf[1] = hexDigit(value); }


/**
 * @brief   Convert a single hex character into a 4 bit nibble
 * @param   ASCII character in ranges 0-9, a-f or A-F
 * @retval  nibble containing a value between 0 and 15; -1 in case of error
 */
inline int8_t parseHexDigit(const char hexchar)
{
  if((hexchar >= '0') && (hexchar <= '9')) { return(hexchar - '0'); }
  if((hexchar >= 'a') && (hexchar <= 'f')) { return(hexchar - 'a' + 10); }
  if((hexchar >= 'A') && (hexchar <= 'F')) { return(hexchar - 'A' + 10); }
  return(-1); // ERROR
}

/**
 * @brief   Convert 1-2 hex character string (eg "0a") into a binary value
 * @param   pointer to a token containing characters between 0-9, a-f or A-F
 * @retval  byte containing converted value [0,255]; -1 in case of error
 */
int parseHexByte(const char *s);


// Class for scratch space that can be passed into callers to trim stack/auto usage.
// Possible to create tail end for use by nested callers
// where a routine needs to keep some state during those calls.
// Scratch size is limited to 255 bytes.
class ScratchSpace final
  {
  public:
    // Buffer space; non-NULL except in case of error (when bufsize will also be 0).
    uint8_t *const buf;
    // Buffer size; strictly positive except in case of error (when buf will also be NULL).
    const uint8_t bufsize;

    // Create an instance.
    //   * buf_  start of buffer space;
    //     must be non-NULL except to indicate that the buffer is unusable.
    //   * bufsize_  size of usable start of buffer;
    //     must be positive except to indicate that the buffer is unusable.
    constexpr ScratchSpace(uint8_t *const buf_, const uint8_t bufsize_)
      : buf((0 == bufsize_) ? NULL : buf_), bufsize((NULL == buf_) ? 0 : bufsize_) { }

    // Check if sub-spacw cannot be made (would not leave at least one byte available).
    static constexpr bool subSpaceCannotBeMade(uint8_t oldSize, uint8_t reserveN)
        { return((0 == reserveN) || (oldSize <= reserveN)); }
    // Create a sub-space n bytes from the start of the current space.
    // If the existing buffer is smaller than n (or null), or n is null,
    // the the result will be NULL and zero-sized.
    constexpr ScratchSpace(const ScratchSpace &parent, const uint8_t reserveN)
      : buf(subSpaceCannotBeMade(parent.bufsize, reserveN) ? NULL : parent.buf + reserveN),
        bufsize(subSpaceCannotBeMade(parent.bufsize, reserveN) ? 0 : parent.bufsize - reserveN)
        { }
  };


#ifdef ARDUINO_ARCH_AVR
// Diagnostic tools for memory problems.
// Arduino AVR memory layout: DATA, BSS [_end, __bss_end], (HEAP,) [SP] STACK [RAMEND]
// See: http://web-engineering.info/node/30
#define MemoryChecks_DEFINED
class MemoryChecks
  {
  public:
     typedef uint16_t SP_type;

  private:
    // Minimum value recorded for SP.
    // Marked volatile for safe access from ISRs.
    // Initialised to be RAMEND.
    static volatile SP_type minSP;

  public:
    // Compute stack space in use on ARDUINO/AVR; non-negative.
    static uint16_t stackSpaceInUse() { return(RAMEND - SP); }
    // Compute space after DATA and BSS (_end) and below STACK (ignoring HEAP) on ARDUINO/AVR; should be strictly +ve.
    // If this becomes non-positive then variables are likely being corrupted.
    static int16_t spaceBelowStackToEnd() { return(SP - (intptr_t)&_end); }

    // Reset SP minimum: ISR-safe.
    static void resetMinSP() { ATOMIC_BLOCK (ATOMIC_RESTORESTATE) { minSP = RAMEND; } }
    // Record current SP if minimum: ISR-safe.
    // Can be buried in parts of code prone to deep recursion.
    static void recordIfMinSP() { ATOMIC_BLOCK (ATOMIC_RESTORESTATE) { if(SP < minSP) { minSP = SP; } } }
    // Get SP minimum: ISR-safe.
    static SP_type getMinSP() { ATOMIC_BLOCK (ATOMIC_RESTORESTATE) { return(minSP); } }
    // Get minimum space below SP above _end: ISR-safe.
    static int16_t getMinSPSpaceBelowStackToEnd() { ATOMIC_BLOCK (ATOMIC_RESTORESTATE) { return(minSP - (intptr_t)&_end); } }
    // Force restart if minimum space below SP has not remained strictly positive.
    static void forceResetIfStackOverflow() { if(getMinSPSpaceBelowStackToEnd() <= 0) { forceReset(); } }
};
#else
// Dummy do-nothing version to allow test bugs to be harmlessly dropped into portable code.
class MemoryChecks
  {
  public:
    static void recordIfMinSP() { }
    static void forceResetIfStackOverflow() { }
  };
#endif // ARDUINIO_ARCH_AVR


}

#endif
