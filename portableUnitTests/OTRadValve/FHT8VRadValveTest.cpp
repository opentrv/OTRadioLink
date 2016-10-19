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
 * OTRadValve FHTRadValve tests.
 *
 * Partial, since interactions with hardware (eg radio) hard to portably test.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>

#include "OTRadValve_FHT8VRadValve.h"


// Test (fast) mappings back and forth between [0,100] valve open percentage and [0,255] FS20 representation.
//
// Adapted 2016/10/16 from test_VALVEMODEL.ino testFHT8VPercentage().
TEST(FHT8VRadValve,FHT8VPercentage)
{
    // Test that end-points are mapped correctly from % -> FS20 scale.
    ASSERT_EQ(0, OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(0));
    ASSERT_EQ(255, OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(100));
    // Test that illegal/over values handled sensibly.
    ASSERT_EQ(255, OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(101));
    ASSERT_EQ(255, OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(255));
    // Test that end-points are mapped correctly from FS20 scale to %.
    ASSERT_EQ(0, OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(0));
    ASSERT_EQ(100, OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(255));
    // Test that some critical thresholds are mapped back and forth (round-trip) correctly.
    ASSERT_EQ(OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN, OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN)));
    ASSERT_EQ(OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN, OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN)));
    // Check that all mappings back and forth (round-trip) are reasonably close to target.
    const uint8_t eps = 2; // Tolerance in %
    for(uint8_t i = 0; i <= 100; ++i)
      {
      ASSERT_NEAR(i, OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i)), eps);
      }
    // Check for monotonicity of mapping % -> FS20.
    for(uint8_t i = 0; i < 100; ++i)
      {
      ASSERT_TRUE(
        OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i) <=
        OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i+1));
      }
    // Check for monotonicity of mapping FS20 => %.
    for(uint8_t i = 0; i < 100; ++i)
      {
      ASSERT_TRUE(
        OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i)) <=
        OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i+1)));
      }
    // Check for monotonicity of round-trip.
    for(uint8_t i = 0; i < 100; ++i)
      {
      ASSERT_TRUE(
        OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i) <=
        OTRadValve::FHT8VRadValveUtil::convertPercentTo255Scale(i+1));
      }
}


// Test of FHT8VRadValveUtil::xor_parity_even_bit().
TEST(FHT8VRadValve,xor_parity_even_bit)
{
    EXPECT_EQ(0, OTRadValve::FHT8VRadValveUtil::xor_parity_even_bit(0x00));
    EXPECT_EQ(1, OTRadValve::FHT8VRadValveUtil::xor_parity_even_bit(0x0d));
    EXPECT_EQ(1, OTRadValve::FHT8VRadValveUtil::xor_parity_even_bit(0x49));
    EXPECT_EQ(1, OTRadValve::FHT8VRadValveUtil::xor_parity_even_bit(0x38));
    EXPECT_EQ(0, OTRadValve::FHT8VRadValveUtil::xor_parity_even_bit(0x88));
}

// Test of FHT8VRadValveUtil::_FHT8VCreate200usAppendEncBit().
TEST(FHT8VRadValve,FHT8VCreate200usAppendEncBit)
{
    uint8_t buf[4];

    *buf = 0xff; // Mark buffer as empty.
    // Write a 0.
    ASSERT_EQ(buf, OTRadValve::FHT8VRadValveUtil::_FHT8VCreate200usAppendEncBit(buf, false));
    EXPECT_EQ(0b11000000, 0xf0 & *buf);

    *buf = 0xff; // Mark buffer as empty.
    // Write a 1.
    ASSERT_EQ(buf, OTRadValve::FHT8VRadValveUtil::_FHT8VCreate200usAppendEncBit(buf, true));
    EXPECT_EQ(0b11100000, 0xfc & *buf);

    *buf = 0xff; // Mark buffer as empty.
    // Write 1, 0, 1, 0.
    // The 1st byte (offset 0) starts with 111000 (from 1 bit)
    // and the start of the next encoded 0 (11),
    // The 2nd byte (offset 1) starts with trailing bits from before (00)
    // and the next encoded 1 (111000, from parity),
    // ie 0x38.
    // The third byte is empty until the final 0 is written to it,
    // and then starts with 1100.
    ASSERT_EQ(buf, OTRadValve::FHT8VRadValveUtil::_FHT8VCreate200usAppendEncBit(buf, true));
    ASSERT_EQ(buf+1, OTRadValve::FHT8VRadValveUtil::_FHT8VCreate200usAppendEncBit(buf, false));
    EXPECT_EQ(0b11100011, buf[0]);
    ASSERT_EQ(buf+2, OTRadValve::FHT8VRadValveUtil::_FHT8VCreate200usAppendEncBit(buf+1, true));
    EXPECT_EQ(0b00111000, buf[1]);
    EXPECT_EQ(0b11111111, buf[2]);
    ASSERT_EQ(buf+2, OTRadValve::FHT8VRadValveUtil::_FHT8VCreate200usAppendEncBit(buf+2, false));
    EXPECT_EQ(0b11000000, 0xf0 & buf[2]);
}

// Test of FHT8V bitstream encoding and decoding.
//
// Adapted 2016/10/19 from Unit_Tests.cpp testFHTEncoding().
TEST(FHT8VRadValve,FHTEncoding)
{
    uint8_t buf[OTRadValve::FHT8VRadValveUtil::MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE];
    OTRadValve::FHT8VRadValveUtil::fht8v_msg_t command; // For encoding.
    OTRadValve::FHT8VRadValveUtil::fht8v_msg_t commandDecoded; // For decoding.

    // Encode shortest-possible (all-zero-bits) FHT8V command as 200us-bit-stream...
    command.hc1 = 0;
    command.hc2 = 0;
#ifdef OTV0P2BASE_FHT8V_ADR_USED
    address = 0;
#endif
    command.command = 0;
    command.extension = 0;
    memset(buf, 0, sizeof(buf));

    *buf = 0xff; // Mark buffer as empty.
    const uint8_t *const result1 = OTRadValve::FHT8VRadValveUtil::FHT8VCreate200usBitStreamBptr(buf, &command);
    EXPECT_EQ(((uint8_t)~0U), *result1); // Check that result points at terminator value 0xff/~0.
    //AssertIsTrue((result1 - buf < MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE), result1-buf); // Check not overflowing the buffer.
    EXPECT_EQ(result1 - buf, 35); // Check correct length.
    EXPECT_EQ((uint8_t)0xcc, buf[0]); // Check that result starts with FHT8V 0xcc preamble.
    // Attempt to decode.
    EXPECT_TRUE(OTRadValve::FHT8VRadValveUtil::FHT8VDecodeBitStream(buf, buf + sizeof(buf) - 1, &commandDecoded));
    EXPECT_EQ(0, commandDecoded.hc1);
    EXPECT_EQ(0, commandDecoded.hc2);
    EXPECT_EQ(0, commandDecoded.command);
    EXPECT_EQ(0, commandDecoded.extension);

    // Encode longest-possible (as many 1-bits as possible) FHT8V command as 200us-bit-stream...
    command.hc1 = 0xff;
    command.hc2 = 0xff;
#ifdef OTV0P2BASE_FHT8V_ADR_USED
    address = 0xff;
#endif
    command.command = 0xff;
    command.extension = 0xff;
    memset(buf, 0, sizeof(buf));

    *buf = 0xff; // Mark buffer as empty.
    const uint8_t *const result2 = OTRadValve::FHT8VRadValveUtil::FHT8VCreate200usBitStreamBptr(buf, &command);
    EXPECT_EQ(((uint8_t)~0U), *result2); // Check that result points at terminator value 0xff/~0.
    const size_t minbufsize = OTRadValve::FHT8VRadValveUtil::MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE;
    ASSERT_LT(result2 - buf, minbufsize); // Check not overflowing the (minimum-sized) buffer.
    EXPECT_EQ(result2 - buf, 43); // Check for expected length.
    EXPECT_EQ((uint8_t)0xcc, buf[0]); // Check that result starts with FHT8V 0xcc preamble.
    // Attempt to decode.
    EXPECT_TRUE(OTRadValve::FHT8VRadValveUtil::FHT8VDecodeBitStream(buf, buf + sizeof(buf) - 1, &commandDecoded));
    EXPECT_EQ(0xff, commandDecoded.hc1);
    EXPECT_EQ(0xff, commandDecoded.hc2);
  #ifdef OTV0P2BASE_FHT8V_ADR_USED
    AssertIsTrueWithErr(0xff == commandDecoded.address, commandDecoded.address);
  #endif
    EXPECT_EQ(0xff, commandDecoded.command);
    EXPECT_EQ(0xff, commandDecoded.extension);
}

// Test of head and tail of FHT8V bitstream encoding and decoding.
//
// Adapted 2016/10/18 from Unit_Tests.cpp testFHTEncodingHeadAndTail().
TEST(FHT8VRadValve,FHTEncodingHeadAndTail)
{
    //// Create FHT8V TRV outgoing valve-setting command frame (terminated with 0xff) at bptr with optional headers and trailers.
    ////   * TRVPercentOpen value is used to generate the frame
    ////   * doHeader  if true then an extra RFM22/23-friendly 0xaaaaaaaa sync header is preprended
    ////   * trailer  if not null then a (3-byte) trailer is appended, build from that info plus a CRC
    ////   * command  on entry hc1, hc2 (and address if used) must be set correctly, this sets the command and extension; never NULL
    //// The generated command frame can be resent indefinitely.
    //// The output buffer used must be (at least) FHT8V_200US_BIT_STREAM_FRAME_BUF_SIZE bytes.
    //// Returns pointer to the terminating 0xff on exit.
    //uint8_t *FHT8VCreateValveSetCmdFrameHT_r(uint8_t *const bptrInitial, const bool doHeader, fht8v_msg_t *const command, const uint8_t TRVPercentOpen, const trailingMinimalStatsPayload_t *trailer)

    uint8_t buf[OTRadValve::FHT8VRadValveUtil::MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE];
    OTRadValve::FHT8VRadValveUtil::fht8v_msg_t command; // For encoding.
    OTRadValve::FHT8VRadValveUtil::fht8v_msg_t commandDecoded; // For decoding.

    // Encode a basic message to set a valve to 0%, without headers or trailers.
    // Before encoding stream is 13, 73, 0, 38, 0 with checksum 136.
    // In hex that is 0d 49 00 26 00 88.
    // PREAMBLE
    // An initial preamble of 12 zero bits and then a single one bit is sent, with the zeros encoded as cc cc cc cc cc cc.
    // HC1
    // The 7th byte (offset 6) in the buffer therefore consists of the final 1 from the preamble (111000)
    // and the start of the encoded form for the leading (msbit, bit 7) 0 from the first byte (11, hc1),
    // ie 0xe3.
    // The 8th byte (offset 7) starts with the trailing bits from before (00)
    // followed by the next encoded 0 (1100, from bit 6)
    // and the start of the next encoded 0 (11, from bit 5),
    // ie 0x33.
    // The 9th byte (offset 8) starts with trailing bits from before (00)
    // followed by the encoded 0 (1100, from bit 4)
    // and the start of the next encoded 1 (11, from bit 3),
    // ie 0x33.
    // The 10th byte (offset 9) starts with trailing bits from before (1000)
    // and the start of the next encoded 1 (1110, from bit 2),
    // ie 0x8e.
    // The 11th byte (offset 10) starts with trailing bits from before (00)
    // and the next encoded 1 (1100, from bit 1),
    // and the start of the next encoded 0 (11, from bit 1),
    // ie 0x33.
    // The 12th byte (offset 11) starts with trailing bits from before (1000)
    // and the start of the next encoded 1 (1110, from parity),
    // ie 0x8e.

    command.hc1 = 13;
    command.hc2 = 73;
#ifdef OTV0P2BASE_FHT8V_ADR_USED
    address = 0;
#endif
    command.command = 0x26;
    command.extension = 0;

    memset(buf, 0, sizeof(buf));

    *buf = 0xff; // Mark buffer as empty.
    uint8_t *result1 = OTRadValve::FHT8VRadValveUtil::FHT8VCreate200usBitStreamBptr(buf, &command);
    EXPECT_EQ(((uint8_t)~0U), *result1); // Check that result points at terminator value 0xff/~0.
    ASSERT_GT(sizeof(buf), result1 - buf); // Check not overflowing the buffer.
    EXPECT_EQ(38, result1 - buf); // Check result is expected length.
    EXPECT_EQ(((uint8_t)0xcc), buf[0]); // Check that result starts with FHT8V 0xcc preamble.
    EXPECT_EQ(((uint8_t)0xcc), buf[1]); // Check that result starts with FHT8V 0xcc preamble.
    EXPECT_EQ(((uint8_t)0xcc), buf[2]); // Check that result starts with FHT8V 0xcc preamble.
    EXPECT_EQ(((uint8_t)0xcc), buf[3]); // Check that result starts with FHT8V 0xcc preamble.
    EXPECT_EQ(((uint8_t)0xcc), buf[4]); // Check that result starts with FHT8V 0xcc preamble.
    EXPECT_EQ(((uint8_t)0xcc), buf[5]); // Check that result starts with FHT8V 0xcc preamble.
    EXPECT_EQ(((uint8_t)0xe3), buf[6]); // Check end of preamble and start of hc1.
    EXPECT_EQ(((uint8_t)0x33), buf[7]); // Check continuing hc1.
    EXPECT_EQ(((uint8_t)0x33), buf[8]); // Check continuing hc1.
    EXPECT_EQ(((uint8_t)0x8e), buf[9]); // Check continuing hc1.
    EXPECT_EQ(((uint8_t)0x33), buf[10]); // Check continuing hc1.
    EXPECT_EQ(((uint8_t)0x8e), buf[11]); // Check continuing hc1 and parity.
    EXPECT_EQ(((uint8_t)0xce), buf[34]); // Check part of checksum.

    // Attempt to decode.
    EXPECT_TRUE(OTRadValve::FHT8VRadValveUtil::FHT8VDecodeBitStream(buf, buf + sizeof(buf) - 1, &commandDecoded));
    EXPECT_EQ(13, commandDecoded.hc1);
    EXPECT_EQ(73, commandDecoded.hc2);
    EXPECT_EQ(0x26, commandDecoded.command);
    EXPECT_EQ(0, commandDecoded.extension);
//    // Verify that trailer NOT present.
//    EXPECT_TRUE(!verifyHeaderAndCRCForTrailingMinimalStatsPayload(result1));


//      uint8_t *result1 = FHT8VCreateValveSetCmdFrameHT_r(buf, false, &command, 0, NULL);
//      AssertIsTrueWithErr(((uint8_t)~0U) == *result1, *result1); // Check that result points at terminator value 0xff/~0.
//      //AssertIsTrue((result1 - buf < MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE), result1-buf); // Check not overflowing the buffer.
//      AssertIsTrueWithErr((result1 - buf == 38), result1-buf); // Check correct length: 38-byte body.
//      AssertIsTrueWithErr(((uint8_t)0xcc) == buf[0], buf[0]); // Check that result starts with FHT8V 0xcc preamble.
//      AssertIsTrueWithErr(((uint8_t)0xe3) == buf[6], buf[6]); // Check end of preamble.
//      AssertIsTrueWithErr(((uint8_t)0xce) == buf[34], buf[34]); // Check part of checksum.
//      // Attempt to decode.
//      AssertIsTrue(FHT8VDecodeBitStream(buf, buf + MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE - 1, &commandDecoded));
//      AssertIsTrueWithErr(13 == commandDecoded.hc1, commandDecoded.hc1);
//      AssertIsTrueWithErr(73 == commandDecoded.hc2, commandDecoded.hc2);
//      AssertIsTrueWithErr(0x26 == commandDecoded.command, commandDecoded.command);
//      AssertIsTrueWithErr(0 == commandDecoded.extension, commandDecoded.extension);
//      // Verify that trailer NOT present.
//      AssertIsTrue(!verifyHeaderAndCRCForTrailingMinimalStatsPayload(result1));
//
//     // Encode a basic message to set a valve to 0%, with header but without trailer.
//      command.hc1 = 13;
//      command.hc2 = 73;
//    #ifdef OTV0P2BASE_FHT8V_ADR_USED
//      address = 0;
//    #endif
//      memset(buf, 0xff, sizeof(buf));
//      result1 = FHT8VCreateValveSetCmdFrameHT_r(buf, true, &command, 0, NULL);
//      AssertIsTrueWithErr(((uint8_t)~0U) == *result1, *result1); // Check that result points at terminator value 0xff/~0.
//      //AssertIsTrue((result1 - buf < MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE), result1-buf); // Check not overflowing the buffer.
//      AssertIsTrueWithErr((result1 - buf == RFM22_PREAMBLE_BYTES + 38), result1-buf); // Check correct length: preamble + 38-byte body.
//      AssertIsTrueWithErr(((uint8_t)0xaa) == buf[0], buf[0]); // Check that result starts with FHT8V 0xcc preamble.
//      AssertIsTrueWithErr(((uint8_t)0xcc) == buf[RFM22_PREAMBLE_BYTES], buf[RFM22_PREAMBLE_BYTES]); // Check that result starts with FHT8V 0xcc preamble.
//      AssertIsTrueWithErr(((uint8_t)0xe3) == buf[6+RFM22_PREAMBLE_BYTES], buf[6+RFM22_PREAMBLE_BYTES]); // Check end of preamble.
//      AssertIsTrueWithErr(((uint8_t)0xce) == buf[34+RFM22_PREAMBLE_BYTES], buf[34+RFM22_PREAMBLE_BYTES]); // Check part of checksum.
//      // Attempt to decode.
//      AssertIsTrue(FHT8VDecodeBitStream(buf + RFM22_PREAMBLE_BYTES, buf + MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE - 1, &commandDecoded));
//      AssertIsTrueWithErr(13 == commandDecoded.hc1, commandDecoded.hc1);
//      AssertIsTrueWithErr(73 == commandDecoded.hc2, commandDecoded.hc2);
//      AssertIsTrueWithErr(0x26 == commandDecoded.command, commandDecoded.command);
//      AssertIsTrueWithErr(0 == commandDecoded.extension, commandDecoded.extension);
//      // Verify that trailer NOT present.
//      AssertIsTrue(!verifyHeaderAndCRCForTrailingMinimalStatsPayload(result1));
//
//      // Encode a basic message to set a valve to 0%, with header and trailer.
//      command.hc1 = 13;
//      command.hc2 = 73;
//    #ifdef OTV0P2BASE_FHT8V_ADR_USED
//      address = 0;
//    #endif
//      FullStatsMessageCore_t fullStats;
//      clearFullStatsMessageCore(&fullStats);
//      OTV0P2BASE::captureEntropy1(); // Try to stir a little noise into the PRNG before using it.
//      const bool powerLow = !(OTV0P2BASE::randRNG8() & 0x40); // Random value.
//      fullStats.containsTempAndPower = true;
//      fullStats.tempAndPower.powerLow = powerLow;
//      const int tempC16 = (OTV0P2BASE::randRNG8()&0xff) + (10 << 4); // Random value in range [10C, 25C[.
//      fullStats.tempAndPower.tempC16 = tempC16;
//      memset(buf, 0xff, sizeof(buf));
//      result1 = FHT8VCreateValveSetCmdFrameHT_r(buf, true, &command, 0, &fullStats);
//      AssertIsTrueWithErr(((uint8_t)~0U) == *result1, *result1); // Check that result points at terminator value 0xff/~0.
//      //AssertIsTrue((result1 - buf < MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE), result1-buf); // Check not overflowing the buffer.
//    #if defined(ENABLE_MINIMAL_STATS_TXRX)
//      AssertIsTrueWithErr((result1 - buf == 41 + RFM22_PREAMBLE_BYTES), result1-buf); // Check correct length:preamble + 38-byte body + 3-byte trailer.
//    #else // Expect longer encoding in this case...
//      AssertIsTrueWithErr((result1 - buf == 43 + RFM22_PREAMBLE_BYTES), result1-buf); // Check correct length:preamble + 38-byte body + 5-byte trailer.
//    #endif
//      AssertIsTrueWithErr(((uint8_t)0xaa) == buf[0], buf[0]); // Check that result starts with FHT8V 0xcc preamble.
//      AssertIsTrueWithErr(((uint8_t)0xcc) == buf[RFM22_PREAMBLE_BYTES], buf[RFM22_PREAMBLE_BYTES]); // Check that result starts with FHT8V 0xcc preamble.
//      AssertIsTrueWithErr(((uint8_t)0xe3) == buf[6+RFM22_PREAMBLE_BYTES], buf[6+RFM22_PREAMBLE_BYTES]); // Check end of preamble.
//      AssertIsTrueWithErr(((uint8_t)0xce) == buf[34+RFM22_PREAMBLE_BYTES], buf[34+RFM22_PREAMBLE_BYTES]); // Check part of checksum.
//      // Attempt to decode.
//      const uint8_t *afterBody = FHT8VDecodeBitStream(buf + RFM22_PREAMBLE_BYTES, buf + MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE - 1, &commandDecoded);
//      AssertIsTrue(afterBody);
//      AssertIsTrueWithErr(13 == commandDecoded.hc1, commandDecoded.hc1);
//      AssertIsTrueWithErr(73 == commandDecoded.hc2, commandDecoded.hc2);
//      AssertIsTrueWithErr(0x26 == commandDecoded.command, commandDecoded.command);
//      AssertIsTrueWithErr(0 == commandDecoded.extension, commandDecoded.extension);
//    #if 0 && defined(ENABLE_MINIMAL_STATS_TXRX)
//      OTV0P2BASE::serialPrintAndFlush(F("  Minimal trailer bytes: "));
//      OTV0P2BASE::serialPrintAndFlush(afterBody[0], HEX);
//      OTV0P2BASE::serialPrintAndFlush(' ');
//      OTV0P2BASE::serialPrintAndFlush(afterBody[1], HEX);
//      OTV0P2BASE::serialPrintAndFlush(' ');
//      OTV0P2BASE::serialPrintAndFlush(afterBody[2], HEX);
//      OTV0P2BASE::serialPrintlnAndFlush();
//    #endif
//      // Verify (start of) trailer is OK.
//      for(uint8_t i = 0; i < 3; ++i)
//        {
//        AssertIsTrueWithErr(0xff != afterBody[i], i); // No trailer byte should be 0xff (so 0xff can be terminator).
//        AssertIsTrueWithErr(0 == (0x80 & afterBody[i]), i); // No trailer byte should have its high bit set.
//        }
//    #if defined(ENABLE_MINIMAL_STATS_TXRX)
//      AssertIsTrueWithErr(verifyHeaderAndCRCForTrailingMinimalStatsPayload(afterBody), *afterBody);
//    #endif
//      // Decode values...
//    #if defined(ENABLE_MINIMAL_STATS_TXRX)
//      trailingMinimalStatsPayload_t statsDecoded;
//      extractTrailingMinimalStatsPayload(afterBody, &statsDecoded);
//      AssertIsEqual(powerLow, statsDecoded.powerLow);
//      AssertIsEqual(tempC16, statsDecoded.tempC16);
//    #else
//      FullStatsMessageCore_t statsDecoded;
//      AssertIsTrue(NULL != decodeFullStatsMessageCore(afterBody, sizeof(buf) - (afterBody - buf), (stats_TX_level)OTV0P2BASE::randRNG8(), OTV0P2BASE::randRNG8NextBoolean(), &statsDecoded));
//      AssertIsEqual(powerLow, statsDecoded.tempAndPower.powerLow);
//      AssertIsEqual(tempC16, statsDecoded.tempAndPower.tempC16);
//    #endif
//
//      // Encode a basic message to set a different valve to 0%, with header and trailer.
//      // This one was apparently impossible to TX or RX...
//      command.hc1 = 65;
//      command.hc2 = 74;
//    #ifdef OTV0P2BASE_FHT8V_ADR_USED
//      address = 0;
//    #endif
//      memset(buf, 0xff, sizeof(buf));
//      result1 = FHT8VCreateValveSetCmdFrameHT_r(buf, true, &command, 0, &fullStats);
//    //OTV0P2BASE::serialPrintAndFlush(result1 - buf); OTV0P2BASE::serialPrintlnAndFlush();
//      AssertIsTrueWithErr((result1 - buf) < sizeof(buf), (result1 - buf) - sizeof(buf)); // result1 points to the terminating 0xff, not just after it.
//      AssertIsTrueWithErr(((uint8_t)~0U) == *result1, *result1); // Check that result points at terminator value 0xff/~0.
//      //AssertIsTrue((result1 - buf < MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE), result1-buf); // Check not overflowing the buffer.
//    #if defined(ENABLE_MINIMAL_STATS_TXRX)
//      AssertIsTrueWithErr((result1 - buf == 42 + RFM22_PREAMBLE_BYTES), result1-buf); // Check correct length.
//    #else
//      AssertIsTrueWithErr((result1 - buf == 44 + RFM22_PREAMBLE_BYTES), result1-buf); // Check correct length.
//    #endif
//      AssertIsTrueWithErr(((uint8_t)0xaa) == buf[0], buf[0]); // Check that result starts with FHT8V 0xcc preamble.
//      AssertIsTrueWithErr(((uint8_t)0xcc) == buf[RFM22_PREAMBLE_BYTES], buf[RFM22_PREAMBLE_BYTES]); // Check that result starts with FHT8V 0xcc preamble.
//      // Attempt to decode.
//      afterBody = FHT8VDecodeBitStream(buf + RFM22_PREAMBLE_BYTES, buf + MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE - 1, &commandDecoded);
//      AssertIsTrue(0 != afterBody);
//    //OTV0P2BASE::serialPrintAndFlush(afterBody - buf); OTV0P2BASE::serialPrintlnAndFlush();
//      AssertIsEqual(5, (result1 - buf) - (afterBody - buf));
//      AssertIsTrueWithErr(65 == commandDecoded.hc1, commandDecoded.hc1);
//      AssertIsTrueWithErr(74 == commandDecoded.hc2, commandDecoded.hc2);
//      AssertIsTrueWithErr(0x26 == commandDecoded.command, commandDecoded.command);
//      AssertIsTrueWithErr(0 == commandDecoded.extension, commandDecoded.extension);
//      // Verify trailer start is OK.
//      for(uint8_t i = 0; i < 3; ++i)
//        {
//        AssertIsTrueWithErr(0xff != afterBody[i], i); // No trailer byte should be 0xff (so 0xff can be terminator).
//        AssertIsTrueWithErr(0 == (0x80 & afterBody[i]), i); // No trailer byte should have its high bit set.
//        }
//    #if defined(ENABLE_MINIMAL_STATS_TXRX)
//      AssertIsTrueWithErr(verifyHeaderAndCRCForTrailingMinimalStatsPayload(afterBody), *afterBody);
//    #endif
//      // Decode values...
//      memset(&statsDecoded, 0xff, sizeof(statsDecoded)); // Clear structure...
//    #if defined(ENABLE_MINIMAL_STATS_TXRX)
//      extractTrailingMinimalStatsPayload(afterBody, &statsDecoded);
//      AssertIsEqual(powerLow, statsDecoded.powerLow);
//      AssertIsEqual(tempC16, statsDecoded.tempC16);
//    #else
//      AssertIsTrue(NULL != decodeFullStatsMessageCore(afterBody, sizeof(buf) - (afterBody - buf), (stats_TX_level)OTV0P2BASE::randRNG8(), OTV0P2BASE::randRNG8NextBoolean(), &statsDecoded));
//      AssertIsEqual(powerLow, statsDecoded.tempAndPower.powerLow);
//      AssertIsEqual(tempC16, statsDecoded.tempAndPower.tempC16);
//    #endif
//    #endif
}
