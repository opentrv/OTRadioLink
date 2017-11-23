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
 * - (static) decodeAndHandleRawRXedMessage (single and dual)
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>
#include "OTV0P2BASE_Util.h"
// Only enable these tests if the OTAESGCM library is marked as available.
#if defined(EXT_AVAILABLE_ARDUINO_LIB_OTAESGCM)
#include <OTAESGCM.h>
#endif  // defined(EXT_AVAILABLE_ARDUINO_LIB_OTAESGCM)
#include <OTRadioLink.h>


#ifndef __APPLE__
//static constexpr unsigned int maxStackSecureFrameEncode = 328;
static constexpr unsigned int maxAuthAndDecodeStack = 216; // was 200. clang uses more stack.
#else
// On DHD's system, secure frame enc/decode uses 358 bytes (20170511)
// static constexpr unsigned int maxStackSecureFrameEncode = 1024;
static constexpr unsigned int maxAuthAndDecodeStack = 216;
#endif // __APPLE__

// IF DEFINED: Enable non-workspace versions of AES128GCM.
// These are disabled by default as:
// - They make large (> 200 byte on AVR) stack allocations and are not
//   recommended.
// - When included they prevent -Werror and -Wstack-usage being used together
//   for static analysis of stack allocations.
#undef OTAESGCM_ALLOW_NON_WORKSPACE


TEST(FrameHandler, StackCheckerWorks)
{
    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();
    EXPECT_NE((size_t)0, baseStack);
}

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

    // Mock decryption function
    // Set true to pass decryption, false to fail.
    // Only enable these if the OTAESGCM library is marked as available.
    static bool mockDecryptSuccess = false;
    using mockDecrypt_fn_t = OTRadioLink::SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_fn_t;
    mockDecrypt_fn_t mockDecrypt;
    bool mockDecrypt (void *const,
                      const uint8_t *const /*key*/, const uint8_t *const /*iv*/,
                      const uint8_t *const /*authtext*/, const uint8_t /*authtextSize*/,
                      const uint8_t *const /*ciphertext*/, const uint8_t *const /*tag*/,
                      uint8_t *const /*plaintextOut*/)
    {
        return (mockDecryptSuccess);
    }

    // return fake Key
    bool getKeySuccess(uint8_t *key) { memset(key, 0x0,  /*OTV0P2BASE::VOP2BASE_EE_LEN_16BYTE_PRIMARY_BUILDING_KEY*/ 16); return (true); }
    bool getKeyFail(uint8_t *) { return (false); }

    // Instantiate objects for templating
    NULLSerialStream ss;
    OTRadioLink::OTNullRadioLink rt;
    OTRadValve::OTHubManager<false, false> hm;  // no EEPROM so parameters don't matter
    OTRadValve::BoilerLogic::OnOffBoilerDriverLogic<decltype(hm), hm, heatCallPin> b0;

    // like Nullframe operation but sets a flag
    volatile bool frameOperationCalledFlag = false;
    OTRadioLink::frameOperator_fn_t setFlagFrameOperation;
    bool setFlagFrameOperation(const OTRadioLink::OTFrameData_T &) { frameOperationCalledFlag = true; return (true);}

    struct minimumSecureFrame
    {
        // Preshared ID prefix; only an initial part/prefix of this goes on the wire in the header.
        static const uint8_t id[];
        // IV/nonce starting with first 6 bytes of preshared ID, then 6 bytes of counter.
        static const uint8_t iv[];
        static const uint8_t oldCounter[];
        // 'O' frame body with some JSON stats.
        static const uint8_t body[8];
        // length of secure frame
        static const uint8_t encodedLength;
        // Buffer containing secure frame. Generated using code bellow.
        static const uint8_t buf[];

     // Stuff used to generate a working encodable frame. Taken from SecureFrameTest.cpp
     //    All-zeros const 16-byte/128-bit key.
     //    Can be used for other purposes.
//        static const uint8_t zeroBlock[16];
//        static uint8_t buf[OTRadioLink::SecurableFrameHeader::maxSmallFrameSize];
//        //Example 3: secure, no valve, representative minimum stats {"b":1}).
//        //Note that the sequence number must match the 4 lsbs of the message count, ie from iv[11].
//        //and the ID is 0xaa 0xaa 0xaa 0xaa (transmitted) with the next ID bytes 0x55 0x55.
//        //ResetCounter = 42
//        //TxMsgCounter = 793
//        //(Thus nonce/IV: aa aa aa aa 55 55 00 00 2a 00 03 19)
//        //
//        //3e cf 94 aa aa aa aa 20 | b3 45 f9 29 69 57 0c b8 28 66 14 b4 f0 69 b0 08 71 da d8 fe 47 c1 c3 53 83 48 88 03 7d 58 75 75 | 00 00 2a 00 03 19 29 3b 31 52 c3 26 d2 6d d0 8d 70 1e 4b 68 0d cb 80
//        //
//        //3e  length of header (62) after length byte 5 + (encrypted) body 32 + trailer 32
//        //cf  'O' secure OpenTRV basic frame
//        //04  0 sequence number, ID length 4
//        //aa  ID byte 1
//        //aa  ID byte 2
//        //aa  ID byte 3
//        //aa  ID byte 4
//        //20  body length 32 (after padding and encryption)
//        //    Plaintext body (length 8): 0x7f 0x11 { " b " : 1
//        //    Padded: 7f 11 7b 22 62 22 3a 31 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 17
//        //b3 45 f9 ... 58 75 75  32 bytes of encrypted body
//        //00 00 2a  reset counter
//        //00 03 19  message counter
//        //29 3b 31 ... 68 0d cb  16 bytes of authentication tag
//        //80  enc/auth type/format indicator.
//        static void generateMockFrame() {
//            // def workspace
//            uint8_t bodyBuf[32] = {};
//            memcpy(bodyBuf, body, sizeof(body));
//            uint8_t workspace[OTRadioLink::SimpleSecureFrame32or0BodyTXBase::encodeSecureSmallFrameRawPadInPlace_total_scratch_usage_OTAESGCM_2p0];
//            OTV0P2BASE::ScratchSpaceL sW(workspace, sizeof(workspace));
//            // generate frame
//            const uint8_t encodedLength = OTRadioLink::SimpleSecureFrame32or0BodyTXBase::encodeSecureSmallFrameRawPadInPlace(
//                                            buf, sizeof(buf),
//                                            OTRadioLink::FTS_BasicSensorOrValve,
//                                            id, 4,
//                                            bodyBuf, sizeof(body),
//                                            iv,
//                                            OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_WITH_LWORKSPACE,
//                                            sW, zeroBlock);
//            // print original message
//            std::fprintf(stdout, "const uint8_t encodedLength = %u;\n", encodedLength);
//            std::cout << "const uint8_t body[] = {\n\t";
//            for(size_t i = 0; i < sizeof(body); ++i) {
//                std::fprintf(stdout, "0x%x, ", body[i]);
//                if(7 == (i % 8)) std::cout << "\n\t";
//            }
//            std::cout << " };\n";
//            // print encrypted message
//            std::cout << "const uint8_t buf[] = {\n\t";
//            for(auto i = 0; i < OTRadioLink::SecurableFrameHeader::maxSmallFrameSize; ++i) {
//                std::fprintf(stdout, "0x%x, ", buf[i]);
//                if(7 == (i % 8)) std::cout << "\n\t";
//            }
//            std::cout << " };\n";
//        }
    };
    // Preshared ID prefix; only an initial part/prefix of this goes on the wire in the header.
    const uint8_t minimumSecureFrame::id[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55 };
    // IV/nonce starting with first 6 bytes of preshared ID, then 6 bytes of counter.
    const uint8_t minimumSecureFrame::iv[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x00, 0x00, 0x2a, 0x00, 0x03, 0x19 };
    const uint8_t minimumSecureFrame::oldCounter[] = { 0x00, 0x00, 0x2a, 0x00, 0x03, 0x18 };
    // 'O' frame body with some JSON stats.
    const uint8_t minimumSecureFrame::body[8] = { 0x64, 0x11, 0x7b, 0x22, 0x62, 0x22, 0x3a, 0x31 };  // first byte signals valvePC of 100%.
    // length of secure frame
    const uint8_t minimumSecureFrame::encodedLength = 63;
    // Buffer containing secure frame. Generated using code above.
    const uint8_t minimumSecureFrame::buf[] = {
        0x3e, 0xcf, 0x94, 0xaa, 0xaa, 0xaa, 0xaa, 0x20,
        0xa8, 0x45, 0xf9, 0x29, 0x69, 0x57, 0xc, 0xb8,
        0x28, 0x66, 0x14, 0xb4, 0xf0, 0x69, 0xb0, 0x8,
        0x71, 0xda, 0xd8, 0xfe, 0x47, 0xc1, 0xc3, 0x53,
        0x83, 0x48, 0x88, 0x3, 0x7d, 0x58, 0x75, 0x75,
        0x0, 0x0, 0x2a, 0x0, 0x3, 0x19, 0x51, 0x23,
        0x7e, 0x33, 0xfe, 0x48, 0x8d, 0x1a, 0x81, 0x21,
        0x25, 0xf8, 0x1f, 0x14, 0x6b, 0x8a, 0x80 };
    // Old version with body[0] = 0x7f
//////    uint8_t minimumSecureFrame::buf[] = {};
//    const uint8_t minimumSecureFrame::buf[] = {
//        0x3e, 0xcf, 0x94, 0xaa, 0xaa, 0xaa, 0xaa, 0x20,
//        0xb3, 0x45, 0xf9, 0x29, 0x69, 0x57, 0x0c, 0xb8,
//        0x28, 0x66, 0x14, 0xb4, 0xf0, 0x69, 0xb0, 0x08,
//        0x71, 0xda, 0xd8, 0xfe, 0x47, 0xc1, 0xc3, 0x53,
//        0x83, 0x48, 0x88, 0x03, 0x7d, 0x58, 0x75, 0x75,
//        0x00, 0x00, 0x2a, 0x00, 0x03, 0x19, 0x29, 0x3b,
//        0x31, 0x52, 0xc3, 0x26, 0xd2, 0x6d, 0xd0, 0x8d,
//        0x70, 0x1e, 0x4b, 0x68, 0x0d, 0xcb, 0x80 };

}
// Basic sanity/does it compile tests
TEST(FrameHandler, OTFrameData)
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

TEST(FrameHandler, NullFrameOperationFalse)
{
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };
    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);

    EXPECT_FALSE(OTRadioLink::nullFrameOperation(fd));
}
// Minimum valid Frame
TEST(FrameHandler, SerialFrameOperationSuccess)
{
    OTFHT::NULLSerialStream::verbose = false;
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t decrypted[] = { 0, 0x10, '{', 'b', 'c'};

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    memcpy(fd.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd.decryptedBody, decrypted, sizeof(decrypted));
    fd.decryptedBodyLen = sizeof(decrypted);
    const bool serialOperationSuccess = OTRadioLink::serialFrameOperation<decltype(OTFHT::ss),OTFHT::ss>(fd);
    EXPECT_TRUE(serialOperationSuccess);
}
// Invalid Frame
TEST(FrameHandlerTest, SerialFrameOperationFail)
{
    OTFHT::NULLSerialStream::verbose = false;
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
    const bool serialOperationFailHighBit = OTRadioLink::serialFrameOperation<decltype(OTFHT::ss),OTFHT::ss>(fd);
    EXPECT_FALSE(serialOperationFailHighBit);

    // Case (dbLen > 3)
    const uint8_t decrypted1[] = { 0, 0x10, '{', 'b', 'c', 'd'};
    memcpy(fd.decryptedBody, decrypted1, sizeof(decrypted1));
    fd.decryptedBodyLen = 3;
    const bool serialOperationFailLength= OTRadioLink::serialFrameOperation<decltype(OTFHT::ss),OTFHT::ss>(fd);
    EXPECT_FALSE(serialOperationFailLength);

    // Case ('{' == db[2])
    const uint8_t decrypted2[] = { 0, 0x10, 's', 'b', 'c', 'd'};
    memcpy(fd.decryptedBody, decrypted2, sizeof(decrypted2));
    fd.decryptedBodyLen = sizeof(decrypted2);
    const bool serialOperationFailBrace= OTRadioLink::serialFrameOperation<decltype(OTFHT::ss),OTFHT::ss>(fd);
    EXPECT_FALSE(serialOperationFailBrace);
}

// Minimum valid Frame
TEST(FrameHandler, RelayFrameOperationSuccess)
{
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };
    const uint8_t nodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t decrypted[] = { 0, 0x10, '{', 'b', 'c'};

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    memcpy(fd.senderNodeID, nodeID, sizeof(nodeID));
    memcpy(fd.decryptedBody, decrypted, sizeof(decrypted));
    fd.decryptedBodyLen = sizeof(decrypted);
    const bool relayOperationSuccess = OTRadioLink::relayFrameOperation<decltype(OTFHT::rt), OTFHT::rt>(fd);
    EXPECT_TRUE(relayOperationSuccess);
}

// Invalid Frame
TEST(FrameHandler, RelayFrameOperationFail)
{
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
    const bool relayOperationFailNullMsgPtr = OTRadioLink::relayFrameOperation<decltype(OTFHT::rt), OTFHT::rt>(fd0);
    EXPECT_FALSE(relayOperationFailNullMsgPtr);

    // Other cases
    OTRadioLink::OTFrameData_T fd1(&msgBuf[1]);
    memcpy(fd1.senderNodeID, nodeID, sizeof(nodeID));

    // Case (0 != (db[1] & 0x10)
    const uint8_t decrypted0[] = { 0, 0x1, '{', 'b', 'c', 'd'};
    memcpy(fd1.decryptedBody, decrypted0, sizeof(decrypted0));
    fd1.decryptedBodyLen = sizeof(decrypted0);
    const bool relayOperationFailHighBit = OTRadioLink::relayFrameOperation<decltype(OTFHT::rt), OTFHT::rt>(fd1);
    EXPECT_FALSE(relayOperationFailHighBit);

    // Case (dbLen > 3)
    const uint8_t decrypted1[] = { 0, 0x10, '{', 'b', 'c', 'd'};
    memcpy(fd1.decryptedBody, decrypted1, sizeof(decrypted1));
    fd1.decryptedBodyLen = 3;
    const bool relayOperationFailLength = OTRadioLink::relayFrameOperation<decltype(OTFHT::rt), OTFHT::rt>(fd1);
    EXPECT_FALSE(relayOperationFailLength);

    // Case ('{' == db[2])
    const uint8_t decrypted2[] = { 0, 0x10, 's', 'b', 'c', 'd'};
    memcpy(fd1.decryptedBody, decrypted2, sizeof(decrypted2));
    fd1.decryptedBodyLen = sizeof(decrypted2);
    const bool relayOperationFailBrace = OTRadioLink::relayFrameOperation<decltype(OTFHT::rt), OTFHT::rt>(fd1);
    EXPECT_FALSE(relayOperationFailBrace);
}

// Minimum valid Frame
TEST(FrameHandler, BoilerFrameOperationSuccess)
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
    const bool boilerOperationSuccess = OTRadioLink::boilerFrameOperation<decltype(OTFHT::b0), OTFHT::b0, OTFHT::minuteCount>(fd);
    EXPECT_TRUE(boilerOperationSuccess);
}

TEST(FrameHandler, authAndDecodeSecurableFrameBasic)
{
    // fd.decryptedBody set after getKey is called. Set to 0 by default and not changed on failing
    // secure frame decode. Therefore, on key success and frame decode fail, should be set to 0.
    constexpr uint8_t expectedDecryptedBodyLen = 0;
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    fd.decryptedBodyLen = 0xff;  // Test that this is really set.

    const bool test1 = OTRadioLink::authAndDecodeOTSecurableFrame<OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                                                                  OTFHT::mockDecrypt,
                                                                  OTFHT::getKeySuccess>(fd);
    EXPECT_FALSE(test1);
    EXPECT_EQ(expectedDecryptedBodyLen, fd.decryptedBodyLen);
}

TEST(FrameHandler, authAndDecodeSecurableFrameGetKeyFalse)
{
    // fd.decryptedBody only set after getKey succeeds. Therefore, on key fail, should be unchanged.
    constexpr uint8_t expectedDecryptedBodyLen = 0xff;
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    0,1,2,3,4 };

    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    fd.decryptedBodyLen = 0xff;  // Test that this is really set.

    //
    const bool test1 = OTRadioLink::authAndDecodeOTSecurableFrame<OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                                                                  OTFHT::mockDecrypt,
                                                                  OTFHT::getKeyFail>(fd);
    EXPECT_FALSE(test1);
    EXPECT_EQ(expectedDecryptedBodyLen, fd.decryptedBodyLen);
}

// Basic test with an invalid message.
TEST(FrameHandlerTest, decodeAndHandleOTSecurableFrameBasic)
{
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    'O',1,2,3,4 };
    const uint8_t * const msgStart = &msgBuf[1];

    const bool test1 = OTRadioLink::decodeAndHandleOTSecureOFrame<OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                                                                  OTFHT::mockDecrypt,
                                                                  OTFHT::getKeySuccess,
                                                                  OTRadioLink::nullFrameOperation
                                                                 >(msgStart);
    EXPECT_FALSE(test1);
}

//
TEST(FrameHandlerTest, decodeAndHandleOTSecurableFrameNoAuthSuccess)
{

    // Secure Frame start
    const uint8_t * const msgStart = &OTFHT::minimumSecureFrame::buf[1];

    //
    const bool test1 = OTRadioLink::decodeAndHandleOTSecureOFrame<OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                                                                  OTFHT::mockDecrypt,
                                                                  OTFHT::getKeySuccess,
                                                                  OTRadioLink::nullFrameOperation
                                                                 >(msgStart);
    EXPECT_TRUE(test1);
}

// Measure stack usage of authAndDecodeOTSecurableFrame
// (DE20170616): 80 (decodeSecureSmallFrameSafely code path disabled)
TEST(FrameHandler, authAndDecodeOTSecurableFrameStackCheck)
{
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    'O',1,2,3,4 };
    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();
    OTRadioLink::authAndDecodeOTSecurableFrame<OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                                               OTFHT::mockDecrypt,
                                               OTFHT::getKeySuccess>(fd);
    const size_t maxStack = OTV0P2BASE::MemoryChecks::getMinSP();
    // Uncomment to print stack usage
//    std::cout << baseStack - maxStack << "\n";
    EXPECT_GT((intptr_t)maxAuthAndDecodeStack, (intptr_t)(baseStack - maxStack));
}



// Measure stack usage of decodeAndHandleOTSecureFrame
// (DE20170616): 128
TEST(FrameHandler, decodeAndHandleOTSecureOFrameStackCheck)
{
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    'O',1,2,3,4 };
    const uint8_t * const msgStart = &msgBuf[1];
    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();
    OTRadioLink::decodeAndHandleOTSecureOFrame<OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                                               OTFHT::mockDecrypt,
                                               OTFHT::getKeySuccess,
                                               OTRadioLink::nullFrameOperation
                                              >(msgStart);
    const size_t maxStack = OTV0P2BASE::MemoryChecks::getMinSP();
    // Uncomment to print stack usage
//    std::cout << baseStack - maxStack << "\n";
    EXPECT_GT((intptr_t)200, (intptr_t)(baseStack - maxStack));
}

// Should always return false
TEST(FrameHandler, OTMessageQueueHandlerNull)
{
    OTRadioLink::OTMessageQueueHandlerNull mh;
    EXPECT_FALSE(mh.handle(false, OTFHT::rt));
}

TEST(FrameHandler, OTMessageQueueHandlerBasic)
{
    OTRadioLink::OTMessageQueueHandler<
        OTFHT::pollIO, 4800,
        OTRadioLink::decodeAndHandleDummyFrame> mh;
    OTRadioLink::OTNullRadioLink rl;
    EXPECT_FALSE(mh.handle(false, rl));
}

//// Only enable these tests if the OTAESGCM library is marked as available.
#if defined(EXT_AVAILABLE_ARDUINO_LIB_OTAESGCM)
TEST(FrameHandlerTest, setFlagFrameOperation)
{
    // Make sure flag is false.
    OTFHT::frameOperationCalledFlag = false;
    // message
    // msg buf consists of    { len | Message   }
    const uint8_t msgBuf[] = { 5,    'O',1,2,3,4 };
    OTRadioLink::OTFrameData_T fd(&msgBuf[1]);
    OTFHT::setFlagFrameOperation(fd);
    EXPECT_TRUE(OTFHT::frameOperationCalledFlag);

}

TEST(FrameHandlerTest, authAndDecodeSecurableFrameFull)
{
    // Secure Frame start
    const uint8_t * senderID = OTFHT::minimumSecureFrame::id;
    const uint8_t * msgCounter = OTFHT::minimumSecureFrame::oldCounter;
    const uint8_t * const msgStart = &OTFHT::minimumSecureFrame::buf[1];

    OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter &sfrx = OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter::getInstance();
    sfrx.setMockIDValue(senderID);
    sfrx.setMockCounterValue(msgCounter);

    OTRadioLink::OTFrameData_T fd(msgStart);
    EXPECT_NE(0, fd.sfh.checkAndDecodeSmallFrameHeader(OTFHT::minimumSecureFrame::buf, OTFHT::minimumSecureFrame::encodedLength));

    // Workspace for authAndDecodeOTSecurableFrameWithWorkspace
    constexpr size_t workspaceRequired =
            OTRadioLink::SimpleSecureFrame32or0BodyRXBase::decodeSecureSmallFrameSafely_total_scratch_usage_OTAESGCM_3p0
            + OTAESGCM::OTAES128GCMGenericWithWorkspace<>::workspaceRequiredDec
            + OTRadioLink::authAndDecodeOTSecurableFrameWithWorkspace_scratch_usage; // + space to hold the key
    uint8_t workspace[workspaceRequired];
    OTV0P2BASE::ScratchSpaceL sW(workspace, sizeof(workspace));

    // Set up stack usage checks
    const bool test1 = OTRadioLink::authAndDecodeOTSecurableFrameWithWorkspace<
                OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_WITH_LWORKSPACE,
                OTFHT::getKeySuccess
            >(fd, sW);
    EXPECT_TRUE(test1);
    EXPECT_EQ(0, strncmp((const char *) fd.decryptedBody, (const char *) OTFHT::minimumSecureFrame::body, sizeof(OTFHT::minimumSecureFrame::body)));
}

TEST(FrameHandlerTest, decodeAndHandleOTSecurableFrameDecryptSuccess)
{
    OTFHT::NULLSerialStream::verbose = false;

    // Make sure flag is false.
    OTFHT::frameOperationCalledFlag = false;
    // Secure Frame start
    const uint8_t * senderID = OTFHT::minimumSecureFrame::id;
    const uint8_t * msgCounter = OTFHT::minimumSecureFrame::oldCounter;
    const uint8_t * const msgStart = &OTFHT::minimumSecureFrame::buf[1];

    OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter &sfrx = OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter::getInstance();
    sfrx.setMockIDValue(senderID);
    sfrx.setMockCounterValue(msgCounter);

    // Workspace for decodeAndHandleOTSecureOFrameWithWorkspace
    constexpr size_t workspaceRequired =
            OTRadioLink::SimpleSecureFrame32or0BodyRXBase::decodeSecureSmallFrameSafely_total_scratch_usage_OTAESGCM_3p0
            + OTAESGCM::OTAES128GCMGenericWithWorkspace<>::workspaceRequiredDec
            + OTRadioLink::authAndDecodeOTSecurableFrameWithWorkspace_scratch_usage; // + space to hold the key
    uint8_t workspace[workspaceRequired];
    OTV0P2BASE::ScratchSpaceL sW(workspace, sizeof(workspace));

    const bool test1 = OTRadioLink::decodeAndHandleOTSecureOFrameWithWorkspace<
            OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
            OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_WITH_LWORKSPACE,
            OTFHT::getKeySuccess,
            OTFHT::setFlagFrameOperation,
            OTRadioLink::serialFrameOperation<decltype(OTFHT::ss),OTFHT::ss>
            >(msgStart, sW);
    EXPECT_TRUE(test1);
    EXPECT_TRUE(OTFHT::frameOperationCalledFlag);
}


namespace FTBHT {
constexpr uint8_t heatCallPin = 0;
constexpr bool inHubMode = true;
// This value is a const, but marking as such causes template instantiation to
// fail on Travis CI when compiling with gcc (Ubuntu 4.8.4-2ubuntu1~14.04.3) 4.8.4 
/*const*/ uint8_t minuteCount = 1;
OTRadValve::OTHubManager<false, false> hm;  // no EEPROM so parameters don't matter
OTRadValve::BoilerLogic::OnOffBoilerDriverLogic<decltype(hm), hm, heatCallPin> b1;
//
bool decodeAndHandleSecureFrame(volatile const uint8_t *const msg)
{
    // Workspace for decodeAndHandleOTSecureOFrameWithWorkspace
    constexpr size_t workspaceRequired =
            OTRadioLink::SimpleSecureFrame32or0BodyRXBase::decodeSecureSmallFrameSafely_total_scratch_usage_OTAESGCM_3p0
            + OTAESGCM::OTAES128GCMGenericWithWorkspace<>::workspaceRequiredDec
            + OTRadioLink::authAndDecodeOTSecurableFrameWithWorkspace_scratch_usage; // + space to hold the key
    uint8_t workspace[workspaceRequired];
    OTV0P2BASE::ScratchSpaceL sW(workspace, sizeof(workspace));

    return (OTRadioLink::decodeAndHandleOTSecureOFrameWithWorkspace<
                            OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                            OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_WITH_LWORKSPACE,
                            OTFHT::getKeySuccess,
                            OTRadioLink::serialFrameOperation<decltype(OTFHT::ss),OTFHT::ss>,
                            OTRadioLink::boilerFrameOperation<decltype(b1), b1, minuteCount>
                            >(msg, sW));
}
}
// Test message handler to boiler hub stack
TEST(FrameHandlerTest, frameToBoilerHubTest)
{
    OTFHT::NULLSerialStream::verbose = false;

    // Reset boiler driver state
    FTBHT::b1.reset();

    const uint8_t * senderID = OTFHT::minimumSecureFrame::id;
    const uint8_t * msgCounter = OTFHT::minimumSecureFrame::oldCounter;

    OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter &sfrx = OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter::getInstance();
    sfrx.setMockIDValue(senderID);
    sfrx.setMockCounterValue(msgCounter);

    // Setup message handler and mock radio
    OTRadioLink::OTMessageQueueHandler<
        OTFHT::pollIO, 4800,
        FTBHT::decodeAndHandleSecureFrame> mh;
    OTRadioLink::OTRadioLinkMock rl;
    // Populate radio message buffer
    memcpy(rl.message, OTFHT::minimumSecureFrame::buf, sizeof(OTFHT::minimumSecureFrame::buf));

    // Trick boiler hub into believing 10 minutes have passed.
    for(auto i = 0; i < 100; ++i) {
        FTBHT::b1.processCallsForHeat(true, FTBHT::inHubMode);
    }
    EXPECT_FALSE(FTBHT::b1.isBoilerOn());  // Should initialise to off

    // "Handle" to trigger bh remote call for heat.
    const bool test1 = mh.handle(false, rl);
    EXPECT_TRUE(test1);
    EXPECT_FALSE(FTBHT::b1.isBoilerOn());  // Should still be off, until heat call processed.
    FTBHT::b1.processCallsForHeat(false, FTBHT::inHubMode);
    EXPECT_TRUE(FTBHT::b1.isBoilerOn());
}

#ifdef OTAESGCM_ALLOW_NON_WORKSPACE
//
TEST(FrameHandlerTest, authAndDecodeSecurableFrameFull)
{
    // Secure Frame start
    const uint8_t * senderID = OTFHT::minimumSecureFrame::id;
    const uint8_t * msgCounter = OTFHT::minimumSecureFrame::oldCounter;
    const uint8_t * const msgStart = &OTFHT::minimumSecureFrame::buf[1];

    OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter &sfrx = OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter::getInstance();
    sfrx.setMockIDValue(senderID);
    sfrx.setMockCounterValue(msgCounter);

    OTRadioLink::OTFrameData_T fd(msgStart);
    EXPECT_NE(0, fd.sfh.checkAndDecodeSmallFrameHeader(OTFHT::minimumSecureFrame::buf, OTFHT::minimumSecureFrame::encodedLength));

    // Set up stack usage checks
    const bool test1 = OTRadioLink::authAndDecodeOTSecurableFrame<
                OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                OTFHT::getKeySuccess
            >(fd);
    EXPECT_TRUE(test1);
    EXPECT_EQ(0, strncmp((const char *) fd.decryptedBody, (const char *) OTFHT::minimumSecureFrame::body, sizeof(OTFHT::minimumSecureFrame::body)));
}


TEST(FrameHandlerTest, decodeAndHandleOTSecurableFrameDecryptSuccess)
{
    // Make sure flag is false.
    OTFHT::frameOperationCalledFlag = false;
    // Secure Frame start
    const uint8_t * senderID = OTFHT::minimumSecureFrame::id;
    const uint8_t * msgCounter = OTFHT::minimumSecureFrame::oldCounter;
    const uint8_t * const msgStart = &OTFHT::minimumSecureFrame::buf[1];

    OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter &sfrx = OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter::getInstance();
    sfrx.setMockIDValue(senderID);
    sfrx.setMockCounterValue(msgCounter);
    const bool test1 = OTRadioLink::decodeAndHandleOTSecureOFrame<OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                                                                  OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                                                                  OTFHT::getKeySuccess,
                                                                  OTFHT::setFlagFrameOperation
                                                                 >(msgStart);
    EXPECT_TRUE(test1);
    EXPECT_TRUE(OTFHT::frameOperationCalledFlag);
}
#endif

#endif
