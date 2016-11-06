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

