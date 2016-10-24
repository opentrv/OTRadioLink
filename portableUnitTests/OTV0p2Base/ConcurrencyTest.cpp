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
 * Driver for OTV0p2Base concurrency tests.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>

#include "OTV0P2BASE_Concurrency.h"


// Tests some basic features of Atomic_UInt8T.
TEST(Concurrency,AtomicUInt8T)
{
    volatile OTV0P2BASE::Atomic_UInt8T v0(0);
    EXPECT_EQ(0, v0.load());
    OTV0P2BASE::safeDecIfNZWeak(v0);
    EXPECT_EQ(0, v0.load());
    volatile OTV0P2BASE::Atomic_UInt8T v1(1);
    EXPECT_EQ(1, v1.load());
    OTV0P2BASE::safeDecIfNZWeak(v1); // In practice not expected to fail.
    EXPECT_EQ(0, v1.load());
    // Test initialisation, load/store, decrement.
    for(uint8_t i = 255; i > 0; --i)
        {
        volatile OTV0P2BASE::Atomic_UInt8T v(i);
        EXPECT_EQ(i, v.load());
        OTV0P2BASE::safeDecIfNZWeak(v); // In practice not expected to fail.
        EXPECT_EQ(i-1, v.load());
        volatile OTV0P2BASE::Atomic_UInt8T w;
        w.store(i); // Test explicit store.
        EXPECT_EQ(i, w.load());
        OTV0P2BASE::safeDecIfNZWeak(w); // In practice not expected to fail.
        EXPECT_EQ(i-1, w.load());
        }
}

