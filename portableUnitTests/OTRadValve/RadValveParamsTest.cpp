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
 * OTRadValve parameter tests.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>
#include "OTRadValve_Parameters.h"


// Some basic tests of sanity of parameter block.
TEST(RadValveParams,paramBasics)
{
    typedef OTRadValve::DEFAULT_ValveControlParameters defaultParams;
    const int F = defaultParams::FROST;
    EXPECT_LT(0, F);
    EXPECT_LE(OTRadValve::MIN_TARGET_C, F);
    EXPECT_GE(OTRadValve::MAX_TARGET_C, F);
    const int W = defaultParams::WARM;
    EXPECT_LE(OTRadValve::MIN_TARGET_C, W);
    EXPECT_GE(OTRadValve::MAX_TARGET_C, W);
    EXPECT_LE(F, W);
    EXPECT_GE(32, W); // To meet BS EN 215:2004 S5.3.5.

    typedef OTRadValve::DEFAULT_DHW_ValveControlParameters DHWParams;
    const int DF = DHWParams::FROST;
    EXPECT_LT(0, DF);
    EXPECT_LE(OTRadValve::MIN_TARGET_C, DF);
    EXPECT_GE(OTRadValve::MAX_TARGET_C, DF);
    const int DW = DHWParams::WARM;
    EXPECT_LE(OTRadValve::MIN_TARGET_C, DW);
    EXPECT_GE(OTRadValve::MAX_TARGET_C, DW);
    EXPECT_LE(DF, DW);
    EXPECT_GE(90, W); // For safety.
}
