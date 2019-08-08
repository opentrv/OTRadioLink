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

Author(s) / Copyright (s): Damon Hart-Davis 2014--2017
                           Deniz Erbilgin 2017
*/

/*
 Simple utilities.
 */

#ifndef OTV0P2BASE_UTIL_H
#define OTV0P2BASE_UTIL_H

#include <stdint.h>
#include <stddef.h>
#include "OTV0P2BASE_Concurrency.h"
#include "OTV0P2BASE_ErrorReport.h"

#ifdef ARDUINO
#include <Arduino.h>
//#include <util/atomic.h>
#include "OTV0P2BASE_Sleep.h"
#elif defined(EFR32FG1P133F256GM48)
#include "OTV0P2BASE_Sleep.h"  // TODO
#else
#include <string.h>
#include "utility/OTV0P2BASE_ArduinoCompat.h"
#endif

// include a coarse, watchdog based profiling routine
#undef OTMEMCHECKS_FREQ_PROFILING
#undef OTMEMCHECKS_TIME_PROFILING


#ifdef __APPLE__
// OSX does not provide _etext, _edata or _end as variables, but instead
// provides get_etext(), get_edata() and get_end() functions to return their
// addresses.
#include "mach-o/getsect.h"
#else
extern "C" {
extern char _end;
}
// Function that returns the address of _end to ease support with OSX.
static size_t get_end() { return ((size_t)&_end); }
#endif

namespace OTV0P2BASE
{


// Templated function versions of min()/max() that do not evaluate the arguments twice.
template <class T> constexpr const T& fnmin(const T& a, const T& b) { return((a>b)?b:a); }
template <class T> constexpr const T& fnmax(const T& a, const T& b) { return((a<b)?b:a); }

// Compatible non-macro constexpr fn replacement for Arduino constrain().
// Constrains x to inclusive range [l,h].
template <class T> constexpr const T& fnconstrain(const T& x, const T& l, const T& h) { return((x<l)?l:((x>h)?h:x)); }

// Absolute difference.
// Requires < and - in order to work.
template <class T> constexpr const T fnabsdiff(const T& a, const T& b) { return((a<b)?(b-a):(a-b)); }

// Absolute value.
// Requires < and unary - in order to work.
template <class T> constexpr const T fnabs(const T& a) { return((a<0)?(-a):(a)); }

// Empty struct type as a placeholder.
struct emptyStruct { };

// Extract ASCII hex digit in range [0-9][a-f] (ie lowercase) from bottom 4 bits of argument.
// Eg, passing in 0xa (10) returns 'a'.
// The top 4 bits are ignored.
inline char hexDigit(const uint8_t value) { const uint8_t v = 0xf&value; if(v<10) { return(char('0'+v)); } return(char('a'+(v-10))); }
//inline char hexDigit(const uint8_t value) { const uint8_t v = *("0123456789abcdef" + (0xf&value)); }
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


// Modelled on the Linux likely()/unlikely() macros.
// Static hint whether branch is expected likely to be taken or not.
// Works for g++ and clang.
#if defined(__GNUG__) || defined(__clang__)
#define BRANCH_HINT_likely(x)       __builtin_expect(!!(x),1)
#define BRANCH_HINT_unlikely(x)     __builtin_expect(!!(x),0)
#else
#define BRANCH_HINT_likely(x)       (x)
#define BRANCH_HINT_unlikely(x)     (x)
#endif


// Large scratch space that can be passed into callers to trim stack usage.
// Possible to create tail end for use by nested callers
// where a routine needs to keep some state during those calls.
// Scratch size is limited to 255 bytes.
class ScratchSpaceL final
  {
  public:
    // Buffer space; non-NULL except in case of error (when bufsize will also be 0).
    uint8_t *const buf;
    // Buffer size; strictly positive except in case of error (when buf will also be NULL).
    const size_t bufsize;

    // R/W access to buf.
    uint8_t *getBuf() { return buf; }
    // RO accessor for when ScratchSpace is const
    // NOTE not currently safe.
    const uint8_t *getBuf() const { return buf; }

    // Create an instance.
    //   * buf_  start of buffer space;#ifdef OTWATCHDOG_PROFILING
    //     must be non-NULL except to indicate that the buffer is unusable.
    //   * bufsize_  size of usable start of buffer;
    //     must be positive except to indicate that the buffer is unusable.
    constexpr ScratchSpaceL(uint8_t *const buf_, const size_t bufsize_)
      : buf((0 == bufsize_) ? NULL : buf_), bufsize((NULL == buf_) ? 0 : bufsize_) { }

    // Check if sub-space cannot be made (would not leave at least one byte available).
    static constexpr bool subSpaceCannotBeMade(size_t oldSize, size_t reserveN)
        { return((0 == reserveN) || (oldSize <= reserveN)); }
    // Create a sub-space n bytes from the start of the current space.
    // If the existing buffer is smaller than n (or null), or n is null,
    // the the result will be NULL and zero-sized.
    constexpr ScratchSpaceL(const ScratchSpaceL &parent, const size_t reserveN)
      : buf(subSpaceCannotBeMade(parent.bufsize, reserveN) ? NULL : parent.buf + reserveN),
        bufsize(subSpaceCannotBeMade(parent.bufsize, reserveN) ? 0 : parent.bufsize - reserveN)
        { }
  };

// Class for scratch space that can be passed into callers to trim stack usage.
// Possible to create tail end for use by nested callers
// where a routine needs to keep some state during those calls.
// Scratch size is limited to 255 bytes.
template<typename buf_T>
class ScratchSpaceTemplate final
  {
  public:
    // Buffer space; non-NULL except in case of error (when bufsize will also be 0).
    // DEPRECATED!!!!
    buf_T *const buf;
    // Buffer size; strictly positive except in case of error (when buf will also be NULL).
    const uint8_t bufsize;

    // R/W access to buf.
    buf_T *getBuf() { return buf; }
    // RO accessor for when ScratchSpace is const
    // NOTE not currently safe.
    const buf_T *getBuf() const { return buf; }

    // Create an instance.
    //   * buf_  start of buffer space;
    //     must be non-NULL except to indicate that the buffer is unusable.
    //   * bufsize_  size of usable start of buffer;
    //     must be positive except to indicate that the buffer is unusable.
    constexpr ScratchSpaceTemplate(buf_T *const buf_, const uint8_t bufsize_)
      : buf((0 == bufsize_) ? NULL : buf_), bufsize((NULL == buf_) ? 0 : bufsize_) { }

    // Check if sub-space cannot be made (would not leave at least one byte available).
    static constexpr bool subSpaceCannotBeMade(uint8_t oldSize, uint8_t reserveN)
        { return((0 == reserveN) || (oldSize <= reserveN)); }
    // Create a sub-space n bytes from the start of the current space.
    // If the existing buffer is smaller than n (or null), or n is null,
    // the the result will be NULL and zero-sized.
    constexpr ScratchSpaceTemplate(const ScratchSpaceTemplate<buf_T> &parent, const uint8_t reserveN)
      : buf(subSpaceCannotBeMade(parent.bufsize, reserveN) ? NULL : parent.buf + reserveN),
        bufsize(subSpaceCannotBeMade(parent.bufsize, reserveN) ? 0 : parent.bufsize - reserveN)
        { }

    // Wrap a sub-space round a large space.
    // At most 255 bytes will be available in the new sub-space.
    constexpr ScratchSpaceTemplate(const ScratchSpaceL &parent)
      : buf(parent.buf),
        bufsize((parent.bufsize > 255) ? 255 : uint8_t(parent.bufsize))
        { }
  };

using ScratchSpace = ScratchSpaceTemplate<uint8_t>;

// Diagnostic tools for memory problems.
// Arduino AVR memory layout: DATA, BSS [_end, __bss_end], (HEAP,) [SP] STACK [RAMEND]
// See: http://web-engineering.info/node/30
#ifdef ARDUINO_ARCH_AVR
// Get the stack pointer and return as a size_t.
// Prefered AVR way reads stack pointer register
// This is a hack to hide differences between AVR-GCC and CI environments.
inline size_t getSP() { return ((size_t)SP); }
#else
//  Dummy variable to hold stack pointer.
// Required for recordIfMinSP to function properly.
// Assuming stack grows downwards, MUST be set to a higher number than the highest possible address used by the program.
// TODO replace with constexpr containing the highest available RAM address.
extern size_t RAMEND;
// Get the stack pointer and return as a size_t.
// If not on AVR, create new local variable and get its address.
#if 1
inline size_t getSP() {
    size_t position = (size_t)&position;
    return (position);
}
#endif
#if 0
// GCC specific version
inline size_t getSP() {
    return ((size_t)__builtin_frame_address (0));
}
#endif
#if 0
// x86 specific version
inline size_t getSP() {
    register size_t sp asm ("sp");
    return sp;
}
#endif
#endif  // ARDUINO_ARCH_AVR

#if !defined(ARDUINO_ARCH_AVR) && !defined(__arm__)
// Stub function for forceReset() when not on embedded processor
// TODO Is there a better place for this?
inline void forceReset() {}
#endif  // !defined(ARDUINO_ARCH_AVR) && !defined(__arm__)

#define MemoryChecks_DEFINED
// Requires ATOMIC_BLOCK and ATOMIC_RESTORESTATE to be defined on non AVR architectures.
// On non-AVR architectures, resetMinSP() should be called before doing anything else.
class MemoryChecks
  {
  private:
    // Minimum value recorded for SP.
    // Marked volatile for safe access from ISRs.
    static volatile OTV0P2BASE::OTAtomic_t<size_t> minSP;
    // Stores which call to recordIfMinSP minsp was recorded at.
    // Defaults to 0
    static volatile uint8_t checkLocation;
    // Stores the program counter.
    // This value must be multiplied by 2 to get the values in the disassembler.
    static volatile OTV0P2BASE::OTAtomic_t<size_t> tempProgramCounter;
    static volatile size_t programCounter;

  public:
    // Minimum stack space remaining before a warning is issued.
    static constexpr uint8_t MIN_ALLOWABLE_STACK_SPACE {64};

    // Store return address of the calling function (Assumes recordPC is correctly inlined)
    static inline void recordPC() { tempProgramCounter.store((size_t)__builtin_return_address(0)); }
    // Compute stack space in use on ARDUINO/AVR; non-negative.
    static size_t stackSpaceInUse() { return((size_t)RAMEND - getSP()); }
    // Compute space after DATA and BSS (_end) and below STACK (ignoring HEAP) on ARDUINO/AVR; should be strictly +ve.
    // If this becomes non-positive then variables are likely being corrupted.
    static intptr_t spaceBelowStackToEnd() { return((getSP() - (intptr_t)get_end())); }

    // Reset SP minimum: ISR-safe.
    static void resetMinSP() { minSP.store(RAMEND); checkLocation = 0; programCounter = 0;}
    // Record current SP if minimum: ISR-safe.
    // Can be buried in parts of code prone to deep recursion.
    // Location defaults to 0 but can be assigned a value for the particular stack check to aid debug.
    // NOTE:
    // - On AVRs checkLocation may be overwritten with an incorrect value if recordIfMinSP is called in an interrupt
    //   after the if statement but before writing checkLocation. In this case, minSP will be set by the call in the
    //   interrupt but checkLocation will be set by the original call (i.e. outside the interrupt).
    //   FIXME Ignored for now as is a minor problem and fixing it is not worth the effort and will be too complicated (DE20170504)
    // - In the case setting minSP fails, it is assumed that its value was changed in an interrupt and therefore is
    //   "more correct."
    static void recordIfMinSP(uint8_t location = 0) {
        const size_t pos = getSP();
        size_t min = minSP.load();
        if(pos < min) {
            checkLocation = location;
            minSP.compare_exchange_strong(min, pos);
            programCounter = tempProgramCounter.load();
            // Set a warning if necessary
            // FIXME: Should only be triggered if the high watermark has increased. Are these semantics correct?
            const auto remainingStack = min - (intptr_t)get_end();
            if(MIN_ALLOWABLE_STACK_SPACE > remainingStack) { ErrorReporter.set(ErrorReport::WARN_STACK_SPACE_LOW); }
        }
    }
    // Get SP minimum: ISR-safe.
    static size_t getMinSP() { return(minSP.load()); }
    // Get minimum space below SP above _end: ISR-safe.
    static intptr_t getMinSPSpaceBelowStackToEnd() { return(minSP.load() - (intptr_t)get_end()); }
    // Force restart if minimum space below SP has not remained strictly positive.
    static void forceResetIfStackOverflow() { if(getMinSPSpaceBelowStackToEnd() <= 0) { forceReset(); } }
    // Get the identifier for location of stack check with highest stack usage,
    static uint8_t getLocation() { return checkLocation; }

    // Return the program counter recordPC was called and when a new interrupt is called.
    // NOTE: - program counter is not guaranteed correspond to when the max stack was seen.
    //       - program counter may be corrupted when being written from tempProgramCounter.
    //         The likelyhood of this occuring is low and it will have no effect on correct
    //         functioning of V0p2 so it is ignored.
    // stored counter is multiplied by 2 to to correspond to disassembly output.
    static size_t getPC() {return programCounter * 2;}

#ifdef OTMEMCHECKS_FREQ_PROFILING
  public:
    static constexpr uint8_t callTableSize = 8;
  private:
    // coarse profiling
    // Arrays to store maximum and minimum calls by various functions.
    // Will work as long as functions are not called more than 255 times - highly unlikely in V0p2.
    static volatile uint8_t curCalls[callTableSize];
    static volatile uint8_t maxCalls[callTableSize];
    static volatile uint8_t minCalls[callTableSize];
  public:
    // Reset call table to appropriate values (curCalls and maxCalls should grow upwards, minCalls should grow downwards)
    // Assuming that we don't have to worry about overflowing in this instance, to avoid needing atomics.
    static void initCallTable() { memset ((uint8_t * const)curCalls, 0, sizeof curCalls); memset((uint8_t * const)maxCalls, 0, sizeof(maxCalls)); memset((uint8_t * const)minCalls, 255, sizeof(minCalls)); };

    // Update maxCalls and minCalls then clear curCalls.
    // This should be called by the watchdog routine in OTRadioLink_RTC.h/cpp
    static void resetCallTable()
    {
        uint8_t *maxp = (uint8_t *)maxCalls;
        uint8_t *minp = (uint8_t *)minCalls;
        for( uint8_t * ip = (uint8_t *)curCalls; ip != ( curCalls + sizeof(curCalls) );) {
            *maxp = OTV0P2BASE::fnmax(*maxp, *ip);
            *minp = OTV0P2BASE::fnmin(*minp, *ip);
            *ip = 0;  // Since we're iterating through it, reset currCalls as we go.
            // Point to next entries.
            ++ip; ++maxp; ++minp;
        }
    }

    /**
     * @brief   Increments current calls table.
     * @note    Location Table:
     *           0: loop
     *           1: ISR(PCINT0_vect)
     *           2: pollIO
     *           3: decodeAndHandleSecureFrame
     *           4: bareStatsTX
     * @note    Calls table will overflow at 255 calls. Not a problem on V0p2
     *          but may require bounds checking/larger tables in other apps.
     */
    static void fnCalled(uint8_t loc) { ++curCalls[loc]; }

    /**
     * @brief   Copy the min and max calls into a pair of uint8_t arrays
     *          supplied by the caller.
     * @param   maxBuf: Array to copy maximum number number of calls into.
     *                  Minimum length is callTableSize.
     * @param   minBuf: Array to copy minimum number number of calls into.
     *                  Minimum length is callTableSize.
     */
    static void getCallTable(uint8_t * const maxBuf, uint8_t * const minBuf)
        {memcpy(maxBuf, (const uint8_t *)maxCalls, sizeof(maxCalls)); memcpy(minBuf, (const uint8_t *)minCalls, sizeof(minCalls));}
#else // OTMEMCHECKS_FREQ_PROFILING
    // stub versions of profiling functions
    static void initCallTable() {}
    static void resetCallTable() {}
    static void fnCalled(uint8_t) {}
    static void getCallTable(uint8_t *const, uint8_t *const) {}
#endif // OTMEMCHECKS_FREQ_PROFILING
#ifdef OTMEMCHECKS_TIME_PROFILING
  public:
    static constexpr uint8_t timeTableSize = 8;
  private:
    // coarse profiling
    // Arrays to store maximum and minimum calls by various functions.
    // Will work as long as functions are not called more than 255 times - highly unlikely in V0p2.
    static volatile uint8_t tempStartTime[timeTableSize];
    static volatile uint8_t startTime[timeTableSize];
    static volatile uint8_t duration[timeTableSize];
  public:
    // Reset call table to appropriate values (curCalls and maxCalls should grow upwards, minCalls should grow downwards)
    // Assuming that we don't have to worry about overflowing in this instance, to avoid needing atomics.
    static void initTimeTable() { memset((uint8_t * const)tempStartTime, 0, sizeof(tempStartTime)); memset ((uint8_t * const)startTime, 0, sizeof startTime); memset((uint8_t * const)duration, 0, sizeof(duration)); };

//    // Update maxCalls and minCalls then clear curCalls.
//    // This should be called by the watchdog routine in OTRadioLink_RTC.h/cpp
//    static void resetTimeTable()
//    {
//        uint8_t *maxp = (uint8_t *)maxCalls;
//        uint8_t *minp = (uint8_t *)minCalls;
//        for( uint8_t * ip = (uint8_t *)curCalls; ip != ( curCalls + sizeof(curCalls) );) {
//            *maxp = OTV0P2BASE::fnmax(*maxp, *ip);
//            *minp = OTV0P2BASE::fnmin(*minp, *ip);
//            *ip = 0;  // Since we're iterating through it, reset currCalls as we go.
//            // Point to next entries.
//            ++ip; ++maxp; ++minp;
//        }
//    }

    /**
     * @brief   Increments current calls table.
     * @note    Location Table:
     *           0: loop
     *           1: ISR(PCINT0_vect)
     *           2: pollIO
     *           3: decodeAndHandleSecureFrame
     *           4: bareStatsTX
     * @note    Calls table will overflow at 255 calls. Not a problem on V0p2
     *          but may require bounds checking/larger tables in other apps.
     */
    static void fnStart(uint8_t loc) {
        tempStartTime[loc] = OTV0P2BASE::getSubCycleTime();
    }
    static void fnExit(uint8_t loc) {
        const uint8_t curTime = OTV0P2BASE::getSubCycleTime();
        const uint8_t temp = tempStartTime[loc];
        volatile uint8_t &start = startTime[loc];
        volatile uint8_t &dur = duration[loc];
        const uint8_t curDur = curTime - temp;
        if(255 < (uint16_t)(start + dur)) { return; }
        if(curDur > dur) { start = temp; dur = curDur;}
//        volatile uint8_t *const start = &startTime[loc];
//        volatile uint8_t *const dur = &duration[loc];
//        const uint8_t curDur = curTime - temp;
//        if(255 < (uint16_t)(*start + *dur)) { return; }
//        if(curDur > *dur) { *start = temp; *dur = curDur;}
    }

    /**
     * @brief   Copy the min and max calls into a pair of uint8_t arrays
     *          supplied by the caller.
     * @param   maxBuf: Array to copy maximum number number of calls into.
     *                  Minimum length is callTableSize.
     * @param   minBuf: Array to copy minimum number number of calls into.
     *                  Minimum length is callTableSize.
     */
    static void getTimeTable(uint8_t * const startTimeBuf, uint8_t * const durBuf)
        {memcpy(startTimeBuf, (const uint8_t *)startTime, sizeof(startTime)); memcpy(durBuf, (const uint8_t *)duration, sizeof(duration));}
//#else // OTMEMCHECKS_TIME_PROFILING
#endif
};

/**
 * @brief   Force restart if SPAM/heap/stack likely corrupt.
 *          Complain and keep complaining when getting near stack overflow.
 *          TODO: make DEBUG-only when confident all configs OK.
 *          Optionally reports max stack usage and location, per loop.
 */
void stackCheck();

}

#endif
