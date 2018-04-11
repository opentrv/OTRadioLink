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

Author(s) / Copyright (s): Damon Hart-Davis 2016--2017
*/

/*
 Portable concurrency/atomicity support that should work for small MCUs and bigger platforms.

 Has some fairly ugly stuff in the header to allow
 fast, efficient, hardware-specific support.

 To some extent modelled on Java and C++ support,
 eg java.util.concurrent.atomic.AtomicReference and std::atomic.
 Actual MCU implementations are likely to be heavily restricted subsets and hand-optimised.

 Mainly intended to support (volatile) values shared by ISR routines
 in a small number of idioms;
 NOT a general-purpose complete set of possible actions.

 See: https://docs.oracle.com/javase/7/docs/api/java/util/concurrent/atomic/AtomicReference.html
 See: http://en.cppreference.com/w/cpp/atomic/atomic
 */

#ifndef OTV0P2BASE_CONCURRENCY_H
#define OTV0P2BASE_CONCURRENCY_H

#include <stdint.h>
//#include "OTV0P2BASE_Util.h"

#ifdef ARDUINO

#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#endif

#else
// Non-Arduino platforms may have grown-up concurrency support.
#include <atomic>
#define OTV0P2BASE_PLATFORM_HAS_atomic

#endif


namespace OTV0P2BASE
{


// Atomic values.
// Aim is to have something portable that can be replaced with STL objects where appropriate.
#ifdef OTV0P2BASE_PLATFORM_HAS_atomic
    // RAII style ATOMIC_BLOCK(RESTORE_STATE) on AVR.
    // WARNING! EXPERIMENTAL!
    // Create instance where interrupts should be locked out,
    // interrupts will be restored to previous state at end of scope.
    // Stub implementation for unit testing.
    class RAII_AtomicBlock final
    {
    public:
        RAII_AtomicBlock() {}
        ~RAII_AtomicBlock() {}
    };


    // Default is to use the std::atomic where it exists, eg for hosted test cases.
    template<typename T>
    using OTAtomic_t = std::atomic<T>;
#elif defined(ARDUINO_ARCH_AVR)

    // RAII style ATOMIC_BLOCK(RESTORE_STATE) on AVR.
    // WARNING! EXPERIMENTAL!
    // Create instance where interrupts should be locked out,
    // interrupts will be restored to previous state at end of scope.
    class RAII_AtomicBlock final
    {
    public:
        const uint8_t savedSREG;
        // Saves the register containing the IRQ state and disables global interrupts.
        RAII_AtomicBlock() : savedSREG(SREG) { cli(); }
        // Restores IRQ state to what was saved in the constructor.
        ~RAII_AtomicBlock() { SREG = savedSREG; }
    };

    // OpenTRV version of std::atomic<> for use on AVR 8-bit arch.
    // Causes -Wreturn-type warnings due to ATOMIC_BLOCK macros.
    template <typename T>
    struct OTAtomic_t final
    {
        // Direct access to value.
        // Use sparingly, eg where concurrency is not an issue on an MCU, eg with interrupts locked out.
        // Marked volatile for ISR safely, ie to prevent cacheing of the value or re-ordering of access.
        volatile T value;

        // Create uninitialised value.
        OTAtomic_t() = default;
        // Create initialised value.
        constexpr OTAtomic_t(T v) noexcept : value(v) { }

        // Atomically load current value.
        // 16 bit operation requires 2 instructions on AVR.
        T load() const volatile noexcept
            { ATOMIC_BLOCK (ATOMIC_RESTORESTATE) { return(value); } }

        // Atomically load current value.
        // 16 bit operation requires 2 instructions on AVR.
        void store(T desired) volatile noexcept
            { ATOMIC_BLOCK (ATOMIC_RESTORESTATE) { value = desired; } }

        // Strong compare-and-exchange.
        // Atomically, if value == expected then replace value with desired and return true,
        // else load expected with value and return false.
        bool compare_exchange_strong(T& expected, T desired) volatile noexcept
        {
            // Lock out interrupts for a compound operation.
            ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
            {
                if(value == expected) { value = desired; return(true); }
                else { expected = value; return(false); }
            }
        }
    };
    // Specialisation for uint8_t types. Should be Identical to Atomic_UInt8T
    template <>
    struct OTAtomic_t<uint8_t> final
    {
        // Direct access to value.
        // Use sparingly, eg where concurrency is not an issue on an MCU, eg with interrupts locked out.
        // Marked volatile for ISR safely, ie to prevent cacheing of the value or re-ordering of access.
        volatile uint8_t value;

        // Create uninitialised value.
        OTAtomic_t() = default;
        // Create initialised value.
        constexpr OTAtomic_t(uint8_t v) noexcept : value(v) { }

        // Atomically load current value.
        // Relies on load/store of single byte being atomic on AVR.
        uint8_t load() const volatile noexcept { return(value); }

        // Atomically load current value.
        // Relies on load/store of single byte being atomic on AVR.
        void store(uint8_t desired) volatile noexcept { value = desired; }

        // Strong compare-and-exchange.
        // Atomically, if value == expected then replace value with desired and return true,
        // else load expected with value and return false.
        bool compare_exchange_strong(uint8_t& expected, uint8_t desired) volatile noexcept
            {
            // Lock out interrupts for a compound operation.
            ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
                {
                if(value == expected) { value = desired; return(true); }
                else { expected = value; return(false); }
                }
            }
    };
    // Specialisation for bool
    template <>
    struct OTAtomic_t<bool> final
    {
        // Direct access to value.
        // Use sparingly, eg where concurrency is not an issue on an MCU, eg with interrupts locked out.
        // Marked volatile for ISR safely, ie to prevent cacheing of the value or re-ordering of access.
        volatile bool value;

        // Create uninitialised value.
        OTAtomic_t() = default;
        // Create initialised value.
        constexpr OTAtomic_t(bool v) noexcept : value(v) { }

        // Atomically load current value.
        // Relies on load/store of single byte being atomic on AVR.
        bool load() const volatile noexcept { return(value); }

        // Atomically load current value.
        // Relies on load/store of single byte being atomic on AVR.
        void store(bool desired) volatile noexcept { value = desired; }

        // Strong compare-and-exchange.
        // Atomically, if value == expected then replace value with desired and return true,
        // else load expected with value and return false.
        bool compare_exchange_strong(bool& expected, bool desired) volatile noexcept
            {
            // Lock out interrupts for a compound operation.
            ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
                {
                if(value == expected) { value = desired; return(true); }
                else { expected = value; return(false); }
                }
            }
    };
#endif // OTV0P2BASE_PLATFORM_HAS_atomic ...
    // Atomic_UInt8T is kept to avoid changing current code.
    typedef OTAtomic_t<uint8_t> Atomic_UInt8T;


    // Helper method: safely decrements (volatile) Atomic_UInt8T arg if non-zero, ie does not wrap around.
    // Does nothing if already zero.
    // May do nothing if interrupted by or interleaved with other activity.
    // Does not loop or spin or block; may shut out interrupts briefly or similar on some platforms.
    // Safe because will never decrement value through zero, even in face of ISR/thread races.
    // Typically used by foreground (non-ISR) routines to decrement timers until zero.
    inline void safeDecIfNZWeak(volatile Atomic_UInt8T &v)
      {
      uint8_t o = v.load();
      if(0 == o) { return; }
      const uint8_t om1 = o - 1U;
      v.compare_exchange_strong(o, om1);
      }

    // Helper method: safely increments (volatile) Atomic_UInt8T arg if not maximum value, ie does not wrap around.
    // Does nothing if already maximum (0xff).
    // May do nothing if interrupted by or interleaved with other activity.
    // Does not loop or spin or block; may shut out interrupts briefly or similar on some platforms.
    // Safe because will never decrement value through zero, even in face of ISR/thread races.
    // Typically used by foreground (non-ISR) routines to increment timers until max.
    inline void safeIncIfNotMaxWeak(volatile Atomic_UInt8T &v)
      {
      uint8_t o = v.load();
      if(0xff == o) { return; }
      const uint8_t op1 = o + 1U;
      v.compare_exchange_strong(o, op1);
      }


}

#endif
