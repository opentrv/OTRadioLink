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
                           Deniz Erbilgin 2016-2018
*/

/*
 * OTRadValve ModelledRadValve thermal model tests.
 *
 * Aim is to, for example, model different
 * radiator efficacies, valve behaviours, boiler speeds, radio loss, etc;
 * to ensure that responsiveness, temperature regulation
 * and valve movement/noise/energy are OK.
 *
 * Model for all-in-one and split unit configurations.
 *
 * This can even be extended to DHW tests.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>

#include "OTRadValve_AbstractRadValve.h"
#include "OTRadValve_ModelledRadValve.h"

#include "ThermalPhysicsModels.h"
using namespace OTRadValve::PortableUnitTest;

TEST(ModelledRadValveThermalModel, roomCold)
{
    TMB::verbose = false;
    // TMB::splitUnit = false;
    // Room start temp
    TMB::InitConditions_t  initCond {
        16.0f, // room temp in C
        19.0f, // target temp in C
        0,     // Valve position in %
    };

    // Set up.
    TMB::ValveModel<> vm;
    TMB::ThermalModelBasic tm;
    TMB::RoomModelBasic rm(initCond, vm, tm);

    // Delay in radiator responding to change in valvePCOpen. Should possibly be asymmetric. todo move into room model.
    for(auto i = 0; i < 20000; ++i) { rm.tick(i); }

    TMB::TempBoundsC_t bounds = rm.getTempBounds();
    EXPECT_GT((initCond.targetTempC + 2.0f), bounds.max);
    EXPECT_LT((initCond.targetTempC - 2.0f), bounds.min);
}

TEST(ModelledRadValveThermalModel, roomColdBinary)
{
    TMB::verbose = false;
    // TMB::splitUnit = false;
    // Room start temp
    TMB::InitConditions_t  initCond {
        16.0f, // room temp in C
        19.0f, // target temp in C
        0,     // Valve position in %
    };

    // Set up.
    TMB::ValveModel<true> vm;
    TMB::ThermalModelBasic tm;
    TMB::RoomModelBasic rm(initCond, vm, tm);

    // Delay in radiator responding to change in valvePCOpen. Should possibly be asymmetric. todo move into room model.
    for(auto i = 0; i < 20000; ++i) { rm.tick(i); }

    TMB::TempBoundsC_t bounds = rm.getTempBounds();
    EXPECT_GT((initCond.targetTempC + 2.5f), bounds.max);
    EXPECT_LT((initCond.targetTempC - 2.5f), bounds.min);
}

TEST(ModelledRadValveThermalModel, roomHot)
{
    TMB::verbose = false;
    // TMB::splitUnit = false;
    // Room start temp
    TMB::InitConditions_t  initCond {
        24.0f, // room temp in C
        19.0f, // target temp in C
        0,     // Valve position in %
    };

    // Set up.
    TMB::ValveModel<> vm;
    TMB::ThermalModelBasic tm;
    TMB::RoomModelBasic rm(initCond, vm, tm);

    // Delay in radiator responding to change in valvePCOpen. Should possibly be asymmetric. todo move into room model.
    for(auto i = 0; i < 20000; ++i) { rm.tick(i); }

    TMB::TempBoundsC_t bounds = rm.getTempBounds();
    // Room does not cool fast enough
    // EXPECT_GT((initCond.targetTempC + 2.0f), bounds.max);
    EXPECT_LT((initCond.targetTempC - 2.0f), bounds.min);
}


/* TODO

Test for sticky / jammed / closed value calling for heat in stable temp room running boiler continually: TODO-1096

Test for sensible outcomes in difficult cases such as:
  * DHW (esp needing glacial as per Bo).
  * All-in-one (TRV1.x) on flow end of rad very slow to heat up
    so valve must let whole rad get warm to let room get warm
    (eg see 1g 2017/01/14 12:00 to 15:00 om tag 20170114-responsiveness).
  * All-in-one (TRV1.x) on rad with very poor air circulation.
  * Split unit (REV2+FHT8V style) with sensor close to and far from radiator.
  * All-in-one or split unit in draughty room.
  * All-in-one or split unit in room with door or window opened on cold day.
  * Behaviour in well-insulated (or otherwise) house
    with central timer only set for a few hours per day.
 */
