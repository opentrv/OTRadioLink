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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2016
*/

/*
 * EXTRA unit test routines for secureable-frame library code.
 *
 * Should all be runnable on any UNO-alike or emulator;
 * does not need V0p2 boards or hardware.
 *
 * Some tests here may duplicate those in the core libraries,
 * which is OK in order to double-check some core functionality.
 */

// Arduino libraries imported here (even for use in other .cpp files).

#define UNIT_TESTS

#include <Wire.h>

// Include the library under test.
#include <OTV0p2Base.h>
// Also testing against crypto.
#include <OTAESGCM.h>

#include <OTRadioLink.h>
#include <OTRadValve.h>
#include <OTRFM23BLink.h>
#include <OTRN2483Link.h>
#include <OTSIM900Link.h>

#if F_CPU == 1000000 // 1MHz CPU indicates V0p2 board.
#define ON_V0P2_BOARD
#endif

void setup()
  {
#ifdef ON_V0P2_BOARD
  // initialize serial communications at 4800 bps for typical use with V0p2 board.
  Serial.begin(4800);
#else
  // initialize serial communications at 9600 bps for typical use with (eg) Arduino UNO.
  Serial.begin(9600);
#endif
  }



// Error exit from failed unit test, one int parameter and the failing line number to print...
// Expects to terminate like panic() with flashing light can be detected by eye or in hardware if required.
static void error(int err, int line)
  {
  for( ; ; )
    {
    Serial.print(F("***Test FAILED*** val="));
    Serial.print(err, DEC);
    Serial.print(F(" =0x"));
    Serial.print(err, HEX);
    if(0 != line)
      {
      Serial.print(F(" at line "));
      Serial.print(line);
      }
    Serial.println();
//    LED_HEATCALL_ON();
//    tinyPause();
//    LED_HEATCALL_OFF();
//    sleepLowPowerMs(1000);
    delay(1000);
    }
  }

// Deal with common equality test.
static inline void errorIfNotEqual(int expected, int actual, int line) { if(expected != actual) { error(actual, line); } }
// Allowing a delta.
static inline void errorIfNotEqual(int expected, int actual, int delta, int line) { if(abs(expected - actual) > delta) { error(actual, line); } }

// Test expression and bucket out with error if false, else continue, including line number.
// Macros allow __LINE__ to work correctly.
#define AssertIsTrueWithErr(x, err) { if(!(x)) { error((err), __LINE__); } }
#define AssertIsTrue(x) AssertIsTrueWithErr((x), 0)
#define AssertIsEqual(expected, x) { errorIfNotEqual((expected), (x), __LINE__); }
#define AssertIsEqualWithDelta(expected, x, delta) { errorIfNotEqual((expected), (x), (delta), __LINE__); }


// Check that correct version of this library is under test.
static void testLibVersion()
  {
  Serial.println("LibVersion");
#if !(1 == ARDUINO_LIB_OTRADIOLINK_VERSION_MAJOR) || !(0 == ARDUINO_LIB_OTRADIOLINK_VERSION_MINOR)
#error Wrong library version!
#endif
  }

// Check that correct versions of underlying libraries are in use.
static void testLibVersions()
  {
  Serial.println("LibVersions");
#if !(1 == ARDUINO_LIB_OTV0P2BASE_VERSION_MAJOR) || !(0 == ARDUINO_LIB_OTV0P2BASE_VERSION_MINOR)
#error Wrong library version!
#endif

#if !(1 == ARDUINO_LIB_OTAESGCM_VERSION_MAJOR) || !(0 <= ARDUINO_LIB_OTAESGCM_VERSION_MINOR)
#error Wrong library version!
#endif
  }


static const int AES_KEY_SIZE = 128; // in bits
static const int GCM_NONCE_LENGTH = 12; // in bytes
static const int GCM_TAG_LENGTH = 16; // in bytes (default 16, 12 possible)

// All-zeros const 16-byte/128-bit key.
// Can be used for other purposes.
static const uint8_t zeroBlock[16] = { };

// Test quick integrity checks, for TX and RX.
static void testFrameQIC()
  {
  Serial.println("FramQIC");
  OTRadioLink::SecurableFrameHeader sfh;
  uint8_t id[OTRadioLink::SecurableFrameHeader::maxIDLength];
  uint8_t buf[OTRadioLink::SecurableFrameHeader::maxSmallFrameSize + 1];
  // Uninitialised SecurableFrameHeader should be 'invalid'.
  AssertIsTrue(sfh.isInvalid());
  // ENCODE
  // Test various bad input combos that should be caught by QIC.
  // Can futz (some of the) inputs that should not matter...
  // Should fail with bad ID length.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_BasicSensorOrValve,
                                               OTV0P2BASE::randRNG8(),
                                               id, OTRadioLink::SecurableFrameHeader::maxIDLength + 1,
                                               2,
                                               1));
  // Should fail with bad buffer length.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, 0,
                                               false, OTRadioLink::FTS_BasicSensorOrValve,
                                               OTV0P2BASE::randRNG8(),
                                               id, 2,
                                               2,
                                               1));
  // Should fail with bad frame type.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_NONE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 2,
                                               2,
                                               1));
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_INVALID_HIGH,
                                               OTV0P2BASE::randRNG8(),
                                               id, 2,
                                               2,
                                               1));
  // Should fail with impossible body length.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               252,
                                               1));
  // Should fail with impossible trailer length.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               0,
                                               0));
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               0,
                                               252));
  // Should fail with impossible body + trailer length (for small frame).
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1,
                                               32,
                                               32));
  // "I'm Alive!" message with 1-byte ID should succeed and be of full header length (5).
  AssertIsEqual(5, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               id, 1, // Minimal (non-empty) ID.
                                               0, // No payload.
                                               1));
  // Large but legal body size.
  AssertIsEqual(5, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
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
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf, 0));
  // Should fail with bad (too small) frame length.
  buf[0] = 3 & OTV0P2BASE::randRNG8();
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf, sizeof(buf)));
  // Should fail with bad (too large) frame length for 'small' frame.
  buf[0] = 64;
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf, sizeof(buf)));
  // Should fail with bad (too large) frame header for the input buffer.
  const uint8_t buf1[] = { 0x08, 0x4f, 0x02, 0x80, 0x81 }; // , 0x02, 0x00, 0x01, 0x23 };
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf1, 5));
  // Should fail with bad trailer byte (illegal 0x00 value).
  const uint8_t buf2[] = { 0x08, 0x4f, 0x02, 0x80, 0x81, 0x02, 0x00, 0x01, 0x00 };
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf2, sizeof(buf2)));
  // Should fail with bad trailer byte (illegal 0xff value).
  const uint8_t buf3[] = { 0x08, 0x4f, 0x02, 0x80, 0x81, 0x02, 0x00, 0x01, 0xff };
  AssertIsEqual(0, sfh.checkAndDecodeSmallFrameHeader(buf3, sizeof(buf3)));
  // TODO
  // TODO
  // TODO
  }

// Test encoding of header for TX.
static void testFrameHeaderEncoding()
  {
  Serial.println("FrameHeaderEncoding");
  OTRadioLink::SecurableFrameHeader sfh;
  uint8_t id[OTRadioLink::SecurableFrameHeader::maxIDLength];
  uint8_t buf[OTRadioLink::SecurableFrameHeader::maxSmallFrameSize];
  //
  // Test vector 1 / example from the spec.
  //Example insecure frame, valve unit 0% open, no call for heat/flags/stats.
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //08 4f 02 80 81 02 | 00 01 | 23
  //
  //08 length of header (8) after length byte 5 + body 2 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //02 body length 2
  //00 valve 0%, no call for heat
  //01 no flags or stats, unreported occupancy
  //23 CRC value
  id[0] = 0x80;
  id[1] = 0x81;
  AssertIsEqual(6, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_BasicSensorOrValve,
                                               0,
                                               id, 2,
                                               2,
                                               1));
  AssertIsEqual(0x08, buf[0]);
  AssertIsEqual(0x4f, buf[1]);
  AssertIsEqual(0x02, buf[2]);
  AssertIsEqual(0x80, buf[3]);
  AssertIsEqual(0x81, buf[4]);
  AssertIsEqual(0x02, buf[5]);
  // Check related parameters.
  AssertIsEqual(8, sfh.fl);
  AssertIsEqual(6, sfh.getBodyOffset());
  AssertIsEqual(8, sfh.getTrailerOffset());
  //
  // Test vector 2 / example from the spec.
  //Example insecure frame, no valve, representative minimum stats {"b":1}
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //0e 4f 02 80 81 08 | 7f 11 7b 22 62 22 3a 31 | 61
  //
  //0e length of header (14) after length byte 5 + body 8 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //08 body length 8
  //7f no valve, no call for heat
  //11 stats present flag only, unreported occupancy
  //7b 22 62 22 3a 31  {"b":1  Stats: note that implicit trailing '}' is not sent.
  //61 CRC value
  id[0] = 0x80;
  id[1] = 0x81;
  AssertIsEqual(6, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_BasicSensorOrValve,
                                               0,
                                               id, 2,
                                               8,
                                               1));
  AssertIsEqual(0x0e, buf[0]);
  AssertIsEqual(0x4f, buf[1]);
  AssertIsEqual(0x02, buf[2]);
  AssertIsEqual(0x80, buf[3]);
  AssertIsEqual(0x81, buf[4]);
  AssertIsEqual(0x08, buf[5]);
  // Check related parameters.
  AssertIsEqual(14, sfh.fl);
  AssertIsEqual(6, sfh.getBodyOffset());
  AssertIsEqual(14, sfh.getTrailerOffset());
  }

// Test decoding of header for RX.
static void testFrameHeaderDecoding()
  {
  Serial.println("FrameHeaderDecoding");
  OTRadioLink::SecurableFrameHeader sfh;
  //
  // Test vector 1 / example from the spec.
  //Example insecure frame, valve unit 0% open, no call for heat/flags/stats.
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //08 4f 02 80 81 02 | 00 01 | 23
  //
  //08 length of header (8) after length byte 5 + body 2 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //02 body length 2
  //00 valve 0%, no call for heat
  //01 no flags or stats, unreported occupancy
  //23 CRC value
  const uint8_t buf1[] = { 0x08, 0x4f, 0x02, 0x80, 0x81, 0x02, 0x00, 0x01, 0x23 };
  AssertIsEqual(6, sfh.checkAndDecodeSmallFrameHeader(buf1, sizeof(buf1)));
  // Check decoded parameters.
  AssertIsEqual(8, sfh.fl);
  AssertIsEqual(2, sfh.getIl());
  AssertIsEqual(0x80, sfh.id[0]);
  AssertIsEqual(0x81, sfh.id[1]);
  AssertIsEqual(2, sfh.bl);
  AssertIsEqual(1, sfh.getTl());
  AssertIsEqual(6, sfh.getBodyOffset());
  AssertIsEqual(8, sfh.getTrailerOffset());
  //
  // Test vector 2 / example from the spec.
  //Example insecure frame, no valve, representative minimum stats {"b":1}
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //0e 4f 02 80 81 08 | 7f 11 7b 22 62 22 3a 31 | 61
  //
  //0e length of header (14) after length byte 5 + body 8 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //08 body length 8
  //7f no valve, no call for heat
  //11 stats present flag only, unreported occupancy
  //7b 22 62 22 3a 31  {"b":1  Stats: note that implicit trailing '}' is not sent.
  //61 CRC value
  static const uint8_t buf2[] = { 0x0e, 0x4f, 0x02, 0x80, 0x81, 0x08, 0x7f, 0x11, 0x7b, 0x22, 0x62, 0x22, 0x3a, 0x31, 0x61 };
  AssertIsEqual(6, sfh.checkAndDecodeSmallFrameHeader(buf2, sizeof(buf2)));
  // Check decoded parameters.
  AssertIsEqual(14, sfh.fl);
  AssertIsEqual(2, sfh.getIl());
  AssertIsEqual(0x80, sfh.id[0]);
  AssertIsEqual(0x81, sfh.id[1]);
  AssertIsEqual(8, sfh.bl);
  AssertIsEqual(1, sfh.getTl());
  AssertIsEqual(6, sfh.getBodyOffset());
  AssertIsEqual(14, sfh.getTrailerOffset());
  }

// Test CRC computation for insecure frames.
static void testNonsecureFrameCRC()
  {
  Serial.println("NonsecureFrameCRC");
  OTRadioLink::SecurableFrameHeader sfh;
  //
  // Test vector 1 / example from the spec.
  //Example insecure frame, valve unit 0% open, no call for heat/flags/stats.
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //08 4f 02 80 81 02 | 00 01 | 23
  //
  //08 length of header (8) after length byte 5 + body 2 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //02 body length 2
  //00 valve 0%, no call for heat
  //01 no flags or stats, unreported occupancy
  //23 CRC value
  const uint8_t buf1[] = { 0x08, 0x4f, 0x02, 0x80, 0x81, 0x02, 0x00, 0x01, 0x23 };
  AssertIsEqual(6, sfh.checkAndDecodeSmallFrameHeader(buf1, 6));
  AssertIsEqual(0x23, sfh.computeNonSecureFrameCRC(buf1, sizeof(buf1) - 1));
  // Decode entire frame, emulating RX, structurally validating the header then checking the CRC.
  AssertIsTrue(0 != sfh.checkAndDecodeSmallFrameHeader(buf1, sizeof(buf1)));
  AssertIsTrue(0 != decodeNonsecureSmallFrameRaw(&sfh, buf1, sizeof(buf1)));
  //
  // Test vector 2 / example from the spec.
  //Example insecure frame, no valve, representative minimum stats {"b":1}
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //0e 4f 02 80 81 08 | 7f 11 7b 22 62 22 3a 31 | 61
  //
  //0e length of header (14) after length byte 5 + body 8 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //08 body length 8
  //7f no valve, no call for heat
  //11 stats present flag only, unreported occupancy
  //7b 22 62 22 3a 31  {"b":1  Stats: note that implicit trailing '}' is not sent.
  //61 CRC value
  const uint8_t buf2[] = { 0x0e, 0x4f, 0x02, 0x80, 0x81, 0x08, 0x7f, 0x11, 0x7b, 0x22, 0x62, 0x22, 0x3a, 0x31, 0x61 };
  // Just decode and check the frame header first
  AssertIsEqual(6, sfh.checkAndDecodeSmallFrameHeader(buf2, 6));
  AssertIsEqual(0x61, sfh.computeNonSecureFrameCRC(buf2, sizeof(buf2) - 1));
  // Decode entire frame, emulating RX, structurally validating the header then checking the CRC.
  AssertIsTrue(0 != sfh.checkAndDecodeSmallFrameHeader(buf2, sizeof(buf2)));
  AssertIsTrue(0 != decodeNonsecureSmallFrameRaw(&sfh, buf2, sizeof(buf2)));
  }

// Test encoding of entire non-secure frame for TX.
static void testNonsecureSmallFrameEncoding()
  {
  Serial.println("NonSecureSmallFrameEncoding");
  uint8_t buf[OTRadioLink::SecurableFrameHeader::maxSmallFrameSize];
  //
  // Test vector 1 / example from the spec.
  //Example insecure frame, valve unit 0% open, no call for heat/flags/stats.
  //In this case the frame sequence number is zero, and ID is 0x80 0x81.
  //
  //08 4f 02 80 81 02 | 00 01 | 23
  //
  //08 length of header (8) after length byte 5 + body 2 + trailer 1
  //4f 'O' insecure OpenTRV basic frame
  //02 0 sequence number, ID length 2
  //80 ID byte 1
  //81 ID byte 2
  //02 body length 2
  //00 valve 0%, no call for heat
  //01 no flags or stats, unreported occupancy
  //23 CRC value
  const uint8_t id[] =  { 0x80, 0x81 };
  const uint8_t body[] = { 0x00, 0x01 };
  AssertIsEqual(9, OTRadioLink::encodeNonsecureSmallFrame(buf, sizeof(buf),
                                    OTRadioLink::FTS_BasicSensorOrValve,
                                    0,
                                    id, 2,
                                    body, 2));
  AssertIsEqual(0x08, buf[0]);
  AssertIsEqual(0x4f, buf[1]);
  AssertIsEqual(0x02, buf[2]);
  AssertIsEqual(0x80, buf[3]);
  AssertIsEqual(0x81, buf[4]);
  AssertIsEqual(0x02, buf[5]);
  AssertIsEqual(0x00, buf[6]);
  AssertIsEqual(0x01, buf[7]);
  AssertIsEqual(0x23, buf[8]);
  }

// Test simple plain-text padding for encryption.
static void testSimplePadding()
  {
  Serial.println("SimplePadding");
  uint8_t buf[OTRadioLink::ENC_BODY_SMALL_FIXED_CTEXT_SIZE];
  // Provoke failure with NULL buffer.
  AssertIsEqual(0, OTRadioLink::SimpleSecureFrame32or0BodyTXBase::addPaddingTo32BTrailing0sAndPadCount(NULL, 0x1f & OTV0P2BASE::randRNG8()));
  // Provoke failure with over-long unpadded plain-text.
  AssertIsEqual(0, OTRadioLink::SimpleSecureFrame32or0BodyTXBase::addPaddingTo32BTrailing0sAndPadCount(buf, 1 + OTRadioLink::ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE));  
  // Check padding in case with single random data byte (and the rest of the buffer set differently).
  // Check the entire padded result for correctness.
  const uint8_t db0 = OTV0P2BASE::randRNG8();
  buf[0] = db0;
  memset(buf+1, ~db0, sizeof(buf)-1);
  AssertIsEqual(32, OTRadioLink::SimpleSecureFrame32or0BodyTXBase::addPaddingTo32BTrailing0sAndPadCount(buf, 1));
  AssertIsEqual(db0, buf[0]);
  for(int i = 30; --i > 0; ) { AssertIsEqual(0, buf[i]); }
  AssertIsEqual(30, buf[31]);
  // Ensure that unpadding works.
  AssertIsEqual(1, OTRadioLink::SimpleSecureFrame32or0BodyRXBase::removePaddingTo32BTrailing0sAndPadCount(buf));
  AssertIsEqual(db0, buf[0]);
  }

// Test simple fixed-size NULL enc/dec behaviour.
static void testSimpleNULLEncDec()
  {
  const OTRadioLink::SimpleSecureFrame32or0BodyTXBase::fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t e = OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL;
  const OTRadioLink::SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t d = OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleDec_NULL_IMPL;
  // Check that calling the NULL enc routine with bad args fails.
  AssertIsTrue(!e(NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL));
  static const uint8_t plaintext1[32] = { 'a', 'b', 'c', 'd', 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4 };
  static const uint8_t nonce1[12] = { 'q', 'u', 'i', 'c', 'k', ' ', 6, 5, 4, 3, 2, 1 };
  static const uint8_t authtext1[2] = { 'H', 'i' };
  // Output ciphertext and tag buffers.
  uint8_t co1[32], to1[16];
  AssertIsTrue(e(NULL, zeroBlock, nonce1, authtext1, sizeof(authtext1), plaintext1, co1, to1));
  AssertIsEqual(0, memcmp(plaintext1, co1, 32));
  AssertIsEqual(0, memcmp(nonce1, to1, 12));
  AssertIsEqual(0, to1[12]);
  AssertIsEqual(0, to1[15]);
  // Check that calling the NULL decc routine with bad args fails.
  AssertIsTrue(!d(NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL));
  // Decode the ciphertext and tag from above and ensure that it 'works'.
  uint8_t plaintext1Decoded[32];
  AssertIsTrue(d(NULL, zeroBlock, nonce1, authtext1, sizeof(authtext1), co1, to1, plaintext1Decoded));
  AssertIsEqual(0, memcmp(plaintext1, plaintext1Decoded, 32));
  }

// Test a simple fixed-size enc/dec function pair.
// Aborts with Assert...() in case of failure.
static void runSimpleEncDec(const OTRadioLink::SimpleSecureFrame32or0BodyTXBase::fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t e,
                            const OTRadioLink::SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t d)
  {
  // Check that calling the NULL enc routine with bad args fails.
  AssertIsTrue(!e(NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL));
  // Try with plaintext and authext...
  static const uint8_t plaintext1[32] = { 'a', 'b', 'c', 'd', 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4 };
  static const uint8_t nonce1[12] = { 'q', 'u', 'i', 'c', 'k', ' ', 6, 5, 4, 3, 2, 1 };
  static const uint8_t authtext1[2] = { 'H', 'i' };
  // Output ciphertext and tag buffers.
  uint8_t co1[32], to1[16];
  AssertIsTrue(e(NULL, zeroBlock, nonce1, authtext1, sizeof(authtext1), plaintext1, co1, to1));
  // Check that calling the NULL dec routine with bad args fails.
  AssertIsTrue(!d(NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL));
  // Decode the ciphertext and tag from above and ensure that it 'works'.
  uint8_t plaintext1Decoded[32];
  AssertIsTrue(d(NULL, zeroBlock, nonce1, authtext1, sizeof(authtext1), co1, to1, plaintext1Decoded));
  AssertIsEqual(0, memcmp(plaintext1, plaintext1Decoded, 32));
  // Try with authtext and no plaintext.
  AssertIsTrue(e(NULL, zeroBlock, nonce1, authtext1, sizeof(authtext1), NULL, co1, to1));
  AssertIsTrue(d(NULL, zeroBlock, nonce1, authtext1, sizeof(authtext1), NULL, to1, plaintext1Decoded));
  }

// Test basic access to crypto features.
// Check basic operation of the simple fixed-sized encode/decode routines.
static void testCryptoAccess()
  {
  Serial.println("CryptoAccess");
  // NULL enc/dec.
  runSimpleEncDec(OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL,
                  OTRadioLink::fixed32BTextSize12BNonce16BTagSimpleDec_NULL_IMPL);
  // AES-GCM 128-bit key enc/dec.
  runSimpleEncDec(OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_STATELESS,
                  OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS);
  }

// Check using NIST GCMVS test vector.
// Test via fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_STATELESS interface.
// See http://csrc.nist.gov/groups/STM/cavp/documents/mac/gcmvs.pdf
// See http://csrc.nist.gov/groups/STM/cavp/documents/mac/gcmtestvectors.zip
//
//[Keylen = 128]
//[IVlen = 96]
//[PTlen = 256]
//[AADlen = 128]
//[Taglen = 128]
//
//Count = 0
//Key = 298efa1ccf29cf62ae6824bfc19557fc
//IV = 6f58a93fe1d207fae4ed2f6d
//PT = cc38bccd6bc536ad919b1395f5d63801f99f8068d65ca5ac63872daf16b93901
//AAD = 021fafd238463973ffe80256e5b1c6b1
//CT = dfce4e9cd291103d7fe4e63351d9e79d3dfd391e3267104658212da96521b7db
//Tag = 542465ef599316f73a7a560509a2d9f2
//
// keylen = 128, ivlen = 96, ptlen = 256, aadlen = 128, taglen = 128, count = 0
static void testGCMVS1ViaFixed32BTextSize()
  {
  Serial.println("GCMVS1ViaFixed32BTextSize");
  // Inputs to encryption.
  static const uint8_t input[32] = { 0xcc, 0x38, 0xbc, 0xcd, 0x6b, 0xc5, 0x36, 0xad, 0x91, 0x9b, 0x13, 0x95, 0xf5, 0xd6, 0x38, 0x01, 0xf9, 0x9f, 0x80, 0x68, 0xd6, 0x5c, 0xa5, 0xac, 0x63, 0x87, 0x2d, 0xaf, 0x16, 0xb9, 0x39, 0x01 };
  static const uint8_t key[AES_KEY_SIZE/8] = { 0x29, 0x8e, 0xfa, 0x1c, 0xcf, 0x29, 0xcf, 0x62, 0xae, 0x68, 0x24, 0xbf, 0xc1, 0x95, 0x57, 0xfc };
  static const uint8_t nonce[GCM_NONCE_LENGTH] = { 0x6f, 0x58, 0xa9, 0x3f, 0xe1, 0xd2, 0x07, 0xfa, 0xe4, 0xed, 0x2f, 0x6d };
  static const uint8_t aad[16] = { 0x02, 0x1f, 0xaf, 0xd2, 0x38, 0x46, 0x39, 0x73, 0xff, 0xe8, 0x02, 0x56, 0xe5, 0xb1, 0xc6, 0xb1 };
  // Space for outputs from encryption.
  uint8_t tag[GCM_TAG_LENGTH]; // Space for tag.
  uint8_t cipherText[max(32, sizeof(input))]; // Space for encrypted text.
  // Instance to perform enc/dec.
  // Do encryption via simplified interface.
  AssertIsTrue(OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_STATELESS(NULL,
            key, nonce,
            aad, sizeof(aad),
            input,
            cipherText, tag));
  // Check some of the cipher text and tag.
//            "0388DACE60B6A392F328C2B971B2FE78F795AAAB494B5923F7FD89FF948B  61 47 72 C7 92 9C D0 DD 68 1B D8 A3 7A 65 6F 33" :
  AssertIsEqual(0xdf, cipherText[0]);
  AssertIsEqual(0x91, cipherText[5]);
  AssertIsEqual(0xdb, cipherText[sizeof(cipherText)-1]);
  AssertIsEqual(0x24, tag[1]);
  AssertIsEqual(0xd9, tag[14]);
  // Decrypt via simplified interface...
  uint8_t inputDecoded[32];
  AssertIsTrue(OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS(NULL,
            key, nonce,
            aad, sizeof(aad),
            cipherText, tag,
            inputDecoded));
  AssertIsEqual(0, memcmp(input, inputDecoded, 32));
  }

// Test encoding/encryption then decoding/decryption of entire secure frame.
static void testSecureSmallFrameEncoding()
  {
  Serial.println("SecureSmallFrameEncoding");
  uint8_t buf[OTRadioLink::SecurableFrameHeader::maxSmallFrameSize];
  //Example 3: secure, no valve, representative minimum stats {"b":1}).
  //Note that the sequence number must match the 4 lsbs of the message count, ie from iv[11].
  //and the ID is 0xaa 0xaa 0xaa 0xaa (transmitted) with the next ID bytes 0x55 0x55.
  //ResetCounter = 42
  //TxMsgCounter = 793
  //(Thus nonce/IV: aa aa aa aa 55 55 00 00 2a 00 03 19)
  //
  //3e cf 94 aa aa aa aa 20 | b3 45 f9 29 69 57 0c b8 28 66 14 b4 f0 69 b0 08 71 da d8 fe 47 c1 c3 53 83 48 88 03 7d 58 75 75 | 00 00 2a 00 03 19 29 3b 31 52 c3 26 d2 6d d0 8d 70 1e 4b 68 0d cb 80
  //
  //3e  length of header (62) after length byte 5 + (encrypted) body 32 + trailer 32
  //cf  'O' secure OpenTRV basic frame
  //04  0 sequence number, ID length 4
  //aa  ID byte 1
  //aa  ID byte 2
  //aa  ID byte 3
  //aa  ID byte 4
  //20  body length 32 (after padding and encryption)
  //    Plaintext body (length 8): 0x7f 0x11 { " b " : 1 
  //    Padded: 7f 11 7b 22 62 22 3a 31 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 17
  //b3 45 f9 ... 58 75 75  32 bytes of encrypted body
  //00 00 2a  reset counter
  //00 03 19  message counter
  //29 3b 31 ... 68 0d cb  16 bytes of authentication tag
  //80  enc/auth type/format indicator.
  // Preshared ID prefix; only an initial part/prefix of this goes on the wire in the header.
  const uint8_t id[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55 };
  // IV/nonce starting with first 6 bytes of preshared ID, then 6 bytes of counter.
  const uint8_t iv[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x00, 0x00, 0x2a, 0x00, 0x03, 0x19 };
  // 'O' frame body with some JSON stats.
  const uint8_t body[] = { 0x7f, 0x11, 0x7b, 0x22, 0x62, 0x22, 0x3a, 0x31 };
  const uint8_t encodedLength = OTRadioLink::SimpleSecureFrame32or0BodyTXBase::encodeSecureSmallFrameRaw(buf, sizeof(buf),
                                    OTRadioLink::FTS_BasicSensorOrValve,
                                    id, 4,
                                    body, sizeof(body),
                                    iv,
                                    OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_STATELESS,
                                    NULL, zeroBlock);
  AssertIsEqual(63, encodedLength);
  AssertIsTrue(encodedLength <= sizeof(buf));
  //3e cf 04 aa aa aa aa 20 | ...
  AssertIsEqual(0x3e, buf[0]);
  AssertIsEqual(0xcf, buf[1]);
  AssertIsEqual(0x94, buf[2]); // Seq num is iv[11] & 0xf, ie 4 lsbs of message counter (and IV).
  AssertIsEqual(0xaa, buf[3]);
  AssertIsEqual(0xaa, buf[4]);
  AssertIsEqual(0xaa, buf[5]);
  AssertIsEqual(0xaa, buf[6]);
  AssertIsEqual(0x20, buf[7]);
  //... b3 45 f9 29 69 57 0c b8 28 66 14 b4 f0 69 b0 08 71 da d8 fe 47 c1 c3 53 83 48 88 03 7d 58 75 75 | ...
  AssertIsEqual(0xb3, buf[8]); // 1st byte of encrypted body.
  AssertIsEqual(0x75, buf[39]); // 32nd/last byte of encrypted body.
  //... 00 00 2a 00 03 19 29 3b 31 52 c3 26 d2 6d d0 8d 70 1e 4b 68 0d cb 80
  AssertIsEqual(0x00, buf[40]); // 1st byte of counters.
  AssertIsEqual(0x00, buf[41]);
  AssertIsEqual(0x2a, buf[42]);
  AssertIsEqual(0x00, buf[43]); 
  AssertIsEqual(0x03, buf[44]);
  AssertIsEqual(0x19, buf[45]); // Last byte of counters.
  AssertIsEqual(0x29, buf[46]); // 1st byte of tag.
  AssertIsEqual(0xcb, buf[61]); // 16th/last byte of tag.
  AssertIsEqual(0x80, buf[62]); // enc format.
  // To decode, emulating RX, structurally validate unpack the header and extract the ID.
  OTRadioLink::SecurableFrameHeader sfhRX;
  AssertIsTrue(0 != sfhRX.checkAndDecodeSmallFrameHeader(buf, encodedLength));
  // (Nominally a longer ID and key is looked up with the ID in the header, and an iv built.)
  uint8_t decodedBodyOutSize;
  uint8_t decryptedBodyOut[OTRadioLink::ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE];
  // Should decode and authenticate correctly.
  AssertIsTrue(0 != OTRadioLink::SimpleSecureFrame32or0BodyRXBase::decodeSecureSmallFrameRaw(&sfhRX,
                                        buf, encodedLength,
                                        OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                                        NULL, zeroBlock, iv,
                                        decryptedBodyOut, sizeof(decryptedBodyOut), decodedBodyOutSize));
  // Body content should be correctly decrypted and extracted.
  AssertIsEqual(sizeof(body), decodedBodyOutSize);
  AssertIsEqual(0, memcmp(body, decryptedBodyOut, sizeof(body)));
  // Check that flipping any single bit should make the decode fail
  // unless it leaves all info (seqNum, id, body) untouched.
  const uint8_t loc = OTV0P2BASE::randRNG8() % encodedLength;
  const uint8_t mask = (uint8_t) (0x100U >> (1+(OTV0P2BASE::randRNG8()&7)));
  buf[loc] ^= mask;
//  Serial.println(loc);
//  Serial.println(mask);
  AssertIsTrue((0 == sfhRX.checkAndDecodeSmallFrameHeader(buf, encodedLength)) ||
               (0 == OTRadioLink::SimpleSecureFrame32or0BodyRXBase::decodeSecureSmallFrameRaw(&sfhRX,
                                        buf, encodedLength,
                                        OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                                        NULL, zeroBlock, iv,
                                        decryptedBodyOut, sizeof(decryptedBodyOut), decodedBodyOutSize)) ||
               ((sizeof(body) == decodedBodyOutSize) && (0 == memcmp(body, decryptedBodyOut, sizeof(body))) && (0 == memcmp(id, sfhRX.id, 4))));
  }

// Test encoding of beacon frames.
static void testBeaconEncoding()
  {
  Serial.println("BeaconEncoding");
  // Non-secure beacon.
  uint8_t buf[max(OTRadioLink::generateNonsecureBeaconMaxBufSize, OTRadioLink::SimpleSecureFrame32or0BodyTXBase::generateSecureBeaconMaxBufSize)];
  // Generate zero-length-ID beacon.
  const uint8_t b0 = OTRadioLink::generateNonsecureBeacon(buf, sizeof(buf), 0, NULL, 0);
  AssertIsEqual(5, b0);
  AssertIsEqual(0x04, buf[0]);
  AssertIsEqual(0x21, buf[1]);
  AssertIsEqual(0x00, buf[2]);
  AssertIsEqual(0x00, buf[3]); // Body length 0.
  AssertIsEqual(0x65, buf[4]);
  // Generate maximum-length-zero-ID beacon automatically at non-zero seq.
  const uint8_t b1 = OTRadioLink::generateNonsecureBeacon(buf, sizeof(buf), 4, zeroBlock, OTRadioLink::SecurableFrameHeader::maxIDLength);
  AssertIsEqual(13, b1);
  AssertIsEqual(0x0c, buf[0]);
  AssertIsEqual(0x21, buf[1]);
  AssertIsEqual(0x48, buf[2]);
  AssertIsEqual(0x00, buf[3]);
  AssertIsEqual(0x00, buf[4]);
  AssertIsEqual(0x00, buf[5]);
  AssertIsEqual(0x00, buf[6]);
  AssertIsEqual(0x00, buf[7]);
  AssertIsEqual(0x00, buf[8]);
  AssertIsEqual(0x00, buf[9]);
  AssertIsEqual(0x00, buf[10]);
  AssertIsEqual(0x00, buf[11]); // Body length 0.
  AssertIsEqual(0x29, buf[12]);
  // Generate maximum-length-from-EEPROM-ID beacon automatically at non-zero seq.
  const uint8_t b2 = OTRadioLink::generateNonsecureBeacon(buf, sizeof(buf), 5, NULL, OTRadioLink::SecurableFrameHeader::maxIDLength);
  AssertIsEqual(13, b2);
  AssertIsEqual(0x0c, buf[0]);
  AssertIsEqual(0x21, buf[1]);
  AssertIsEqual(0x58, buf[2]);
  for(uint8_t i = 0; i < OTRadioLink::SecurableFrameHeader::maxIDLength; ++i)
    { AssertIsEqual(eeprom_read_byte((uint8_t *)(V0P2BASE_EE_START_ID + i)), buf[3 + i]); }
  AssertIsEqual(0x00, buf[11]); // Body length 0.
  //AssertIsEqual(0xXX, buf[12]); // CRC will vary with ID.
  //
  //const unsigned long before = millis();
  for(int idLen = 0; idLen <= 8; ++idLen)
    {
    // Secure beacon...  All zeros key; ID and IV as from spec Example 3 at 20160207.
    const uint8_t *const key = zeroBlock;
    // Preshared ID prefix; only an initial part/prefix of this goes on the wire in the header.
    const uint8_t id[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55 };
    // IV/nonce starting with first 6 bytes of preshared ID, then 6 bytes of counter.
    const uint8_t iv[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x00, 0x00, 0x2a, 0x00, 0x03, 0x19 };
//    const uint8_t sb1 = OTRadioLink::SimpleSecureFrame32or0BodyTXBase::generateSecureBeaconRaw(buf, sizeof(buf), id, idLen, iv, OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_STATELESS, NULL, key);
    const uint8_t sb1 = OTRadioLink::SimpleSecureFrame32or0BodyTXBase::encodeSecureSmallFrameRaw(buf, sizeof(buf),
                                    OTRadioLink::FTS_ALIVE, id, idLen,
                                    NULL, 0,
                                    iv, OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleEnc_DEFAULT_STATELESS, NULL, key);
    AssertIsEqual(27 + idLen, sb1);
    //
    // Check decoding (auth/decrypt) of beacon at various levels.
    // Validate structure of frame first.
    // This is quick and checks for insane/dangerous values throughout.
    OTRadioLink::SecurableFrameHeader sfh;
    const uint8_t l = sfh.checkAndDecodeSmallFrameHeader(buf, sb1);
    AssertIsEqual(4 + idLen, l);
    uint8_t decryptedBodyOutSize;
    const uint8_t dlr = OTRadioLink::SimpleSecureFrame32or0BodyRXBase::decodeSecureSmallFrameRaw(&sfh,
                                    buf, sizeof(buf),
                                    OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                                    NULL, key, iv,
                                    NULL, 0, decryptedBodyOutSize);
    // Should be able to decode, ie pass authentication.
    AssertIsEqual(27 + idLen, dlr);
    // Construct IV from ID and trailer.
    const uint8_t dlfi = OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance()._decodeSecureSmallFrameFromID(&sfh,
                                    buf, sizeof(buf),
                                    OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                                    id, sizeof(id),
                                    NULL, key,
                                    NULL, 0, decryptedBodyOutSize);
    // Should be able to decode, ie pass authentication.
    AssertIsEqual(27 + idLen, dlfi);         
    }
//  const unsigned long after = millis();
//  Serial.println(after - before); // DHD20160207: 1442 for 8 rounds, or ~180ms per encryption.
  }

// Test low-wear unary encoding.
static void testUnaryEncoding()
  {
  Serial.println("UnaryEncoding");
  // Check for conversion back and forth of all allowed represented values.
  for(uint8_t i = 0; i <= 8; ++i) { AssertIsEqual(i, OTV0P2BASE::eeprom_unary_1byte_decode(OTV0P2BASE::eeprom_unary_1byte_encode(i))); }
  for(uint8_t i = 0; i <= 16; ++i) { AssertIsEqual(i, OTV0P2BASE::eeprom_unary_2byte_decode(OTV0P2BASE::eeprom_unary_2byte_encode(i))); }
  // Ensure that all-1s (erased) EEPROM values equate to 0.
  AssertIsEqual(0, OTV0P2BASE::eeprom_unary_1byte_decode(0xff));
  AssertIsEqual(0, OTV0P2BASE::eeprom_unary_1byte_decode(0xffffU));
  // Check rejection of at least one bad value.
  AssertIsEqual(-1, OTV0P2BASE::eeprom_unary_1byte_decode(0xef));
  AssertIsEqual(-1, OTV0P2BASE::eeprom_unary_2byte_decode(0xccccU));
  }

// Test some basic parameters of node associations.
// Does not wear non-volatile memory (eg EEPROM).
static void testNodeAssoc()
  {
  Serial.println("NodeAssoc");
  const uint8_t nas = OTV0P2BASE::countNodeAssociations();
  AssertIsTrue(nas <= OTV0P2BASE::MAX_NODE_ASSOCIATIONS);
  const int8_t i = OTV0P2BASE::getNextMatchingNodeID(0, NULL, 0, NULL);
  // Zero-length prefix look-up should succeed IFF there is at least one entry.
  AssertIsTrue((0 == nas) == (-1 == i));
  }

// Test some message counter routines.
// Does not wear non-volatile memory (eg EEPROM).
static void testMsgCount()
  {
  Serial.println("MsgCount");
  // Two counter values to compare that should help spot overflow or wrong byte order operations.
  const uint8_t count1[] = { 0, 0, 0x83, 0, 0, 0 };
  const uint8_t count1plus1[] = { 0, 0, 0x83, 0, 0, 1 };
  const uint8_t count1plus256[] = { 0, 0, 0x83, 0, 1, 0 };
  const uint8_t count2[] = { 0, 0, 0x82, 0x88, 1, 1 };
  const uint8_t countmax[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  // Check that identical values compare as identical.
  AssertIsEqual(0, OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcountercmp(zeroBlock, zeroBlock));
  AssertIsEqual(0, OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcountercmp(count1, count1));
  AssertIsEqual(0, OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcountercmp(count2, count2));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcountercmp(count1, count2) > 0);
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcountercmp(count2, count1) < 0);
  // Test simple addition to counts.
  uint8_t count1copy[sizeof(count1)];
  memcpy(count1copy, count1, sizeof(count1copy));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcounteradd(count1copy, 0));
  AssertIsEqual(0, OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcountercmp(count1copy, count1));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcounteradd(count1copy, 1));
  AssertIsEqual(0, OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcountercmp(count1copy, count1plus1));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcounteradd(count1copy, 255));
  AssertIsEqual(0, OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcountercmp(count1copy, count1plus256));
  // Test simple addition to counts.
  uint8_t countmaxcopy[sizeof(countmax)];
  memcpy(countmaxcopy, countmax, sizeof(countmaxcopy));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcounteradd(countmaxcopy, 0));
  AssertIsEqual(0, OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcountercmp(countmaxcopy, countmax));
  AssertIsTrue(!OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcounteradd(countmaxcopy, 1));
  AssertIsEqual(0, OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcountercmp(countmaxcopy, countmax));
  AssertIsTrue(!OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcounteradd(countmaxcopy, 42));
  AssertIsEqual(0, OTRadioLink::SimpleSecureFrame32or0BodyBase::msgcountercmp(countmaxcopy, countmax));
  }

// Test handling of persistent/reboot/restart part of primary message counter.
// Does not wear non-volatile memory (eg EEPROM).
static void testPermMsgCount()
  {
  Serial.println("PermMsgCount");
  uint8_t loadBuf[OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR];
  uint8_t buf[OTRadioLink::SimpleSecureFrame32or0BodyTXBase::primaryPeristentTXMessageRestartCounterBytes];
  // Initialise to state of empty EEPROM; result should be a valid all-zeros restart count.
  memset(loadBuf, 0, sizeof(loadBuf));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyTXV0p2::read3BytePersistentTXRestartCounter(loadBuf, buf));
  AssertIsEqual(0, memcmp(buf, zeroBlock, OTRadioLink::SimpleSecureFrame32or0BodyTXBase::primaryPeristentTXMessageRestartCounterBytes));
  // Ensure that it can be incremented and gives the correct next (0x000001) value.
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyTXV0p2::increment3BytePersistentTXRestartCounter(loadBuf));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyTXV0p2::read3BytePersistentTXRestartCounter(loadBuf, buf));
  AssertIsEqual(0, memcmp(buf, zeroBlock, OTRadioLink::SimpleSecureFrame32or0BodyTXBase::primaryPeristentTXMessageRestartCounterBytes - 1));
  AssertIsEqual(1, buf[2]);
  // Initialise to all-0xff state (with correct CRC), which should cause failure.
  memset(loadBuf, 0xff, sizeof(loadBuf)); loadBuf[3] = 0xf; loadBuf[7] = 0xf;
  AssertIsTrue(!OTRadioLink::SimpleSecureFrame32or0BodyTXV0p2::read3BytePersistentTXRestartCounter(loadBuf, buf));
  // Ensure that it CANNOT be incremented.
  AssertIsTrue(!OTRadioLink::SimpleSecureFrame32or0BodyTXV0p2::increment3BytePersistentTXRestartCounter(loadBuf));
  // Test recovery from broken primary counter a few times.
  memset(loadBuf, 0, sizeof(loadBuf));
  for(uint8_t i = 0; i < 3; ++i)
    {
    // Damage one of the primary or secondary counter bytes (or CRCs).
    loadBuf[0x7 & OTV0P2BASE::randRNG8()] = OTV0P2BASE::randRNG8();
    AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyTXV0p2::getInstance().read3BytePersistentTXRestartCounter(loadBuf, buf));
    AssertIsEqual(0, buf[1]);
    AssertIsEqual(0, buf[1]);
    AssertIsEqual(i, buf[2]);
    AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyTXV0p2::getInstance().increment3BytePersistentTXRestartCounter(loadBuf));
    }
  // Also verify in passing that all zero message counter will never be acceptable for an RX message,
  // regardless of the node ID, since the new count as to be higher than any previous for the ID.
  AssertIsTrue(!OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().validateRXMessageCount(zeroBlock, zeroBlock));
  }

// Test handling of persistent/reboot/restart part of primary TX message counter.
// Tests to only be run once per restart because they cause device wear (EEPROM).
// NOTE: best not to run on a live device as this will mess with its associations, etc.
// This clears the key so as to make it clear that this device is not to be regarded as secure.
static void testPermMsgCountRunOnce()
  {
  Serial.println("PermMsgCountRunOnce");
  // We're compromising system security and keys here, so clear any secret keys set, first.
  AssertIsTrue(OTV0P2BASE::setPrimaryBuilding16ByteSecretKey(NULL)); // Fail if we can't ensure that the key is cleared.
  // Check that key is correctly erased.
  uint8_t tmpKeyBuf[16];
  AssertIsTrue(!OTV0P2BASE::getPrimaryBuilding16ByteSecretKey(tmpKeyBuf));
  memset(tmpKeyBuf, 0, sizeof(tmpKeyBuf));
  // Check that we don't get a spurious key match.
  AssertIsTrue(!OTV0P2BASE::checkPrimaryBuilding16ByteSecretKey(tmpKeyBuf));
  OTRadioLink::SimpleSecureFrame32or0BodyTXV0p2 instance = OTRadioLink::SimpleSecureFrame32or0BodyTXV0p2::getInstance();
  // Working buffer space...
  uint8_t loadBuf[OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR];
  uint8_t buf[OTRadioLink::SimpleSecureFrame32or0BodyTXBase::primaryPeristentTXMessageRestartCounterBytes];
  // Initial test that blank EEPROM (or reset to all zeros) after processing yields all zeros.
  AssertIsTrue(instance.resetRaw3BytePersistentTXRestartCounterInEEPROM(true));
  instance.loadRaw3BytePersistentTXRestartCounterFromEEPROM(loadBuf);
  AssertIsTrue(0 == memcmp(loadBuf, zeroBlock, sizeof(loadBuf)));
  AssertIsTrue(instance.read3BytePersistentTXRestartCounter(loadBuf, buf));
  AssertIsEqual(0, memcmp(buf, zeroBlock, OTRadioLink::SimpleSecureFrame32or0BodyTXBase::primaryPeristentTXMessageRestartCounterBytes));
  AssertIsTrue(instance.get3BytePersistentTXRestartCounter(buf));
  AssertIsTrue(0 == memcmp(buf, zeroBlock, OTRadioLink::SimpleSecureFrame32or0BodyTXBase::primaryPeristentTXMessageRestartCounterBytes));
  // Increment the persistent TX counter and ensure that we see it as non-zero.
  AssertIsTrue(instance.increment3BytePersistentTXRestartCounter());
  AssertIsTrue(instance.get3BytePersistentTXRestartCounter(buf));
  AssertIsTrue(0 != memcmp(buf, zeroBlock, OTRadioLink::SimpleSecureFrame32or0BodyTXBase::primaryPeristentTXMessageRestartCounterBytes));
  // So reset to non-all-zeros (should be default) and make sure that we see it as not-all-zeros.
  AssertIsTrue(instance.resetRaw3BytePersistentTXRestartCounterInEEPROM());
  AssertIsTrue(instance.get3BytePersistentTXRestartCounter(buf));
  AssertIsTrue(0 != memcmp(buf, zeroBlock, OTRadioLink::SimpleSecureFrame32or0BodyTXBase::primaryPeristentTXMessageRestartCounterBytes));
  // Initial test that from blank EEPROM (or reset to all zeros)
  // that getting the message counter gives non-zero reboot and ephemeral parts.
  AssertIsTrue(instance.resetRaw3BytePersistentTXRestartCounterInEEPROM(true));
  uint8_t mcbuf[OTRadioLink::SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes];
  AssertIsTrue(instance.incrementAndGetPrimarySecure6BytePersistentTXMessageCounter(mcbuf));
#if 1
  for(int i = 0; i < sizeof(mcbuf); ++i) { Serial.print(' '); Serial.print(mcbuf[i], HEX); }
  Serial.println();
#endif
  // Assert that each half of the message counter is non-zero.
  AssertIsTrue((0 != mcbuf[0]) || (0 != mcbuf[1]) || (0 != mcbuf[2]));
  AssertIsTrue((0 != mcbuf[3]) || (0 != mcbuf[4]) || (0 != mcbuf[5]));
  }

// Test some basic parameters of node associations.
// Also tests intimate interaction with management of RX message counters.
// Tests to only be run once per restart because they cause device wear (EEPROM).
// NOTE: best not to run on a live device as this will mess with its associations, etc.
static void testNodeAssocRunOnce()
  {
  Serial.println("NodeAssocRunOnce");
  OTV0P2BASE::clearAllNodeAssociations();
  AssertIsEqual(0, OTV0P2BASE::countNodeAssociations());
  AssertIsEqual(-1, OTV0P2BASE::getNextMatchingNodeID(0, NULL, 0, NULL));
  // Check that attempting to get aux data for a non-existent node/association fails.
  uint8_t mcbuf[OTRadioLink::SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes];
  AssertIsTrue(!OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(zeroBlock, mcbuf));
  // Test adding associations and looking them up.
  const uint8_t ID0[] = { 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7 };
  AssertIsEqual(0, OTV0P2BASE::addNodeAssociation(ID0));
  AssertIsEqual(1, OTV0P2BASE::countNodeAssociations());
  AssertIsEqual(0, OTV0P2BASE::getNextMatchingNodeID(0, NULL, 0, NULL));
  for(uint8_t i = 0; i <= sizeof(ID0); ++i)
    { AssertIsEqual(0, OTV0P2BASE::getNextMatchingNodeID(0, ID0, i, NULL)); }
  // Check that RX msg count for new association is OK, and all zeros.
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(ID0, mcbuf));
  AssertIsEqual(0, memcmp(mcbuf, zeroBlock, sizeof(mcbuf)));
  // Now deliberately damage the primary count and check that the counter value can still be retrieved.
  // Ensure damanged byte has at least one 1 and one 0 and is at a random position towards the end.
  OTV0P2BASE::eeprom_smart_update_byte((uint8_t *)(OTV0P2BASE::V0P2BASE_EE_START_NODE_ASSOCIATIONS + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_0_OFFSET + 2 + (3 & OTV0P2BASE::randRNG8())), 0x40 | (0x3f & OTV0P2BASE::randRNG8()));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(ID0, mcbuf));
  AssertIsEqual(0, memcmp(mcbuf, zeroBlock, sizeof(mcbuf)));
  // Attempting to update to the same (0) value should fail.
  AssertIsTrue(!OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().updateRXMessageCountAfterAuthentication(ID0, zeroBlock));
  // Updating to a new count and reading back should work.
  const uint8_t newCount[] = { 0, 1, 2, 3, 4, 5 };
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().updateRXMessageCountAfterAuthentication(ID0, newCount));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(ID0, mcbuf));
  AssertIsEqual(0, memcmp(mcbuf, newCount, sizeof(mcbuf)));
  // Attempting to update to a lower (0) value should fail.
  AssertIsTrue(!OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().updateRXMessageCountAfterAuthentication(ID0, zeroBlock));
  // Add/test second association...
  const uint8_t ID1[] = { 0x88, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0x8f };
  AssertIsEqual(1, OTV0P2BASE::addNodeAssociation(ID1));
  AssertIsEqual(2, OTV0P2BASE::countNodeAssociations());
  // Zero-length-lookup matches any entry.
  AssertIsEqual(0, OTV0P2BASE::getNextMatchingNodeID(0, NULL, 0, NULL));
  AssertIsEqual(1, OTV0P2BASE::getNextMatchingNodeID(1, NULL, 0, NULL));
  for(uint8_t i = 1; i <= sizeof(ID1); ++i)
    { AssertIsEqual(1, OTV0P2BASE::getNextMatchingNodeID(0, ID1, i, NULL)); }
  for(uint8_t i = 1; i <= sizeof(ID1); ++i)
    { AssertIsEqual(1, OTV0P2BASE::getNextMatchingNodeID(1, ID1, i, NULL)); }
  // Test that first ID cannot be matched from after its index in the table.
  for(uint8_t i = 1; i <= sizeof(ID0); ++i)
    { AssertIsEqual(0, OTV0P2BASE::getNextMatchingNodeID(0, ID0, i, NULL)); }
  for(uint8_t i = 1; i <= sizeof(ID0); ++i)
    { AssertIsEqual(-1, OTV0P2BASE::getNextMatchingNodeID(1, ID0, i, NULL)); }
  // Updating (ID0) to a new (up-by-one) count and reading back should work.
  const uint8_t newCount2[] = { 0, 1, 2, 3, 4, 6 };
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().updateRXMessageCountAfterAuthentication(ID0, newCount2));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(ID0, mcbuf));
  AssertIsEqual(0, memcmp(mcbuf, newCount2, sizeof(mcbuf)));
  // Updating to a new (up-by-slightly-more-than-one) count and reading back should work.
  const uint8_t newCount3[] = { 0, 1, 2, 3, 4, 9 };
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().updateRXMessageCountAfterAuthentication(ID0, newCount3));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(ID0, mcbuf));
  AssertIsEqual(0, memcmp(mcbuf, newCount3, sizeof(mcbuf)));
  // Updating to a new (up-by-much-more-than-one) count and reading back should work.
  const uint8_t newCount4[] = { 0, 1, 2, 3, 4, 99 };
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().updateRXMessageCountAfterAuthentication(ID0, newCount4));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(ID0, mcbuf));
  AssertIsEqual(0, memcmp(mcbuf, newCount4, sizeof(mcbuf)));
  // Updating to a new (up-by-much-much-more-than-one) count and reading back should work.
  const uint8_t newCount5[] = { 0, 1, 0x99, 1, 0x81, (OTV0P2BASE::randRNG8() & 0x7f) };
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().updateRXMessageCountAfterAuthentication(ID0, newCount5));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(ID0, mcbuf));
  AssertIsEqual(0, memcmp(mcbuf, newCount5, sizeof(mcbuf)));
  // Updating to a new (up-by-one) count and reading back should work.
  const uint8_t newCount6[] = { 0, 1, 0x99, 1, 0x81, 1+newCount5[5] };
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().updateRXMessageCountAfterAuthentication(ID0, newCount6));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(ID0, mcbuf));
  AssertIsEqual(0, memcmp(mcbuf, newCount6, sizeof(mcbuf)));
  // Updating to a new (up-by-one) count and reading back should work.
  const uint8_t newCount7[] = { 0, 1, 0x99, 1, 0x81, 2+newCount5[5] };
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().updateRXMessageCountAfterAuthentication(ID0, newCount7));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(ID0, mcbuf));
  AssertIsEqual(0, memcmp(mcbuf, newCount7, sizeof(mcbuf)));
  // Updating to a new (up-by-one) count and reading back should work.
  const uint8_t newCount8[] = { 0, 1, 0x99, 1, 0x81, 3+newCount5[5] };
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().updateRXMessageCountAfterAuthentication(ID0, newCount8));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(ID0, mcbuf));
  AssertIsEqual(0, memcmp(mcbuf, newCount8, sizeof(mcbuf)));
  // Updating to a new (up-by-one) count and reading back should work.
  const uint8_t newCount9[] = { 0, 1, 0x99, 1, 0x81, 4+newCount5[5] };
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().updateRXMessageCountAfterAuthentication(ID0, newCount9));
  AssertIsTrue(OTRadioLink::SimpleSecureFrame32or0BodyRXV0p2::getInstance().getLastRXMessageCounter(ID0, mcbuf));
  AssertIsEqual(0, memcmp(mcbuf, newCount9, sizeof(mcbuf)));
  }


// To be called from loop() instead of main code when running unit tests.
// Tests generally flag an error and stop the test cycle with a call to panic() or error().
void loop()
  {
  static int loopCount = 0;

  // Allow the terminal console to be brought up.
  for(int i = 3; i > 0; --i)
    {
    Serial.print(F("Tests starting... "));
    Serial.print(i);
    Serial.println();
    delay(1000);
    }
  Serial.println();


  // Run the tests, fastest / newest / most-fragile / most-interesting first...
  testLibVersion();
  testLibVersions();

  testUnaryEncoding();
  testFrameQIC();
  testFrameHeaderEncoding();
  testFrameHeaderDecoding();
  testNonsecureFrameCRC();
  testNonsecureSmallFrameEncoding();
  testMsgCount();
  testPermMsgCount();
  testSimplePadding();
  testSimpleNULLEncDec();
  testCryptoAccess();
  testGCMVS1ViaFixed32BTextSize();
  testSecureSmallFrameEncoding();
  testBeaconEncoding();
  testNodeAssoc();

  // Run-once tests.
  // May cause wear on (eg) EEPROM, so only run once,
  // and only after all other tests have passed.
  static bool runOnce;
  if(!runOnce)
    {
    runOnce = true;
    Serial.println(F("RUN-ONCE tests... "));
    testPermMsgCountRunOnce();
    testNodeAssocRunOnce();
    }


  // Announce successful loop completion and count.
  ++loopCount;
  Serial.println();
  Serial.print(F("%%% All tests completed OK, round "));
  Serial.print(loopCount);
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.flush();
  delay(2000);
  }
