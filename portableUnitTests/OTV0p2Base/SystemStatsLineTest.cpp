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
    static OTRadValve::RadValveMock modelledRadValve;
    static OTV0P2BASE::TemperatureC16Mock tempC16;
    static OTV0P2BASE::PseudoSensorOccupancyTracker occupancy;
    static OTV0P2BASE::SensorAmbientLightAdaptiveMock ambLight;
    // Dummy (non-functioning) temperature and relative humidity sensors.
    static OTV0P2BASE::HumiditySensorMock rh;
    static OTRadValve::NULLValveSchedule schedule;
    }
TEST(SystemStatsLine,Basics)
{
    // Reset inputs/controls for stats report so test is idempotent.
    Basics::bp.reset();
    Basics::valveMode.reset();
    Basics::modelledRadValve.reset();
    Basics::tempC16.reset();
    Basics::rh.reset();
    Basics::ambLight.reset();
    Basics::occupancy.reset();

    // Set a reasonable room temperature.
    Basics::tempC16.set((18 << 4) + 14);
    // Set a reasonable RH%.
    Basics::rh.set(50);

    // Create stats line instance wrapped round simple bounded buffer.
    OTV0P2BASE::SystemStatsLine<
        decltype(Basics::valveMode), &Basics::valveMode,
        decltype(Basics::modelledRadValve), &Basics::modelledRadValve,
        decltype(Basics::tempC16), &Basics::tempC16,
        decltype(Basics::rh), &Basics::rh,
        decltype(Basics::ambLight), &Basics::ambLight,
        decltype(Basics::occupancy), &Basics::occupancy,
        decltype(Basics::schedule), &Basics::schedule,
        true, // Enable JSON stats.
        decltype(Basics::bp), &Basics::bp> ssl1;

    // Buffer should remain empty before any explicit activity.
    ASSERT_EQ(0, Basics::bp.getSize());
    ASSERT_EQ('\0', Basics::buf[0]);

    // Generate a stats line.
    ssl1.serialStatusReport();

    // Buffer should contain status line starting with '='.
    ASSERT_LE(1, Basics::bp.getSize());
    ASSERT_EQ(OTV0P2BASE::SERLINE_START_CHAR_STATS, Basics::buf[0]);

    // Check entire status line including trailing line termination.
    EXPECT_STREQ("=F0%@18CE;{\"@\":\"\",\"H|%\":50,\"L\":0,\"occ|%\":0}\r\n", Basics::buf);


    // Clear the buffer.
    Basics::bp.reset();

    // Create stats line instance wrapped round simple bounded buffer.
    // In this case omit all the 'optional' values.
    OTV0P2BASE::SystemStatsLine<
        decltype(Basics::valveMode), &Basics::valveMode,
        OTRadValve::AbstractRadValve, (OTRadValve::AbstractRadValve *)NULL,
        OTV0P2BASE::TemperatureC16Base, (OTV0P2BASE::TemperatureC16Base *)NULL,
        OTV0P2BASE::HumiditySensorBase, (OTV0P2BASE::HumiditySensorBase *)NULL,
        OTV0P2BASE::SensorAmbientLightBase, (OTV0P2BASE::SensorAmbientLightBase *)NULL,
        OTV0P2BASE::PseudoSensorOccupancyTracker, (OTV0P2BASE::PseudoSensorOccupancyTracker *)NULL,
        OTRadValve::SimpleValveScheduleBase, (OTRadValve::SimpleValveScheduleBase *)NULL,
        false, // No JSON stats.
        decltype(Basics::bp), &Basics::bp> sslO;
    // Generate a stats line.
    sslO.serialStatusReport();
    // Check entire status line including trailing line termination.
    EXPECT_STREQ("=F\r\n", Basics::buf);

    // Clear the buffer.
    Basics::bp.reset();

// FIXME FIXME FIXME
//    // Ensure that failing to provide a non-NULL printer does not cause a crash.
//    OTV0P2BASE::SystemStatsLine<
//        decltype(Basics::valveMode), &Basics::valveMode,
//        OTRadValve::AbstractRadValve, (OTRadValve::AbstractRadValve *)NULL,
//        OTV0P2BASE::TemperatureC16Base, (OTV0P2BASE::TemperatureC16Base *)NULL,
//        OTV0P2BASE::HumiditySensorBase, (OTV0P2BASE::HumiditySensorBase *)NULL,
//        OTV0P2BASE::SensorAmbientLightBase, (OTV0P2BASE::SensorAmbientLightBase *)NULL,
//        OTV0P2BASE::PseudoSensorOccupancyTracker, (OTV0P2BASE::PseudoSensorOccupancyTracker *)NULL,
//        OTRadValve::SimpleValveScheduleBase, (OTRadValve::SimpleValveScheduleBase *)NULL,
//        false, // No JSON stats.
//        Print, (Print *)NULL> sslBad;
//    // Generate a stats line.
//    sslBad.serialStatusReport();
//    // Buffer should remain empty before any explicit activity.
//    ASSERT_EQ(0, Basics::bp.getSize());
//    ASSERT_EQ('\0', Basics::buf[0]);
}

