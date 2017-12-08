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
                           Deniz Erbilgin   2017
*/

/*
 * OTRadValve Actuator behaviour tests.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>

#include "OTRadValve_AbstractRadValve.h"

// Test BinaryRelayDirect for off by 1 error.
// With default value, should set pin low (false) when target valve pc >= 50.
TEST(BinaryRelayDirect, ToggleBehaviourTest) {
    // Try values
    EXPECT_FALSE(OTRadValve::BinaryRelayHelper::calcRelayState(0U));
    EXPECT_FALSE(OTRadValve::BinaryRelayHelper::calcRelayState(49U));
    EXPECT_TRUE(OTRadValve::BinaryRelayHelper::calcRelayState(50U));
    EXPECT_TRUE(OTRadValve::BinaryRelayHelper::calcRelayState(99U));
    EXPECT_TRUE(OTRadValve::BinaryRelayHelper::calcRelayState(100U));
    EXPECT_TRUE(OTRadValve::BinaryRelayHelper::calcRelayState(127U));
    EXPECT_TRUE(OTRadValve::BinaryRelayHelper::calcRelayState(255U));
}