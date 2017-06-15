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
 * Driver for OTV0p2Base Util tests.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>

#include "OTV0P2BASE_Util.h"


// Test the stack usage of empty function calls in CI
namespace OTCISU
{
    // (DE20170615) Each fn call consumes 16 bytes stack on x64 Linux w/ -O0
    static constexpr uint_fast8_t maxStackEmptyFn = 20;
    static constexpr uint_fast8_t maxStackCallEmptyFn = 40;
    void emptyFn() { OTV0P2BASE::MemoryChecks::recordIfMinSP(); }
    void callEmptyFn() { emptyFn(); }
}
TEST(CIStackUsage, emptyFn)
{
    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();

    OTCISU::emptyFn();
    const size_t maxStack = OTV0P2BASE::MemoryChecks::getMinSP();
    // Uncomment to print stack usage
//    std::cout << baseStack - maxStack << "\n";
    EXPECT_GT(OTCISU::maxStackEmptyFn, baseStack - maxStack);
}

TEST(CIStackUsage, callEmptyFn)
{
    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();

    OTCISU::callEmptyFn();

    const size_t maxStack = OTV0P2BASE::MemoryChecks::getMinSP();
    // Uncomment to print stack usage
//    std::cout << baseStack - maxStack << "\n";
    EXPECT_GT(OTCISU::maxStackCallEmptyFn, baseStack - maxStack);
}

// Minimally test ScratchSpace.
TEST(ScratchSpace,basics)
{
    OTV0P2BASE::ScratchSpace ss1(NULL, 0);
    EXPECT_EQ(NULL, ss1.buf);
    EXPECT_EQ(0, ss1.bufsize);
    OTV0P2BASE::ScratchSpace ss2(NULL, 1);
    EXPECT_EQ(NULL, ss2.buf);
    EXPECT_EQ(0, ss2.bufsize);
    uint8_t buf[42];
    OTV0P2BASE::ScratchSpace ss3(buf, 0);
    EXPECT_EQ(NULL, ss3.buf);
    EXPECT_EQ(0, ss3.bufsize);
    OTV0P2BASE::ScratchSpace ss4(buf, sizeof(buf));
    EXPECT_EQ(buf, ss4.buf);
    EXPECT_EQ(sizeof(buf), ss4.bufsize);

    // Now create a sub-scratch-space.
    OTV0P2BASE::ScratchSpace sss1(ss1, 0);
    EXPECT_EQ(NULL, sss1.buf);
    EXPECT_EQ(0, sss1.bufsize);
    OTV0P2BASE::ScratchSpace sss2(ss4, 0);
    EXPECT_EQ(NULL, sss2.buf);
    EXPECT_EQ(0, sss2.bufsize);
    OTV0P2BASE::ScratchSpace sss3(ss4, 2*sizeof(buf));
    EXPECT_EQ(NULL, sss3.buf);
    EXPECT_EQ(0, sss3.bufsize);
    OTV0P2BASE::ScratchSpace sss4(ss4, 4);
    EXPECT_EQ(buf+4, sss4.buf);
    EXPECT_EQ(sizeof(buf)-4, sss4.bufsize);
}

