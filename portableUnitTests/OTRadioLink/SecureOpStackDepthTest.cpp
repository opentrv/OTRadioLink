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
                           Deniz Erbilgin 2017
*/

/*
 * OTRadValve tests of secure frame operations dependent on OTASEGCM
 * with a particular view to managing stack depth to avoid overflow
 * especially on very limited RAM platforms such as AVR.
 */

// Only enable these tests if the OTAESGCM library is marked as available.
#if defined(EXT_AVAILABLE_ARDUINO_LIB_OTAESGCM)


#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>

#include <OTV0p2Base.h>
#include <OTAESGCM.h>
#include <OTRadioLink.h>


//static const int AES_KEY_SIZE = 128; // in bits
//static const int GCM_NONCE_LENGTH = 12; // in bytes
//static const int GCM_TAG_LENGTH = 16; // in bytes (default 16, 12 possible)

// All-zeros const 16-byte/128-bit key.
// Can be used for other purposes.
//static const uint8_t zeroBlock[16] = { };

namespace SOSDT {
    // Max stack usage in bytes
    // 20170511
    //           enc, dec, enc*, dec*
    // - DE:     208, 208, 208,  208
    // - DHD:    ???, ???, 358,  ???
    // - Travis: 192, 224, ???,  ???
    // * using a workspace
    #ifndef __APPLE__
    //static constexpr unsigned int maxStackSecureFrameEncode = 328;
    static constexpr unsigned int maxStackSecureFrameDecode = 1600; // was 1024. clang uses more stack
    #else
    // For macOS / clang++ (pretending to be G++) builds.
    // On macOS 10.12.5 system, secure frame enc/decode ~358 bytes at 20170511.
    // static constexpr unsigned int maxStackSecureFrameEncode = 1024;
    static constexpr unsigned int maxStackSecureFrameDecode = 1520;
    #endif // __APPLE__

    bool pollIO(bool) {return (false);}
    bool getKeySuccess(uint8_t *key) { memset(key, 0x0,  /*OTV0P2BASE::VOP2BASE_EE_LEN_16BYTE_PRIMARY_BUILDING_KEY*/ 16); return (true); }

    struct minimumSecureFrame
    {
        // Preshared ID prefix; only an initial part/prefix of this goes on the wire in the header.
        static const uint8_t id[];
        // IV/nonce starting with first 6 bytes of preshared ID, then 6 bytes of counter.
        static const uint8_t iv[];
        static const uint8_t oldCounter[];
        // 'O' frame body with some JSON stats.
        static const uint8_t body[];
        // length of secure frame
        static const uint8_t encodedLength;
        // Buffer containing secure frame. Generated using code bellow.
        static const uint8_t buf[];

        // Stuff used to generate a working encodable frame. Taken from SecureFrameTest.cpp
            // All-zeros const 16-byte/128-bit key.
            // Can be used for other purposes.
        //    static const uint8_t zeroBlock[16] = { };
        //    uint8_t buf[OTRadioLink::SecurableFrameHeader::maxSmallFrameSize];
        //    //Example 3: secure, no valve, representative minimum stats {"b":1}).
        //    //Note that the sequence number must match the 4 lsbs of the message count, ie from iv[11].
        //    //and the ID is 0xaa 0xaa 0xaa 0xaa (transmitted) with the next ID bytes 0x55 0x55.
        //    //ResetCounter = 42
        //    //TxMsgCounter = 793
        //    //(Thus nonce/IV: aa aa aa aa 55 55 00 00 2a 00 03 19)
        //    //
        //    //3e cf 94 aa aa aa aa 20 | b3 45 f9 29 69 57 0c b8 28 66 14 b4 f0 69 b0 08 71 da d8 fe 47 c1 c3 53 83 48 88 03 7d 58 75 75 | 00 00 2a 00 03 19 29 3b 31 52 c3 26 d2 6d d0 8d 70 1e 4b 68 0d cb 80
        //    //
        //    //3e  length of header (62) after length byte 5 + (encrypted) body 32 + trailer 32
        //    //cf  'O' secure OpenTRV basic frame
        //    //04  0 sequence number, ID length 4
        //    //aa  ID byte 1
        //    //aa  ID byte 2
        //    //aa  ID byte 3
        //    //aa  ID byte 4
        //    //20  body length 32 (after padding and encryption)
        //    //    Plaintext body (length 8): 0x7f 0x11 { " b " : 1
        //    //    Padded: 7f 11 7b 22 62 22 3a 31 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 17
        //    //b3 45 f9 ... 58 75 75  32 bytes of encrypted body
        //    //00 00 2a  reset counter
        //    //00 03 19  message counter
        //    //29 3b 31 ... 68 0d cb  16 bytes of authentication tag
        //    //80  enc/auth type/format indicator.
        //    const uint8_t encodedLength = OTRadioLink::SimpleSecureFrame32or0BodyTXBase::encodeSecureSmallFrameRaw(buf, sizeof(buf),
        //                                    OTRadioLink::FTS_BasicSensorOrValve,
        //                                    id, 4,
        //                                    body, sizeof(body),
        //                                    iv,
        //                                    OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_STATELESS,
        //                                    NULL, zeroBlock);
        //    std::fprintf(stdout, "const uint8_t encodedLength = %u;\n", encodedLength);
        //    std::cout << "const uint8_t buf[] = {\n\t";
        //    for(auto i = 0; i < OTRadioLink::SecurableFrameHeader::maxSmallFrameSize; ++i) {
        //        std::fprintf(stdout, "0x%x, ", buf[i]);
        //        if(7 == (i % 8)) std::cout << "\n\t";
        //    }
        //    std::cout << " };\n";
    };
    // Preshared ID prefix; only an initial part/prefix of this goes on the wire in the header.
    const uint8_t minimumSecureFrame::id[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55 };
    // IV/nonce starting with first 6 bytes of preshared ID, then 6 bytes of counter.
    const uint8_t minimumSecureFrame::iv[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x00, 0x00, 0x2a, 0x00, 0x03, 0x19 };
    const uint8_t minimumSecureFrame::oldCounter[] = { 0x00, 0x00, 0x2a, 0x00, 0x03, 0x18 };
    // 'O' frame body with some JSON stats.
    const uint8_t minimumSecureFrame::body[] = { 0x7f, 0x11, 0x7b, 0x22, 0x62, 0x22, 0x3a, 0x31 };
    // length of secure frame
    const uint8_t minimumSecureFrame::encodedLength = 63;
    // Buffer containing secure frame. Generated using code bellow.
    const uint8_t minimumSecureFrame::buf[] = {
        0x3e, 0xcf, 0x94, 0xaa, 0xaa, 0xaa, 0xaa, 0x20,
        0xb3, 0x45, 0xf9, 0x29, 0x69, 0x57, 0x0c, 0xb8,
        0x28, 0x66, 0x14, 0xb4, 0xf0, 0x69, 0xb0, 0x08,
        0x71, 0xda, 0xd8, 0xfe, 0x47, 0xc1, 0xc3, 0x53,
        0x83, 0x48, 0x88, 0x03, 0x7d, 0x58, 0x75, 0x75,
        0x00, 0x00, 0x2a, 0x00, 0x03, 0x19, 0x29, 0x3b,
        0x31, 0x52, 0xc3, 0x26, 0xd2, 0x6d, 0xd0, 0x8d,
        0x70, 0x1e, 0x4b, 0x68, 0x0d, 0xcb, 0x80 };

    /**
     * @brief   Compare the decrypted string with the original plain text.
     *          Set frameOperationCalledFlag true if they match.
     */
    volatile bool frameOperationCalledFlag = false;
    OTRadioLink::frameOperator_fn_t setFlagFrameOperation;
    bool setFlagFrameOperation(const OTRadioLink::OTFrameData_T &fd) {
        if(0 == strncmp((const char *) fd.decryptedBody, (const char *) minimumSecureFrame::body, sizeof(minimumSecureFrame::body))) {
            frameOperationCalledFlag = true;
        }
        return (true);
    }

}


TEST(SecureOpStackDepth, StackCheckerWorks)
{
    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();
    EXPECT_NE((size_t)0, baseStack);
}

TEST(SecureOpStackDepth, SimpleSecureFrame32or0BodyRXFixedCounterBasic)
{
    const uint8_t counter = 0;
    const uint8_t id = 0;
    OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter &sfrx = OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter::getInstance();
    EXPECT_TRUE(sfrx.updateRXMessageCountAfterAuthentication(&id, &counter));
}

namespace SOSDT
{
    /**
     * @brief wrapper around decode function so that we can include top level stack allocations in test.
     */
    bool decodeAndHandleSecureFrameNoWorkspace()
    {
        // Secure Frame start
        const uint8_t * senderID = SOSDT::minimumSecureFrame::id;
        const uint8_t * msgCounter = SOSDT::minimumSecureFrame::oldCounter;
        const uint8_t * const msgStart = &SOSDT::minimumSecureFrame::buf[1];

        OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter &sfrx = OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter::getInstance();
        sfrx.setMockIDValue(senderID);
        sfrx.setMockCounterValue(msgCounter);
        return(OTRadioLink::decodeAndHandleOTSecureOFrame<OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                                                                      OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                                                                      SOSDT::getKeySuccess,
                                                                      SOSDT::setFlagFrameOperation
                                                                     >(msgStart));
    }
}
TEST(SecureOpStackDepth, SimpleSecureFrame32or0BodyRXFixedCounterStack)
{
    // Make sure flag is false.
    SOSDT::frameOperationCalledFlag = false;


    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();

    const bool test1 = SOSDT::decodeAndHandleSecureFrameNoWorkspace();

    const size_t maxStack = OTV0P2BASE::MemoryChecks::getMinSP();
    // Uncomment to print stack usage
     std::cout << "decodeAndHandleOTSecureOFrame stack: " << baseStack - maxStack << "\n";

    EXPECT_TRUE(test1);
    EXPECT_TRUE(SOSDT::frameOperationCalledFlag);
    EXPECT_GT(SOSDT::maxStackSecureFrameDecode, baseStack - maxStack);
}

TEST(SecureOpStackDepth, OTMessageQueueHandlerStackBasic)
{
    // Make sure flag is false.
    SOSDT::frameOperationCalledFlag = false;
    // Secure Frame start
    const uint8_t * senderID = SOSDT::minimumSecureFrame::id;
    const uint8_t * msgCounter = SOSDT::minimumSecureFrame::oldCounter;
//     const uint8_t * const msgStart = &SOSDT::minimumSecureFrame::buf[1];

    OTRadioLink::OTRadioLinkMock rl;
    memcpy(rl.message, SOSDT::minimumSecureFrame::buf, SOSDT::minimumSecureFrame::encodedLength + 1);

    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();


    OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter &sfrx = OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter::getInstance();
    sfrx.setMockIDValue(senderID);
    sfrx.setMockCounterValue(msgCounter);

    OTRadioLink::OTMessageQueueHandler<
        SOSDT::pollIO, 4800,
        OTRadioLink::decodeAndHandleOTSecureOFrame<OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                                                                      OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                                                                      SOSDT::getKeySuccess,
                                                                      SOSDT::setFlagFrameOperation
                                                                     >
                                                  > mh;

    EXPECT_TRUE(mh.handle(false, rl));


    const size_t maxStack = OTV0P2BASE::MemoryChecks::getMinSP();
    // Uncomment to print stack usage
    //  std::cout << "OTMessageQueueHandler stack: " << baseStack - maxStack << "\n";

    // EXPECT_TRUE(test1);
    EXPECT_TRUE(SOSDT::frameOperationCalledFlag);
    EXPECT_GT(SOSDT::maxStackSecureFrameDecode, baseStack - maxStack);
}
namespace SOSDT
{
/**
 * @brief wrapper around decode function so that we can include top level stack allocations in test.
 */
    bool decodeAndHandleSecureFrameWithWorkspace()
    {
        // Secure Frame start
        const uint8_t * senderID = SOSDT::minimumSecureFrame::id;
        const uint8_t * msgCounter = SOSDT::minimumSecureFrame::oldCounter;
        const uint8_t * const msgStart = &SOSDT::minimumSecureFrame::buf[1];
        // Do encryption via simplified interface.
        constexpr uint8_t workspaceSize = 180 + 14;  // FIXME! Find out required space, parametrise for different computers.
        uint8_t workspace[workspaceSize];
        OTV0P2BASE::ScratchSpace sW(workspace, workspaceSize);
        OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter &sfrx = OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter::getInstance();
        sfrx.setMockIDValue(senderID);
        sfrx.setMockCounterValue(msgCounter);
        return(OTRadioLink::decodeAndHandleOTSecureOFrameWithWorkspace<OTRadioLink::SimpleSecureFrame32or0BodyRXFixedCounter,
                                                                      OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_WITH_WORKSPACE,
                                                                      SOSDT::getKeySuccess,
                                                                      SOSDT::setFlagFrameOperation
                                                                     >(msgStart, sW));
    }
}
TEST(SecureOpStackDepth, SimpleSecureFrame32or0BodyRXFixedCounterWithWorkspaceStack)
{
    // Make sure flag is false.
    SOSDT::frameOperationCalledFlag = false;

    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();

    const bool test1 = SOSDT::decodeAndHandleSecureFrameWithWorkspace();

    const size_t maxStack = OTV0P2BASE::MemoryChecks::getMinSP();

    // Uncomment to print stack usage
    std::cout << "decodeAndHandleOTSecureOFramewW stack: " << baseStack - maxStack << "\n";

    EXPECT_TRUE(test1);
    EXPECT_TRUE(SOSDT::frameOperationCalledFlag);
    EXPECT_GE(SOSDT::maxStackSecureFrameDecode, baseStack - maxStack);
}
/**
 * Stack usage:
 * Date     | SimpleSecureFrame32or0BodyRXFixedCounterStack (OTMessageQueueHandlerStackBasic) | notes
 * 20170703 | 816 (928) | 928 probably mostly due to use of OTRadioLinkMock.
 */

#endif // ARDUINO_LIB_OTAESGCM
