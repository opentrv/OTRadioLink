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
 * OTRadValve ValveMode tests.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <OTV0P2BASE_QuickPRNG.h>

#include "OTRadValve_ValveMode.h"


// Test for general sanity of ValveMode.
TEST(ValveMode,basics)
{
//    // If true then be more verbose.
//    const static bool verbose = false;

    // Test new/actuator API.
    OTRadValve::ValveMode vmn;
    EXPECT_EQ(OTRadValve::ValveMode::VMODE_FROST, vmn._get()) << "should start in frost-protection mode";
    for(int i = OTRadValve::ValveMode::VMODE_BAKE; i >= OTRadValve::ValveMode::VMODE_FROST; --i)
        {
        OTRadValve::ValveMode vm;
        vm.set(i);
        EXPECT_EQ(i, vm._get());
        }

    // Test old/discrete API.
    OTRadValve::ValveMode vmo;
    EXPECT_FALSE(vmo.inWarmMode()) << "should start in frost-protection mode";
    EXPECT_FALSE(vmo.inBakeMode()) << "should start in frost-protection mode";

    vmo.setWarmModeDebounced(false);
    EXPECT_FALSE(vmo.inWarmMode()) << "should stay in frost-protection mode";
    EXPECT_FALSE(vmo.inBakeMode()) << "should stay in frost-protection mode";

    vmo.setWarmModeDebounced(true);
    EXPECT_TRUE(vmo.inWarmMode()) << "should be in WARM mode";
    EXPECT_FALSE(vmo.inBakeMode()) << "should not be in BAKE mode";

    vmo.setWarmModeDebounced(false);
    EXPECT_FALSE(vmo.inWarmMode()) << "should revert to frost-protection mode";
    EXPECT_FALSE(vmo.inBakeMode()) << "should revert to frost-protection mode";

    vmo.startBake();
    EXPECT_TRUE(vmo.inWarmMode()) << "should be in WARM mode";
    EXPECT_TRUE(vmo.inBakeMode()) << "should be in BAKE mode";
    vmo.read();
    EXPECT_EQ(OTRadValve::ValveMode::VMODE_BAKE, vmo._get());
    EXPECT_TRUE(vmo.inWarmMode()) << "should still be in WARM mode";
    EXPECT_TRUE(vmo.inBakeMode()) << "should still be in BAKE mode";
    for(int i = 0; i <= OTRadValve::DEFAULT_BAKE_MAX_M; ++i) { vmo.read(); }
    EXPECT_TRUE(vmo.inWarmMode()) << "should stay in WARM mode";
    EXPECT_FALSE(vmo.inBakeMode()) << "should have timed out BAKE mode";
    EXPECT_EQ(OTRadValve::ValveMode::VMODE_WARM, vmo._get());
}


