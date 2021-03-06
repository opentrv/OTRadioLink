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
 * Driver for OTV0p2Base RTC tests.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>

#include "OTV0P2BASE_RTC.h"


// Minimally test getElapsedSecondsLT(old, now).
TEST(RTC,getElapsedSecondsLT)
{
    EXPECT_EQ(0, OTV0P2BASE::getElapsedSecondsLT(0, 0));
    EXPECT_EQ(0, OTV0P2BASE::getElapsedSecondsLT(30, 30));
    EXPECT_EQ(0, OTV0P2BASE::getElapsedSecondsLT(59, 59));
    EXPECT_EQ(10, OTV0P2BASE::getElapsedSecondsLT(49, 59));
    EXPECT_EQ(10, OTV0P2BASE::getElapsedSecondsLT(59, 9));
    EXPECT_EQ(59, OTV0P2BASE::getElapsedSecondsLT(0, 59));
    EXPECT_EQ(59, OTV0P2BASE::getElapsedSecondsLT(1, 0));
    EXPECT_EQ(59, OTV0P2BASE::getElapsedSecondsLT(2, 1));
}

