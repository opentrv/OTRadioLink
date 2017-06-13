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

    class NULLSerialStream final : public Stream
      {
      public:
        static bool verbose;
        void begin(unsigned long) { }
        void begin(unsigned long, uint8_t);
        void end();

        virtual size_t write(uint8_t c) override { if(verbose) { fprintf(stderr, "%c\n", (char) c); } return(0); }
        virtual size_t write(const uint8_t *buf, size_t size) override { size_t n = 0; while((size-- > 0) && (0 != write(*buf++))) { ++n; } return(n); }
        virtual int available() override { return(-1); }
        virtual int read() override { return(-1); }
        virtual int peek() override { return(-1); }
        virtual void flush() override { }
      };
    bool NULLSerialStream::verbose = false;
    // Instantiate objects
    NULLSerialStream ss;

}
// Basic sanity/does it compile tests
TEST(FrameHandlerTest, OTFrameDataTest)
{
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[6] = { 5,    0,1,2,3,4 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t decrypted[] = "hello";

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    EXPECT_EQ(sizeof(fd.senderNodeID), OTV0P2BASE::OpenTRV_Node_ID_Bytes);
    EXPECT_EQ(sizeof(fd.decryptedBody), OTRadioLink::ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE);
    memcpy(fd.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd.decryptedBody, decrypted, sizeof(decrypted));
    fd.decryptedBodyLen = sizeof(decrypted);

    EXPECT_EQ(5, fd.msg[-1]);
    EXPECT_EQ(sizeof(decrypted), fd.decryptedBodyLen);

}
// Minimum valid Frame
TEST(FrameHandlerTest, OTSerialHandlerTestTrue)
{
    OTFHT::NULLSerialStream::verbose = false;
    // Instantiate objects
    OTRadioLink::OTSerialHandler<decltype(OTFHT::ss), OTFHT::ss> sh;
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t decrypted[] = { 'a', 0x10, '{', 'b', 'c'};

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    memcpy(fd.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd.decryptedBody, decrypted, sizeof(decrypted));
    fd.decryptedBodyLen = sizeof(decrypted);

    EXPECT_TRUE(sh.frameHandler(fd));
}
// Invalid Frame
TEST(FrameHandlerTest, OTSerialHandlerTestFalse)
{
    OTFHT::NULLSerialStream::verbose = false;
    // Instantiate objects
    OTRadioLink::OTSerialHandler<decltype(OTFHT::ss), OTFHT::ss> sh;
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[6] = { 5,    0,1,3,4,5 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    memcpy(fd.senderNodeID, nodeID, sizeof(nodeID));

    // Case (0 != (db[1] & 0x10)
    const uint8_t decrypted0[] = { 'a', 0x1, '{', 'b', 'c', 'd'};
    memcpy(fd.decryptedBody, decrypted0, sizeof(decrypted0));
    fd.decryptedBodyLen = sizeof(decrypted0);
    EXPECT_FALSE(sh.frameHandler(fd));

    // Case (dbLen > 3)
    const uint8_t decrypted1[] = { 'a', 0x10, '{', 'b', 'c', 'd'};
    memcpy(fd.decryptedBody, decrypted1, sizeof(decrypted1));
    fd.decryptedBodyLen = 3;
    EXPECT_FALSE(sh.frameHandler(fd));

    // Case ('{' == db[2])
    const uint8_t decrypted2[] = { 'a', 0x10, 's', 'b', 'c', 'd'};
    memcpy(fd.decryptedBody, decrypted2, sizeof(decrypted2));
    fd.decryptedBodyLen = sizeof(decrypted2);
    EXPECT_FALSE(sh.frameHandler(fd));
}

// Minimum valid Frame
TEST(FrameHandlerTest, OTRadioHandlerTestTrue)
{
    // Instantiate objects
    OTRadioLink::OTRadioHandler<decltype(OTFHT::rt), OTFHT::rt> rh;
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t decrypted[] = { 'a', 0x10, '{', 'b', 'c'};

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    memcpy(fd.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd.decryptedBody, decrypted, sizeof(decrypted));
    fd.decryptedBodyLen = sizeof(decrypted);

    EXPECT_TRUE(rh.frameHandler(fd));
}

// Invalid Frame
TEST(FrameHandlerTest, OTRadioHandlerTestFalse)
{
    // Instantiate objects
    OTRadioLink::OTRadioHandler<decltype(OTFHT::rt), OTFHT::rt> rh;

    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[6] = { 5,    0,1,3,4,5 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};

    // Case nullptr
    const uint8_t decryptedValid[] = { 'a', 0x10, '{', 'b', 'c'};
    OTRadioLink::OTFrameData_T fd0(nullptr);
    memcpy(fd0.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd0.decryptedBody, decryptedValid, sizeof(decryptedValid));
    fd0.decryptedBodyLen = sizeof(decryptedValid);
    EXPECT_FALSE(rh.frameHandler(fd0));

    // Other cases
    OTRadioLink::OTFrameData_T fd1(&msgBuf[1]);
    memcpy(fd1.senderNodeID, nodeID, sizeof(nodeID));

    // Case (0 != (db[1] & 0x10)
    const uint8_t decrypted0[] = { 'a', 0x1, '{', 'b', 'c', 'd'};
    memcpy(fd1.decryptedBody, decrypted0, sizeof(decrypted0));
    fd1.decryptedBodyLen = sizeof(decrypted0);
    EXPECT_FALSE(rh.frameHandler(fd1));

    // Case (dbLen > 3)
    const uint8_t decrypted1[] = { 'a', 0x10, '{', 'b', 'c', 'd'};
    memcpy(fd1.decryptedBody, decrypted1, sizeof(decrypted1));
    fd1.decryptedBodyLen = 3;
    EXPECT_FALSE(rh.frameHandler(fd1));

    // Case ('{' == db[2])
    const uint8_t decrypted2[] = { 'a', 0x10, 's', 'b', 'c', 'd'};
    memcpy(fd1.decryptedBody, decrypted2, sizeof(decrypted2));
    fd1.decryptedBodyLen = sizeof(decrypted2);
    EXPECT_FALSE(rh.frameHandler(fd1));
}

TEST(FrameHandlerTest, OTBoilerHandlerTest)
{
    // Instantiate objects
    OTRadioLink::OTBoilerHandler<decltype(OTFHT::b0), OTFHT::b0, OTFHT::minuteCount> bh;
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[6] = { 5,    0,1,2,3,4 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t decrypted[] = "hello";

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    memcpy(fd.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd.decryptedBody, decrypted, sizeof(decrypted));
    fd.decryptedBodyLen = sizeof(decrypted);

    const auto result = bh.frameHandler(fd);
    EXPECT_TRUE(result);
}



// More detailed Tests

