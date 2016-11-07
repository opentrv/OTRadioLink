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
 * OTRadValve tests of secure frames dependent on OTASEGCM
 */

#if defined(EXT_AVAILABLE_ARDUINO_LIB_OTAESGCM)
// Only enable these tests if the OTAESGCM library is available.


#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>

#include <OTAESGCM.h>
#include <OTRadioLink.h>


//static const int AES_KEY_SIZE = 128; // in bits
//static const int GCM_NONCE_LENGTH = 12; // in bytes
//static const int GCM_TAG_LENGTH = 16; // in bytes (default 16, 12 possible)
//
//// All-zeros const 16-byte/128-bit key.
//// Can be used for other purposes.
//static const uint8_t zeroBlock[16] = { };


// Test something.
TEST(OTAESGCMSecureFrame,basics)
{
}

// Test quick integrity checks, for TX and RX.
//
// DHD20161107: imported from test_SECFRAME.ino testFramQIC().
TEST(OTAESGCMSecureFrame, FramQIC)
{
    OTRadioLink::SecurableFrameHeader sfh;
    uint8_t id[OTRadioLink::SecurableFrameHeader::maxIDLength];
    uint8_t buf[OTRadioLink::SecurableFrameHeader::maxSmallFrameSize + 1];
    // Uninitialised SecurableFrameHeader should be 'invalid'.
    EXPECT_TRUE(sfh.isInvalid());
    // ENCODE
    // Test various bad input combos that should be caught by QIC.
    // Can futz (some of the) inputs that should not matter...
    // Should fail with bad ID length.
    EXPECT_EQ(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_BasicSensorOrValve,
                                               OTV0P2BASE::randRNG8(),
                                               id, OTRadioLink::SecurableFrameHeader::maxIDLength + 1,
                                               2,
                                               1));
    // Should fail with bad buffer length.
    EXPECT_EQ(0, sfh.checkAndEncodeSmallFrameHeader(buf, 0,
                                               false, OTRadioLink::FTS_BasicSensorOrValve,
                                               OTV0P2BASE::randRNG8(),
                                               id, 2,
                                               2,
                                               1));
    // Should fail with bad frame type.
    EXPECT_EQ(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_NONE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 2,
                                               2,
                                               1));
    EXPECT_EQ(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_INVALID_HIGH,
                                               OTV0P2BASE::randRNG8(),
                                               id, 2,
                                               2,
                                               1));
    // Should fail with impossible body length.
    EXPECT_EQ(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               252,
                                               1));
    // Should fail with impossible trailer length.
    EXPECT_EQ(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               0,
                                               0));
    EXPECT_EQ(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               0,
                                               252));
    // Should fail with impossible body + trailer length (for small frame).
    EXPECT_EQ(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               32,
                                               32));
    // "I'm Alive!" message with 1-byte ID should succeed and be of full header length (5).
    EXPECT_EQ(5, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1, // Minimal (non-empty) ID.
                                               0, // No payload.
                                               1));
    // Large but legal body size.
    EXPECT_EQ(5, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1, // Minimal (non-empty) ID.
                                               32,
                                               1));
    // DECODE
    // Test various bad input combos that should be caught by QIC.
    // Can futz (some of the) inputs that should not matter...
    // Should fail with bad (too small) buffer.
    buf[0] = OTV0P2BASE::randRNG8();
    EXPECT_EQ(0, sfh.checkAndDecodeSmallFrameHeader(buf, 0));
    // Should fail with bad (too small) frame length.
    buf[0] = 3 & OTV0P2BASE::randRNG8();
    EXPECT_EQ(0, sfh.checkAndDecodeSmallFrameHeader(buf, sizeof(buf)));
    // Should fail with bad (too large) frame length for 'small' frame.
    buf[0] = 64;
    EXPECT_EQ(0, sfh.checkAndDecodeSmallFrameHeader(buf, sizeof(buf)));
    // Should fail with bad (too large) frame header for the input buffer.
    const uint8_t buf1[] = { 0x08, 0x4f, 0x02, 0x80, 0x81 }; // , 0x02, 0x00, 0x01, 0x23 };
    EXPECT_EQ(0, sfh.checkAndDecodeSmallFrameHeader(buf1, 5));
    // Should fail with bad trailer byte (illegal 0x00 value).
    const uint8_t buf2[] = { 0x08, 0x4f, 0x02, 0x80, 0x81, 0x02, 0x00, 0x01, 0x00 };
    EXPECT_EQ(0, sfh.checkAndDecodeSmallFrameHeader(buf2, sizeof(buf2)));
    // Should fail with bad trailer byte (illegal 0xff value).
    const uint8_t buf3[] = { 0x08, 0x4f, 0x02, 0x80, 0x81, 0x02, 0x00, 0x01, 0xff };
    EXPECT_EQ(0, sfh.checkAndDecodeSmallFrameHeader(buf3, sizeof(buf3)));
    // TODO
    // TODO
    // TODO
}



#endif // ARDUINO_LIB_OTAESGCM
