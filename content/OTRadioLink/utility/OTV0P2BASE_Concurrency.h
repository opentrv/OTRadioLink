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

Author(s) / Copyright (s): Damon Hart-Davis 2016
*/

/*
 Portable concurrency/atomicity support that should work for small MCUs and bigger platforms.

 Has some fairly ugly stuff in the header to allow
 fast, efficient, hardware-specific support.

 To some extent modelled on Java and C++ support,
 eg java.util.concurrent.atomic.AtomicReference and std::atomic.
 Actual MCU implementations are likely to be heavily restricted subsets and hand-optimised.

 See: https://docs.oracle.com/javase/7/docs/api/java/util/concurrent/atomic/AtomicReference.html
 See: http://en.cppreference.com/w/cpp/atomic/atomic
 */

#ifndef OTV0P2BASE_CONCURRENCY_H
#define OTV0P2BASE_CONCURRENCY_H

#include <stdint.h>
#include <OTV0p2Base.h>

#ifndef ARDUINO
// Non-Arduino platforms may have grown-up concurrency support.
#include <atomic>
#define OTV0P2BASE_PLATFORM_HAS_atomic
#endif

namespace OTV0P2BASE
{

// A uint8_t atomic value.
#ifdef OTV0P2BASE_PLATFORM_HAS_atomic
    // Default is to use the std::atomic where it exists, eg for hosted test cases.
    typedef std::atomic<std::uint8_t> Atomic_UInt8T;
#elif defined(ARDUINO_ARCH_AVR)
    struct Atomic_UInt8T
        {
        public:
            // Direct access to value.
            // Use sparingly, eg where concurrency is not an issue on an MCU, eg with interrupts locked out.
            uint8_t value;

            Atomic_UInt8T() { }
            Atomic_UInt8T(uint8_t v) : value(v) { }
        };
#endif // OTV0P2BASE_PLATFORM_HAS_atomic ... defined(ARDUINO_ARCH_AVR)

//class Atomic
//  {
//  public:
//    // Supports single-byte atomic compare and set; true if successful
//    // Atomically sets the target to the given updated value if the current value equals the expected value.
//    //  * expect  expected value
//    //  * update  new value
//    static bool compareAndSet(volatile uint8_t &target, uint8_t expect, uint8_t update);
//  }

}

#endif
