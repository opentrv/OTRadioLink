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

Author(s) / Copyright (s): Damon Hart-Davis 2017
*/

/*
 * Driver for OTV0p2Base SystemStatsLine tests.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>
#include <OTRadValve.h>

#include "OTV0P2BASE_SystemStatsLine.h"


// Test basic instance creation, etc.
namespace Basics
    {
    // Working buffer for tests.
    static char buf[80];
    static OTV0P2BASE::BufPrint bp(buf, sizeof(buf));
    // Inputs/controls for stats report.
    static OTRadValve::ValveMode valveMode;
    }
TEST(SystemStatsLine,Basics)
{
    // Reset inputs/controls for stats report so test is idempotent.
    Basics::bp.reset();
    Basics::valveMode.reset();

//    // Should not compile with no Print channel on non-V0p2 platforms.
//    OTV0P2BASE::SystemStatsLine<> ssl0Bad;

//    // Should not compile because of NULL output channel.
//    // Should not compile on non-V0p2 platforms because of true 'flush' param.
//    OTV0P2BASE::SystemStatsLine<(Print *)NULL, true> sslBad;
//    sslBad.serialStatusReport();

    // Create stats line wrapped round simple bounded buffer.
    OTV0P2BASE::SystemStatsLine<
        decltype(Basics::valveMode), &Basics::valveMode,
        decltype(Basics::bp), &Basics::bp> ssl1;

    // Buffer should remain empty before any explicit activity.
    ASSERT_EQ(0, Basics::bp.getSize());
    ASSERT_EQ('\0', Basics::buf[0]);

    // Generate a stats line.
    ssl1.serialStatusReport();

    // Buffer should contain status line starting with '='.
    ASSERT_LE(1, Basics::bp.getSize());
    ASSERT_EQ(OTV0P2BASE::SERLINE_START_CHAR_STATS, Basics::buf[0]);

    // First char after '=' should be mode, in this case 'F'.
    ASSERT_LE(2, Basics::bp.getSize());
    ASSERT_EQ('F', Basics::buf[1]);

}

