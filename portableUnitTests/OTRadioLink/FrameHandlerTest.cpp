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

Author(s) / Copyright (s): Deniz Erbilgin 2017
*/

#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>

#include <OTRadioLink.h>

namespace OTFHT
{
    uint8_t minuteCount = 0;
    static constexpr uint8_t heatCallPin = 0; // unused in unit tests.
    OTRadioLink::OTNullRadioLink rt;
    OTRadValve::BoilerCallForHeat<heatCallPin> b0;
}
// // Basic sanity/does it compile tests
// TEST(FrameHandlerTest, OTSerialHandlerTest)
// {
//     // TODO write test (need print object)
// }
TEST(FrameHandlerTest, OTRadioHandlerTest)
{
    // Instantiate objects
    OTRadioLink::OTRadioHandler<decltype(OTFHT::rt), OTFHT::rt> rh;
    // message
    const uint8_t msgBuf[5] = {0,1,2,3,4};
    const char decryptedBuf[] = "hello";
    auto bufPtr = (const uint8_t * const) &decryptedBuf[1];
    const auto result = rh.frameHandler(msgBuf, bufPtr, sizeof(decryptedBuf) - 1);
    EXPECT_FALSE(result);
}
TEST(FrameHandlerTest, OTBoilerHandlerTest)
{
    // Instantiate objects
    OTRadioLink::OTBoilerHandler<decltype(OTFHT::b0), OTFHT::b0, OTFHT::minuteCount> bh;
    // message
    const uint8_t msgBuf[5] = {0,1,2,3,4};
    const char decryptedBuf[] = "hello";
    auto bufPtr = (const uint8_t * const) &decryptedBuf[1];
    const auto result = bh.frameHandler(msgBuf, bufPtr, sizeof(decryptedBuf) - 1);
    EXPECT_TRUE(result);
}
// TEST(FrameHandlerTest, NullHandlerTest)
// {
//     const char msg[] = "hello";
//     EXPECT_FALSE(OTRadioLink::decodeAndHandleNullFrame((const uint8_t *)msg));
// }
