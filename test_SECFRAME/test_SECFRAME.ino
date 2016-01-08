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

Author(s) / Copyright (s): Damon Hart-Davis 2015
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
#include <OTRadioLink.h>
// Also testing against crypto.
#include <OTAESGCM.h>

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
#if !(0 == ARDUINO_LIB_OTRADIOLINK_VERSION_MAJOR) || !(9 == ARDUINO_LIB_OTRADIOLINK_VERSION_MINOR)
#error Wrong library version!
#endif
  }

// Check that correct versions of underlying libraries are in use.
static void testLibVersions()
  {
  Serial.println("LibVersions");
#if !(0 == ARDUINO_LIB_OTV0P2BASE_VERSION_MAJOR) || !(9 == ARDUINO_LIB_OTV0P2BASE_VERSION_MINOR)
#error Wrong library version!
#endif

#if !(0 == ARDUINO_LIB_OTAESGCM_VERSION_MAJOR) || !(2 > ARDUINO_LIB_OTAESGCM_VERSION_MINOR)
#error Wrong library version!
#endif
  }



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
                                               OTRadioLink::SecurableFrameHeader::maxIDLength + 1, id,
                                               2,
                                               1));
  // Should fail with bad buffer length.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, 0,
                                               false, OTRadioLink::FTS_BasicSensorOrValve,
                                               OTV0P2BASE::randRNG8(),
                                               2, id,
                                               2,
                                               1));
  // Should fail with bad frame type.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_NONE,
                                               OTV0P2BASE::randRNG8(),
                                               2, id,
                                               2,
                                               1));
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_INVALID_HIGH,
                                               OTV0P2BASE::randRNG8(),
                                               2, id,
                                               2,
                                               1));
  // Should fail with impossible body length.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               1, id,
                                               252,
                                               1));
  // Should fail with impossible trailer length.
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               1, id,
                                               0,
                                               0));
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               1, id,
                                               0,
                                               252));
  // Should fail with impossible body + trailer length (for small frame).
  AssertIsEqual(0, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               OTV0P2BASE::randRNG8NextBoolean(), OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               1, id,
                                               32,
                                               32));
  // "I'm Alive!" message with 1-byte ID should succeed and be of full header length (5).
  AssertIsEqual(5, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               1, id, // Minimal (non-empty) ID.
                                               0, // No payload.
                                               1));
  // Large but legal body size.
  AssertIsEqual(5, sfh.checkAndEncodeSmallFrameHeader(buf, sizeof(buf),
                                               false, OTRadioLink::FTS_ALIVE,
                                               OTV0P2BASE::randRNG8(),
                                               1, id, // Minimal (non-empty) ID.
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
                                               2, id,
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
                                               2, id,
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
  const uint8_t buf2[] = { 0x0e, 0x4f, 0x02, 0x80, 0x81, 0x08, 0x7f, 0x11, 0x7b, 0x22, 0x62, 0x22, 0x3a, 0x31, 0x61 };
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
static void testInsecureFrameCRC()
  {
  Serial.println("InsecureFrameCRC");
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
  const uint8_t buf1[] = { 0x08, 0x4f, 0x02, 0x80, 0x81, 0x02, 0x00, 0x01 }; //, 0x23 };
  AssertIsEqual(6, sfh.checkAndDecodeSmallFrameHeader(buf1, sizeof(buf1)));
  AssertIsEqual(0x23, sfh.computeNonSecureFrameCRC(buf1, sizeof(buf1)));
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
  const uint8_t buf2[] = { 0x0e, 0x4f, 0x02, 0x80, 0x81, 0x08, 0x7f, 0x11, 0x7b, 0x22, 0x62, 0x22, 0x3a, 0x31 }; // , 0x61 };
  AssertIsEqual(6, sfh.checkAndDecodeSmallFrameHeader(buf2, sizeof(buf2)));
  AssertIsEqual(0x61, sfh.computeNonSecureFrameCRC(buf2, sizeof(buf2)));
  }


// Test simple plain-text padding for encryption.
static void testSimplePadding()
  {
  Serial.println("SimplePadding");
  uint8_t buf[OTRadioLink::ENC_BODY_SMALL_FIXED_CTEXT_SIZE];
  // Provoke failure with NULL buffer.
  AssertIsEqual(0, OTRadioLink::addPaddingTo32BTrailing0sAndPadCount(NULL, 0x1f & OTV0P2BASE::randRNG8()));
  // Provoke failure with over-long unpadded plain-text.
  AssertIsEqual(0, OTRadioLink::addPaddingTo32BTrailing0sAndPadCount(buf, 1 + OTRadioLink::ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE));  
  // Check padding in case with single random data byte (and the rest of the buffer set differently).
  // Check the entire padded result for correctness.
  const uint8_t db0 = OTV0P2BASE::randRNG8();
  buf[0] = db0;
  memset(buf+1, ~db0, sizeof(buf)-1);
  AssertIsEqual(32, OTRadioLink::addPaddingTo32BTrailing0sAndPadCount(buf, 1));
  AssertIsEqual(db0, buf[0]);
  for(int i = 30; --i > 0; ) { AssertIsEqual(0, buf[i]); }
  AssertIsEqual(30, buf[31]);
  // Ensure that unpadding works.
  AssertIsEqual(1, OTRadioLink::removePaddingTo32BTrailing0sAndPadCount(buf));
  AssertIsEqual(db0, buf[0]);
  }

// Signature of basic fixed-size text encryption/authentication function
// (suitable for type 'O' valve/sensor small frame for example)
// that can be fulfilled by AES-128-GCM for example
// where:
//   * textSize is 32
//   * keySize is 16
//   * nonceSize is 12
//   * tagSize is 16
// The plain-text (and identical cipher-text) size is picked to be
// a multiple of the cipher's block size,
// which implies likely requirement for padding of the plain text.
// Note that the authenticated text size is not fixed, ie is zero or more bytes.
// Returns true on success, false on failure.
template <uint8_t textSize, uint8_t keySize, uint8_t nonceSize, uint8_t tagSize>
    bool fixedTextSizeSimpleEnc(void *state,
        const uint8_t *key, const uint8_t *nonce,
        const uint8_t *authtext, uint8_t authtextSize,
        const uint8_t *plaintext,
        uint8_t *ciphertextOut, uint8_t *tagOut);

// Test basic access to crypto features.
static void testCryptoAccess()
  {
  Serial.println("CryptoAccess");
  // Check NULL enc routine is correct type.
  const OTRadioLink::fixedTextSizeSimpleEnc_ptr_t nep = OTRadioLink::fixedTextSizeSimpleEnc_NULL_IMPL;
  // Check that calling the NULL enc routine with bad args fails.
  AssertIsTrue(!nep(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
  }


// TODO: test with EEPROM ID source (id_ == NULL) ...
// TODO: add EEPROM prefill static routine and pad 1st trailing byte with 0xff.


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

  testFrameQIC();
  testFrameHeaderEncoding();
  testFrameHeaderDecoding();
  testInsecureFrameCRC();
  testSimplePadding();
  testCryptoAccess();



  // Announce successful loop completion and count.
  ++loopCount;
  Serial.println();
  Serial.print(F("%%% All tests completed OK, round "));
  Serial.print(loopCount);
  Serial.println();
  Serial.println();
  Serial.println();
  delay(2000);
  }
