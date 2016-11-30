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
 * Driver for OTV0P2BASE_SensorAmbientLight tests.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>
#include "OTV0P2BASE_SensorAmbientLight.h"


// Test some basics of the non-occupancy parts of the ambient light sensor.
TEST(AmbientLight,basics)
{
    OTV0P2BASE::SensorAmbientLightAdaptiveMock alm;
    const uint8_t dlt = OTV0P2BASE::SensorAmbientLightBase::DEFAULT_LIGHT_THRESHOLD;
    EXPECT_EQ(dlt, alm.getLightThreshold());
    EXPECT_GT(dlt, alm.getDarkThreshold());

    // Set to nominal minimum; should be dark.
    alm.set(0);
    alm.read();
    EXPECT_TRUE(alm.isAvailable());
    EXPECT_TRUE(alm.isRoomDark());
    EXPECT_FALSE(alm.isRoomLit());
    EXPECT_GE(1, alm.getDarkMinutes());

    // Set to pitch black; should be dark.
    alm.set(OTV0P2BASE::SensorAmbientLightBase::DEFAULT_PITCH_DARK_THRESHOLD);
    alm.read();
    EXPECT_TRUE(alm.isAvailable());
    EXPECT_TRUE(alm.isRoomDark());
    EXPECT_FALSE(alm.isRoomLit());
    EXPECT_GE(2, alm.getDarkMinutes());

    // Set to nominal maximum; should be light.
    alm.set(254);
    alm.read();
    EXPECT_TRUE(alm.isAvailable());
    EXPECT_FALSE(alm.isRoomDark());
    EXPECT_TRUE(alm.isRoomLit());
    EXPECT_EQ(0, alm.getDarkMinutes());

    // Set to actual maximum; should be light.
    alm.set(255);
    alm.read();
    EXPECT_TRUE(alm.isAvailable());
    EXPECT_FALSE(alm.isRoomDark());
    EXPECT_TRUE(alm.isRoomLit());
    EXPECT_EQ(0, alm.getDarkMinutes());

    // Check hysteresis.
    alm.set(alm.getLightThreshold() - 1);
    alm.read();
    EXPECT_TRUE(alm.isAvailable());
    EXPECT_FALSE(alm.isRoomDark());
    EXPECT_TRUE(alm.isRoomLit());
    EXPECT_EQ(0, alm.getDarkMinutes());
    alm.set(alm.getDarkThreshold() + 1);
    alm.read();
    EXPECT_TRUE(alm.isAvailable());
    EXPECT_FALSE(alm.isRoomDark());
    EXPECT_TRUE(alm.isRoomLit());
    EXPECT_EQ(0, alm.getDarkMinutes());
    alm.set(alm.getDarkThreshold());
    alm.read();
    EXPECT_TRUE(alm.isAvailable());
    EXPECT_TRUE(alm.isRoomDark());
    EXPECT_FALSE(alm.isRoomLit());
    EXPECT_EQ(1, alm.getDarkMinutes());
    alm.set(alm.getLightThreshold());
    alm.read();
    EXPECT_TRUE(alm.isAvailable());
    EXPECT_TRUE(alm.isRoomDark());
    EXPECT_FALSE(alm.isRoomLit());
    EXPECT_EQ(1, alm.getDarkMinutes());
    alm.set(alm.getLightThreshold() + 1);
    alm.read();
    EXPECT_TRUE(alm.isAvailable());
    EXPECT_FALSE(alm.isRoomDark());
    EXPECT_TRUE(alm.isRoomLit());
    EXPECT_EQ(0, alm.getDarkMinutes());
}

