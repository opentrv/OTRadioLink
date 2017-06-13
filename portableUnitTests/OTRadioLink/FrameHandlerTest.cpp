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

/*
 * UNTESTED:
 * - (static) authAndDecodeOTSecureableFrame
 * - (static) decodeAndHandleOTSecurableFrame (single and dual)
 * - (static) decodeAndHandleRawRXedMessage (single and dual)
 */

#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>

#include <OTAESGCM.h>
#include <OTRadioLink.h>

namespace OTFHT
{
    uint8_t minuteCount = 0;
    static constexpr uint8_t heatCallPin = 0; // unused in unit tests.

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

    // Null pollIO
    // FIXME need true version?
    bool pollIO(bool) {return (false);}
    // return fake Key
    bool getFakeKey(uint8_t *key) { memset(key, 0xff, /*OTV0P2BASE::VOP2BASE_EE_LEN_16BYTE_PRIMARY_BUILDING_KEY*/ 16); return (true); }

    // Instantiate objects for templating
    NULLSerialStream ss;
    OTRadioLink::OTNullRadioLink rt;
    OTRadValve::BoilerCallForHeat<heatCallPin> b0;
    OTRadioLink::OTNullHandlerTrue<decltype(OTFHT::ss), OTFHT::ss> th;
    OTRadioLink::OTNullHandlerFalse<decltype(OTFHT::ss), OTFHT::ss> fh;
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

TEST(FrameHandlerTest, OTNullHandlerTrue)
{
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };
    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);

    EXPECT_TRUE(OTFHT::th.frameHandler(fd));
}
TEST(FrameHandlerTest, OTNullHandlerFalse)
{
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };
    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);

    EXPECT_FALSE(OTFHT::fh.frameHandler(fd));
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
    const uint8_t decrypted[] = { 0, 0x10, '{', 'b', 'c'};

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
    const uint8_t decrypted0[] = { 0, 0x1, '{', 'b', 'c', 'd'};
    memcpy(fd.decryptedBody, decrypted0, sizeof(decrypted0));
    fd.decryptedBodyLen = sizeof(decrypted0);
    EXPECT_FALSE(sh.frameHandler(fd));

    // Case (dbLen > 3)
    const uint8_t decrypted1[] = { 0, 0x10, '{', 'b', 'c', 'd'};
    memcpy(fd.decryptedBody, decrypted1, sizeof(decrypted1));
    fd.decryptedBodyLen = 3;
    EXPECT_FALSE(sh.frameHandler(fd));

    // Case ('{' == db[2])
    const uint8_t decrypted2[] = { 0, 0x10, 's', 'b', 'c', 'd'};
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
    const uint8_t decrypted[] = { 0, 0x10, '{', 'b', 'c'};

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
    const uint8_t decryptedValid[] = { 0, 0x10, '{', 'b', 'c'};
    OTRadioLink::OTFrameData_T fd0(nullptr);
    memcpy(fd0.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd0.decryptedBody, decryptedValid, sizeof(decryptedValid));
    fd0.decryptedBodyLen = sizeof(decryptedValid);
    EXPECT_FALSE(rh.frameHandler(fd0));

    // Other cases
    OTRadioLink::OTFrameData_T fd1(&msgBuf[1]);
    memcpy(fd1.senderNodeID, nodeID, sizeof(nodeID));

    // Case (0 != (db[1] & 0x10)
    const uint8_t decrypted0[] = { 0, 0x1, '{', 'b', 'c', 'd'};
    memcpy(fd1.decryptedBody, decrypted0, sizeof(decrypted0));
    fd1.decryptedBodyLen = sizeof(decrypted0);
    EXPECT_FALSE(rh.frameHandler(fd1));

    // Case (dbLen > 3)
    const uint8_t decrypted1[] = { 0, 0x10, '{', 'b', 'c', 'd'};
    memcpy(fd1.decryptedBody, decrypted1, sizeof(decrypted1));
    fd1.decryptedBodyLen = 3;
    EXPECT_FALSE(rh.frameHandler(fd1));

    // Case ('{' == db[2])
    const uint8_t decrypted2[] = { 0, 0x10, 's', 'b', 'c', 'd'};
    memcpy(fd1.decryptedBody, decrypted2, sizeof(decrypted2));
    fd1.decryptedBodyLen = sizeof(decrypted2);
    EXPECT_FALSE(rh.frameHandler(fd1));
}

// Minimum valid Frame
TEST(FrameHandlerTest, OTBoilerHandlerTestTrue)
{
    // Instantiate objects
    OTRadioLink::OTBoilerHandler<decltype(OTFHT::b0), OTFHT::b0, OTFHT::minuteCount> bh;
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t decrypted[] = { 0 , 0x10, '{', 'b', 'c'};

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    memcpy(fd.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd.decryptedBody, decrypted, sizeof(decrypted));
    fd.decryptedBodyLen = sizeof(decrypted);

    EXPECT_TRUE(bh.frameHandler(fd));
}

TEST(FrameHandlerTest, handleSecureFrameSingle)
{
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t decrypted[] = { 0 , 0x10, '{', 'b', 'c'};

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    memcpy(fd.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd.decryptedBody, decrypted, sizeof(decrypted));

    // 1. Frame longer than 2, handler returns true
    fd.decryptedBodyLen = 2;
    const bool test1 = OTRadioLink::handleOTSecureFrame<decltype(OTFHT::th),OTFHT::th,'O'>(fd);
    EXPECT_TRUE(test1);
    // 2. Frame < 2, handler returns true
    fd.decryptedBodyLen = 1;
    const bool test2 = OTRadioLink::handleOTSecureFrame<decltype(OTFHT::th),OTFHT::th,'O'>(fd);
    EXPECT_FALSE(test2);
    // 3. Frame longer than 2, handler returns false
    fd.decryptedBodyLen = sizeof(decrypted);
    const bool test3 = OTRadioLink::handleOTSecureFrame<decltype(OTFHT::fh),OTFHT::fh,'O'>(fd);
    EXPECT_FALSE(test3);
}

TEST(FrameHandlerTest, handleSecureFrameDual)
{
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t decrypted[] = { 0 , 0x10, '{', 'b', 'c'};

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    memcpy(fd.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd.decryptedBody, decrypted, sizeof(decrypted));

    // 1. Frame longer than 2, Both handler returns true
    fd.decryptedBodyLen = 2;
    const bool test1 = OTRadioLink::handleOTSecureFrame<decltype(OTFHT::th),OTFHT::th, 'O',
                                                        decltype(OTFHT::th),OTFHT::th,'O'>(fd);
    EXPECT_TRUE(test1);
    // 2. Frame < 2, Both handler returns true
    fd.decryptedBodyLen = 1;
    const bool test2 = OTRadioLink::handleOTSecureFrame<decltype(OTFHT::th),OTFHT::th, 'O',
                                                        decltype(OTFHT::th),OTFHT::th,'O'>(fd);
    EXPECT_FALSE(test2);
    // 3. Frame longer than 2, Both handlers returns false
    fd.decryptedBodyLen = sizeof(decrypted);
    const bool test3 = OTRadioLink::handleOTSecureFrame<decltype(OTFHT::fh),OTFHT::fh, 'O',
                                                        decltype(OTFHT::fh),OTFHT::fh,'O'>(fd);
    EXPECT_FALSE(test3);
    // 4. Frame longer than 2, first handler returns false
    fd.decryptedBodyLen = sizeof(decrypted);
    const bool test4 = OTRadioLink::handleOTSecureFrame<decltype(OTFHT::th),OTFHT::th, 'O',
                                                        decltype(OTFHT::fh),OTFHT::fh,'O'>(fd);
    EXPECT_FALSE(test4);
    // 5. Frame longer than 2, second handler returns false
    fd.decryptedBodyLen = sizeof(decrypted);
    const bool test5 = OTRadioLink::handleOTSecureFrame<decltype(OTFHT::fh),OTFHT::fh, 'O',
                                                        decltype(OTFHT::th),OTFHT::th,'O'>(fd);
    EXPECT_FALSE(test5);
}

TEST(FrameHandlerTest, authAndDecodeSecurableFrameBasic)
{
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t decrypted[] = { 0 , 0x10, '{', 'b', 'c'};

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    memcpy(fd.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd.decryptedBody, decrypted, sizeof(decrypted));
    fd.decryptedBodyLen = sizeof(decrypted);

    const bool test1 = OTRadioLink::authAndDecodeOTSecurableFrame<OTFHT::getFakeKey>(fd);
    EXPECT_FALSE(test1);
}

// Should always return false
TEST(FrameHandlerTest, OTMessageQueueHandlerNull)
{
    OTRadioLink::OTMessageQueueHandlerNull mh;
    OTRadioLink::OTNullRadioLink rl;
    EXPECT_FALSE(mh.handle(false, rl));
}

TEST(FrameHandlerTest, OTMessageQueueHandlerSingleBasic)
{
    OTRadioLink::OTMessageQueueHandlerSingle<decltype(OTFHT::fh),OTFHT::fh, 'O',
                                             OTFHT::pollIO, 4800,
                                             OTFHT::getFakeKey> mh;
    OTRadioLink::OTNullRadioLink rl;
    EXPECT_FALSE(mh.handle(false, rl));
}
TEST(FrameHandlerTest, OTMessageQueueHandlerDualBasic)
{
    OTRadioLink::OTMessageQueueHandlerDual<decltype(OTFHT::fh),OTFHT::fh, 'O',
                                           decltype(OTFHT::fh),OTFHT::fh, 'O',
                                           OTFHT::pollIO, 4800,
                                           OTFHT::getFakeKey> mh;
    OTRadioLink::OTNullRadioLink rl;
    EXPECT_FALSE(mh.handle(false, rl));
}
// More detailed Tests

