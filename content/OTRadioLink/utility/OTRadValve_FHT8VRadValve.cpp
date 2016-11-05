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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2016
                           Gary Gladman 2015
                           Mike Stirling 2013
*/

/*
 * Driver for FHT8V wireless valve actuator (and FS20 protocol encode/decode).
 *
 * V0p2/AVR only.
 */

#ifdef ARDUINO_ARCH_AVR
#include <util/parity.h>
#endif

#include <OTV0p2Base.h>
#include <OTRadioLink.h>
#include "OTRadValve_AbstractRadValve.h"
#include "OTRadValve_FHT8VRadValve.h"

namespace OTRadValve
    {


#ifdef FHT8VRadValveUtil_DEFINED

// Appends encoded 200us-bit representation of logical bit (true for 1, false for 0); returns new pointer.
// If is1 is false this appends 1100 else (if is1 is true) this appends 111000
// msb-first to the byte stream being created by FHT8VCreate200usBitStreamBptr.
// bptr must be pointing at the current byte to update on entry which must start off as 0xff;
// this will write the byte and increment bptr (and write 0xff to the new location) if one is filled up.
// Partial byte can only have even number of bits present, ie be in one of 4 states.
// Two least significant bits used to indicate how many bit pairs are still to be filled,
// so initial 0xff value (which is never a valid complete filled byte) indicates 'empty'.
// Exposed primarily to allow unit testing.
uint8_t *FHT8VRadValveUtil::_FHT8VCreate200usAppendEncBit(uint8_t *bptr, const bool is1)
  {
  const uint8_t bitPairsLeftM1 = (*bptr) & 3; // Find out how many bit pairs (-1) are left to fill in the current byte.
  if(!is1) // Appending 1100.
    {
    switch(bitPairsLeftM1)
      {
      case 3: // Empty target byte (should be 0xff currently).
        *bptr = 0xcd; // %11001101 Write back partial byte (msbits now 1100 and two bit pairs remain free).
        break;
      case 2: // Top bit pair already filled.
        *bptr = (*bptr & 0xc0) | 0x30; // Preserve existing ms bit-pair, set middle four bits 1100, one bit pair remains free.
        break;
      case 1: // Top two bit pairs already filled.
        *bptr = (*bptr & 0xf0) | 0xc; // Preserve existing ms (2) bit-pairs, set bottom four bits 1100, write back full byte.
        *++bptr = (uint8_t) ~0U; // Initialise next byte for next incremental update.
        break;
      default: // Top three bit pairs already filled.
        *bptr |= 3; // Preserve existing ms (3) bit-pairs, OR in leading 11 bits, write back full byte.
        *++bptr = 0x3e; // Write trailing 00 bits to next byte and indicate 3 bit-pairs free for next incremental update.
        break;
      }
    }
  else // Appending 111000.
    {
    switch(bitPairsLeftM1)
      {
      case 3: // Empty target byte (should be 0xff currently).
        *bptr = 0xe0; // %11100000 Write back partial byte (msbits now 111000 and one bit pair remains free).
        break;
      case 2: // Top bit pair already filled.
        *bptr = (*bptr & 0xc0) | 0x38; // Preserve existing ms bit-pair, set lsbits to 111000, write back full byte.
        *++bptr = (uint8_t) ~0U; // Initialise next byte for next incremental update.
        break;
      case 1: // Top two bit pairs already filled.
        *bptr = (*bptr & 0xf0) | 0xe; // Preserve existing (2) ms bit-pairs, set bottom four bits to 1110, write back full byte.
        *++bptr = 0x3e; // %00111110 Write trailing 00 bits to next byte and indicate 3 bit-pairs free for next incremental update.
        break;
      default: // Top three bit pairs already filled.
        *bptr |= 3; // Preserve existing ms (3) bit-pairs, OR in leading 11 bits, write back full byte.
        *++bptr = 0x8d; // Write trailing 1000 bits to next byte and indicate 2 bit-pairs free for next incremental update.
        break;
      }
    }
  return(bptr);
  }

// If AVR optimised implementation is not available then use own.
#ifndef parity_even_bit
#define parity_even_bit(b) (FHT8VRadValveUtil::xor_parity_even_bit(b))
#endif

// Appends encoded byte in b msbit first plus trailing even parity bit (9 bits total)
// to the byte stream being created by FHT8VCreate200usBitStreamBptr.
static uint8_t *_FHT8VCreate200usAppendByteEP(uint8_t *bptr, const uint8_t b)
  {
  for(uint8_t mask = 0x80; mask != 0; mask >>= 1)
    { bptr = FHT8VRadValveUtil::_FHT8VCreate200usAppendEncBit(bptr, 0 != (b & mask)); }
  return(FHT8VRadValveUtil::_FHT8VCreate200usAppendEncBit(bptr, 0 != parity_even_bit(b))); // Append even parity bit.
  }

// Create stream of bytes to be transmitted to FHT80V at 200us per bit, msbit of each byte first.
// Byte stream is terminated by 0xff byte which is not a possible valid encoded byte.
// On entry the populated FHT8V command struct is passed by pointer.
// On exit, the memory block starting at buffer contains the low-byte, msbit-first, 0xff-terminated TX sequence.
// The maximum and minimum possible encoded message sizes are 35 (all zero bytes) and 45 (all 0xff bytes) bytes long.
// Note that a buffer space of at least 46 bytes is needed to accommodate the longest-possible encoded message and terminator.
// Returns pointer to the terminating 0xff on exit.
uint8_t *FHT8VRadValveUtil::FHT8VCreate200usBitStreamBptr(uint8_t *bptr, const FHT8VRadValveUtil::fht8v_msg_t *command)
  {
  // Generate FHT8V preamble.
  // First 12 x 0 bits of preamble, pre-encoded as 6 x 0xcc bytes.
  *bptr++ = 0xcc;
  *bptr++ = 0xcc;
  *bptr++ = 0xcc;
  *bptr++ = 0xcc;
  *bptr++ = 0xcc;
  *bptr++ = 0xcc;
  *bptr = (uint8_t) ~0U; // Initialise for _FHT8VCreate200usAppendEncBit routine.
  // Push remaining 1 of preamble.
  bptr = _FHT8VCreate200usAppendEncBit(bptr, true); // Encode 1.

  // Generate body.
  bptr = _FHT8VCreate200usAppendByteEP(bptr, command->hc1);
  bptr = _FHT8VCreate200usAppendByteEP(bptr, command->hc2);
#ifdef OTV0P2BASE_FHT8V_ADR_USED
  bptr = _FHT8VCreate200usAppendByteEP(bptr, command->address);
#else
  bptr = _FHT8VCreate200usAppendByteEP(bptr, 0); // Default/broadcast.  TODO: could possibly be further optimised to send 0 value more efficiently.
#endif
  bptr = _FHT8VCreate200usAppendByteEP(bptr, command->command);
  bptr = _FHT8VCreate200usAppendByteEP(bptr, command->extension);
  // Generate checksum.
#ifdef OTV0P2BASE_FHT8V_ADR_USED
  const uint8_t checksum = 0xc + command->hc1 + command->hc2 + command->address + command->command + command->extension;
#else
  const uint8_t checksum = 0xc + command->hc1 + command->hc2 + command->command + command->extension;
#endif
  bptr = _FHT8VCreate200usAppendByteEP(bptr, checksum);

  // Generate trailer.
  // Append 0 bit for trailer.
  bptr = _FHT8VCreate200usAppendEncBit(bptr, false);
  // Append extra 0 bits to ensure that final required bits are flushed out.
  bptr = _FHT8VCreate200usAppendEncBit(bptr, false);
  bptr = _FHT8VCreate200usAppendEncBit(bptr, false);
  *bptr = (uint8_t)0xff; // Terminate TX bytes.
  return(bptr);
  }

// Current decode state.
typedef struct
  {
  uint8_t const *bitStream; // Initially point to first byte of encoded bit stream.
  uint8_t const *lastByte; // point to last byte of bit stream.
  uint8_t mask; // Current bit mask (the next pair of bits to read); initially 0 to become 0xc0;
  bool failed; // If true, the decode has failed and stays failed/true.
  } decode_state_t;

// Decode bit pattern 1100 as 0, 111000 as 1.
// Returns 1 or 0 for the bit decoded, else marks the state as failed.
// Reads two bits at a time, MSB to LSB, advancing the byte pointer if necessary.
static uint8_t readOneBit(decode_state_t *const state)
  {
  if(state->bitStream > state->lastByte) { state->failed = true; } // Stop if off the buffer end.
  if(state->failed) { return(0); } // Refuse to do anything further once decoding has failed.

  if(0 == state->mask) { state->mask = 0xc0; } // Special treatment of 0 as equivalent to 0xc0 on entry.
#if 0 && defined(V0P2BASE_DEBUG)
  if((state->mask != 0xc0) && (state->mask != 0x30) && (state->mask != 0xc) && (state->mask != 3)) { panic(); }
#endif

  // First two bits read must be 11.
  if(state->mask != (state->mask & *(state->bitStream)))
    {
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("leading 11 corrupt");
#endif
    state->failed = true; return(0);
    }

  // Advance the mask; if the mask becomes 0 (then 0xc0 again) then advance the byte pointer.
  if(0 == ((state->mask) >>= 2))
    {
    state->mask = 0xc0;
    // If end of stream is encountered this is an error since more bits need to be read.
    if(++(state->bitStream) > state->lastByte) { state->failed = true; return(0); }
    }

  // Next two bits can be 00 to decode a zero,
  // or 10 (followed by 00) to decode a one.
  const uint8_t secondPair = (state->mask & *(state->bitStream));
  switch(secondPair)
    {
    case 0:
      {
#if 0 && defined(V0P2BASE_DEBUG)
      V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("decoded 0");
#endif
      // Advance the mask; if the mask becomes 0 then advance the byte pointer.
      if(0 == ((state->mask) >>= 2)) { ++(state->bitStream); }
      return(0);
      }
    case 0x80: case 0x20: case 8: case 2: break; // OK: looks like second pair of an encoded 1.
    default:
      {
#if 0 && defined(V0P2BASE_DEBUG)
      V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("Invalid second pair ");
      V0P2BASE_DEBUG_SERIAL_PRINTFMT(secondPair, HEX);
      V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING(" from ");
      V0P2BASE_DEBUG_SERIAL_PRINTFMT(*(state->bitStream), HEX);
      V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif
      state->failed = true; return(0);
      }
    }

  // Advance the mask; if the mask becomes 0 (then 0xc0 again) then advance the byte pointer.
  if(0 == ((state->mask) >>= 2))
    {
    state->mask = 0xc0;
    // If end of stream is encountered this is an error since more bits need to be read.
    if(++(state->bitStream) > state->lastByte) { state->failed = true; return(0); }
    }

  // Third pair of bits must be 00.
  if(0 != (state->mask & *(state->bitStream)))
     {
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("trailing 00 corrupt");
#endif
    state->failed = true; return(0);
    }

  // Advance the mask; if the mask becomes 0 then advance the byte pointer.
  if(0 == ((state->mask) >>= 2)) { ++(state->bitStream); }
#if 0 && defined(V0P2BASE_DEBUG)
  V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("decoded 1");
#endif
  return(1); // Decoded a 1.
  }

// Decodes a series of encoded bits plus parity (and checks the parity, failing if wrong).
// Returns the byte decoded, else marks the state as failed.
static uint8_t readOneByteWithParity(decode_state_t *const state)
  {
  if(state->failed) { return(0); } // Refuse to do anything further once decoding has failed.

  // Read first bit specially...
  const uint8_t b7 = readOneBit(state);
  uint8_t result = b7;
  uint8_t parity = b7;
  // Then remaining 7 bits...
  for(int i = 7; --i >= 0; )
    {
    const uint8_t bit = readOneBit(state);
    parity ^= bit;
    result = (result << 1) | bit;
    }
  // Then get parity bit and check.
  if(parity != readOneBit(state))
    {
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("bad parity");
#endif
    state->failed = true;
    }
  return(result);
  }

// Decode raw bitstream into non-null command structure passed in; returns true if successful.
// Will return non-null if OK, else NULL if anything obviously invalid is detected such as failing parity or checksum.
// Finds and discards leading encoded 1 and trailing 0.
// Returns NULL on failure, else pointer to next full byte after last decoded.
uint8_t const * FHT8VRadValveUtil::FHT8VDecodeBitStream(uint8_t const *bitStream, uint8_t const *lastByte, FHT8VRadValveUtil::fht8v_msg_t *command)
  {
  decode_state_t state;
  state.bitStream = bitStream;
  state.lastByte = lastByte;
  state.mask = 0;
  state.failed = false;

#if 0 && defined(V0P2BASE_DEBUG)
  V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("FHT8VDecodeBitStream:");
  for(uint8_t const *p = bitStream; p <= lastByte; ++p)
      {
      V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING(" &");
      V0P2BASE_DEBUG_SERIAL_PRINTFMT(*p, HEX);
      }
  V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif

  // Find and absorb the leading encoded '1', else quit if not found by end of stream.
  while(0 == readOneBit(&state)) { if(state.failed) { return(NULL); } }
#if 0 && defined(V0P2BASE_DEBUG)
  V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("Read leading 1");
#endif

  command->hc1 = readOneByteWithParity(&state);
  command->hc2 = readOneByteWithParity(&state);
#ifdef OTV0P2BASE_FHT8V_ADR_USED
  command->address = readOneByteWithParity(&state);
#else
  const uint8_t address = readOneByteWithParity(&state);
#endif
  command->command = readOneByteWithParity(&state);
  command->extension = readOneByteWithParity(&state);
  const uint8_t checksumRead = readOneByteWithParity(&state);
  if(state.failed)
    {
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("Failed to read message");
#endif
    return(NULL);
    }

   // Generate and check checksum.
#ifdef OTV0P2BASE_FHT8V_ADR_USED
  const uint8_t checksum = 0xc + command->hc1 + command->hc2 + command->address + command->command + command->extension;
#else
  const uint8_t checksum = 0xc + command->hc1 + command->hc2 + address + command->command + command->extension;
#endif
  if(checksum != checksumRead)
    {
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("Checksum failed");
#endif
    state.failed = true; return(NULL);
    }
#if 0 && defined(V0P2BASE_DEBUG)
  else
    {
    V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("Checksum OK");
    }
#endif

  // Check the trailing encoded '0'.
  if(0 != readOneBit(&state))
    {
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("Read of trailing 0 failed");
#endif
    state.failed = true; return(NULL);
    }
  if(state.failed) { return(NULL); }

#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("Read entire message");
#endif
  // Return pointer to where any trailing data may be
  // in next byte beyond end of FHT8V frame.
  return(state.bitStream + 1);
  }

#endif // FHT8VRadValveUtil_DEFINED



#ifdef FHT8VRadValveBase_DEFINED
// Sends to FHT8V in FIFO mode command bitstream from buffer starting at bptr up until terminating 0xff.
// The trailing 0xff is not sent.
//
// Sends on the TX channel set, defaulting to 0.
//
// If doubleTX is true, this sends the bitstream twice, with a short (~8ms) pause between transmissions, to help ensure reliable delivery.
//
// Returns immediately without transmitting if the command buffer starts with 0xff (ie is empty).
//
// Returns immediately without trying got transmit if the radio is NULL.
//
// Note: single transmission time is up to about 80ms (without extra trailers), double up to about 170ms.
void FHT8VRadValveBase::FHT8VTXFHTQueueAndSendCmd(uint8_t *bptr, const bool doubleTX)
  {
  if(((uint8_t)0xff) == *bptr) { return; }
  OTRadioLink::OTRadioLink * const r = radio;
  if(NULL == r) { return; }

  const uint8_t buflen = OTRadioLink::frameLenFFTerminated(bptr);
  r->sendRaw(bptr, buflen, channelTX, doubleTX ? OTRadioLink::OTRadioLink::TXmax : OTRadioLink::OTRadioLink::TXnormal);

  //V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("SC");
  }

// Call just after TX of valve-setting command which is assumed to reflect current TRVPercentOpen state.
// This helps avoiding calling for heat from a central boiler until the valve is really open,
// eg to avoid excess load on (or energy wasting by) the circulation pump.
// FIXME: compare against own threshold and have NominalRadValve look at least open of all vs minPercentOpen.
void FHT8VRadValveBase::setFHT8V_isValveOpen()
  { FHT8V_isValveOpen = (value >= getMinPercentOpen()); }
//#ifdef LOCAL_TRV // More nuanced test...
//  { FHT8V_isValveOpen = (NominalRadValve.get() >= NominalRadValve.getMinValvePcReallyOpen()); }
//#elif defined(ENABLE_NOMINAL_RAD_VALVE)
//  { FHT8V_isValveOpen = (NominalRadValve.get() >= NominalRadValve.getMinPercentOpen()); }
//#else
//  { FHT8V_isValveOpen = false; }
//#endif

// If true then allow double TX for normal valve setting, else only allow it for sync.
// May want to enforce this where bandwidth is known to be scarce.
static const bool ALLOW_NON_SYNC_DOUBLE_TX = false;

// Send current (assumed valve-setting) command and adjust FHT8V_isValveOpen as appropriate.
// Only appropriate when the command is going to be heard by the FHT8V valve itself, not just the hub.
void FHT8VRadValveBase::valveSettingTX(const bool allowDoubleTX)
  {
  // Transmit correct valve-setting command that should already be in the buffer...
  // May not allow double TX for non-sync transmissions to conserve bandwidth.
  FHT8VTXFHTQueueAndSendCmd(buf, ALLOW_NON_SYNC_DOUBLE_TX && allowDoubleTX);
  // Indicate state that valve should now actually be in (or physically moving to)...
  setFHT8V_isValveOpen();
  }


#ifdef ARDUINO_ARCH_AVR
// Sleep in reasonably low-power mode until specified target subcycle time, optionally listening (RX) for calls-for-heat.
// Returns true if OK, false if specified time already passed or significantly missed (eg by more than one tick).
// May use a combination of techniques to hit the required time.
// Requesting a sleep until at or near the end of the cycle risks overrun and may be unwise.
// Using this to sleep less then 2 ticks may prove unreliable as the RTC rolls on underneath...
// This is NOT intended to be used to sleep over the end of a minor cycle.
// FIXME: should be passed a function (such as pollIO()) to call while waiting.
void FHT8VRadValveBase::sleepUntilSubCycleTimeOptionalRX(const uint8_t sleepUntil)
    {
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("TXwait");
#endif
    // Poll I/O regularly in case listening out for radio comms.
    OTRadioLink::OTRadioLink * const r = radio;

//    if((NULL != r) && (-1 != r->getListenChannel()))
//      {
//      // Only do nap+poll if lots of time left.
//      while(sleepUntil > fmax(OTV0P2BASE::getSubCycleTime() + (50/OTV0P2BASE::SUBCYCLE_TICK_MS_RD), OTV0P2BASE::GSCT_MAX))
////        { nap15AndPoll(); } // Assumed ~15ms sleep max.
//        { OTV0P2BASE::nap(WDTO_15MS, true); r->poll(); } // FIXME: only polls this radio
//      // Poll for remaining time without nap.
//      while(sleepUntil > OTV0P2BASE::getSubCycleTime())
////        { pollIO(); }
//        { r->poll(); } // FIXME: only polls this radio
//      }
//#if 0 && defined(V0P2BASE_DEBUG)
//    V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("*");
//#endif

    // Sleep until the right time, close enough for FS20 purposes.
    // Sleep in a reasonably low-power way, polling radio if present.
    // FIXME: should probably poll I/O generally here.
    // FIXME: could use this to churn some entropy, etc.
    while(sleepUntil > OTV0P2BASE::getSubCycleTime())
      {
      OTV0P2BASE::nap(WDTO_15MS);
      if(NULL != r) { r->poll(); } // FIXME: only polls this radio
      }
    }
#endif // ARDUINO_ARCH_AVR

#ifdef ARDUINO_ARCH_AVR
// Run the algorithm to get in sync with the receiver.
// Uses halfSecondCount.
// Iff this returns true then a(nother) call to FHT8VPollSyncAndTX_Next()
// at or before each 0.5s from the cycle start should be made.
bool FHT8VRadValveBase::doSync(const bool allowDoubleTX)
  {
  // Do not attempt sync at all (and thus do not attempt any other TX) if valve is disabled.
  if(!isAvailable())
    { syncedWithFHT8V = false; return(false); }

  if(0 == syncStateFHT8V)
    {
    // Make start-up a little less eager/greedy.
    //
    // Randomly postpone sync process a little in order to help avoid clashes
    // eg if many mains-powered devices restart at once after a power cut.
    // May also help interaction with CLI at start-up, and reduce peak power demands.
    // Cannot postpone too long as may make user think that something is broken.
    // Can only help a little since even one other sync can drown out this one,
    // and it would be probably bad to *ever* wait a full sync (120s+) period.
    // (If future we could listen for other sync activity, and back off if any heard.)
    // So, KISS.
    //
    // Have approx 15/16 chance of postponing on each call (each 2s),
    // thus typically start well within 32s.
    if(0 != (0x1e & OTV0P2BASE::randRNG8())) { syncedWithFHT8V = false; return(false); } // Postpone.

    // Starting sync process.
    syncStateFHT8V = 241;
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_TIMESTAMP();
    V0P2BASE_DEBUG_SERIAL_PRINT(' ');
    //V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("FHT8V syncing...");
#endif
    OTV0P2BASE::serialPrintlnAndFlush(F("FHT8V SYNC..."));
    }

  if(syncStateFHT8V >= 2)
    {
    // Generate and send sync (command 12) message immediately for odd-numbered ticks, ie once per second.
    if(syncStateFHT8V & 1)
      {
      FHT8VRadValveBase::fht8v_msg_t command;
      command.hc1 = getHC1();
      command.hc2 = getHC2();
      command.command = 0x2c; // Command 12, extension byte present.
      command.extension = syncStateFHT8V;
      FHT8VRadValveBase::FHT8VCreate200usBitStreamBptr(buf, &command);
      if(halfSecondCount > 0)
        { sleepUntilSubCycleTimeOptionalRX((OTV0P2BASE::SUB_CYCLE_TICKS_PER_S/2) * halfSecondCount); }
      FHT8VTXFHTQueueAndSendCmd(buf, allowDoubleTX); // SEND SYNC
      // Note that FHT8VTXCommandArea now does not contain a valid valve-setting command...
#if 0 && defined(V0P2BASE_DEBUG)
      V0P2BASE_DEBUG_SERIAL_TIMESTAMP();
      V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING(" FHT8V SYNC ");
      V0P2BASE_DEBUG_SERIAL_PRINT(syncStateFHT8V);
      V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif
      }

//    handleQueuedMessages(&Serial, true, radio); // Deal with any pending I/O built up while waiting.

    // After penultimate sync TX set up time to sending of final sync command.
    if(1 == --syncStateFHT8V)
      {
      // Set up timer to sent sync final (0) command
      // with formula: t = 0.5 * (HC2 & 7) + 4 seconds.
      halfSecondsToNextFHT8VTX = (getHC2() & 7) + 8; // Note units of half-seconds for this counter.
      halfSecondsToNextFHT8VTX -= (MAX_HSC - halfSecondCount);
      return(false); // No more TX this minor cycle.
      }
    }

  else // syncStateFHT8V == 1 so waiting to send sync final (0) command...
    {
    if(--halfSecondsToNextFHT8VTX == 0)
      {
      // Send sync final command.
      FHT8VRadValveBase::fht8v_msg_t command;
      command.hc1 = getHC1();
      command.hc2 = getHC2();
      command.command = 0x20; // Command 0, extension byte present.
      command.extension = 0; // DHD20130324: could set to TRVPercentOpen, but anything other than zero seems to lock up FHT8V-3 units.
      FHT8V_isValveOpen = false; // Note that valve will be closed (0%) upon receipt.
      FHT8VRadValveBase::FHT8VCreate200usBitStreamBptr(buf, &command);
      if(halfSecondCount > 0) { sleepUntilSubCycleTimeOptionalRX((OTV0P2BASE::SUB_CYCLE_TICKS_PER_S/2) * halfSecondCount); }
      FHT8VTXFHTQueueAndSendCmd(buf, allowDoubleTX); // SEND SYNC FINAL
      // Note that FHT8VTXCommandArea now does not contain a valid valve-setting command...
#if 0 && defined(V0P2BASE_DEBUG)
      V0P2BASE_DEBUG_SERIAL_TIMESTAMP();
      V0P2BASE_DEBUG_SERIAL_PRINT(' ');
      //V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING(" FHT8V SYNC FINAL");
#endif
      OTV0P2BASE::serialPrintlnAndFlush(F("FHT8V SYNC FINAL"));

      // Assume NOW IN SYNC with the valve...
      syncedWithFHT8V = true;

      // On PICAXE there was no time to recompute valve-setting command immediately after SYNC FINAL SEND...
      // Mark buffer as empty to get it filled with the real TRV valve-setting command ASAP.
//      *FHT8VTXCommandArea = 0xff;
      // On the ATmega there is plenty of CPU heft
      // to fill the command buffer immediately with a valve-setting command.
      FHT8VCreateValveSetCmdFrame(get());

      // Set up correct delay to next TX; no more this minor cycle...
      halfSecondsToNextFHT8VTX = FHT8VTXGapHalfSeconds(command.hc2, halfSecondCount);
      return(false);
      }
    }

  // For simplicity, insist on being called every half-second during sync.
  // TODO: avoid forcing most of these calls to save some CPU/energy and improve responsiveness.
  return(true);
  }
#endif // ARDUINO_ARCH_AVR


// Call at start of minor cycle to manage initial sync and subsequent comms with FHT8V valve.
// Conveys this system's TRVPercentOpen value to the FHT8V value periodically,
// setting FHT8V_isValveOpen true when the valve will be open/opening provided it received the latest TX from this system.
//
//   * allowDoubleTX  if true then a double TX is allowed for better resilience, but at cost of extra time and energy
//
// Uses its static/internal transmission buffer, and always leaves it in valid date.
//
// Iff this returns true then call FHT8VPollSyncAndTX_Next() at or before each 0.5s from the cycle start
// to allow for possible transmissions.
//
// See https://sourceforge.net/p/opentrv/wiki/FHT%20Protocol/ for the underlying protocol.
bool FHT8VRadValveBase::FHT8VPollSyncAndTX_First(const bool allowDoubleTX)
  {
  halfSecondCount = 0;

#ifdef IGNORE_FHT_SYNC // Will TX on 0 and 2 half second offsets.
  // Transmit correct valve-setting command that should already be in the buffer...
  valveSettingTX(allowDoubleTX);
  return(true); // Will need anther TX in slot 2.
#else

  // Give priority to getting in sync over all other tasks, though pass control to them afterwards...
  // NOTE: startup state, or state to force resync is: syncedWithFHT8V = 0 AND syncStateFHT8V = 0
  // Always make maximum effort to be heard by valve when syncing (ie do double TX).
  if(!syncedWithFHT8V) { return(doSync(true)); }

#if 0 && defined(V0P2BASE_DEBUG)
   if(0 == halfSecondsToNextFHT8VTX) { panic(F("FHT8V hs count 0 too soon")); }
#endif

  // If no TX required in this minor cycle then can return false quickly (having decremented ticks-to-next-TX value suitably).
  if(halfSecondsToNextFHT8VTX > MAX_HSC+1)
    {
    halfSecondsToNextFHT8VTX -= (MAX_HSC+1);
    return(false); // No TX this minor cycle.
    }

  // TX is due this (first) slot so do it (and no more will be needed this minor cycle).
  if(0 == --halfSecondsToNextFHT8VTX)
    {
    valveSettingTX(allowDoubleTX); // Should be heard by valve.
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_TIMESTAMP();
    V0P2BASE_DEBUG_SERIAL_PRINT(' ');
    // V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING(" FHT8V TX");
    OTV0P2BASE::serialPrintlnAndFlush(F("FHT8V TX"));
#endif
    // Set up correct delay to next TX.
    halfSecondsToNextFHT8VTX = FHT8VTXGapHalfSeconds(getHC2(), 0);
    return(false);
    }

  // Will need to TX in a following slot in this minor cycle...
  return(true);
#endif
  }

#ifdef ARDUINO_ARCH_AVR
// If FHT8VPollSyncAndTX_First() returned true then call this each 0.5s from the start of the cycle, as nearly as possible.
// This allows for possible transmission slots on each half second.
//
//   * allowDoubleTX  if true then a double TX is allowed for better resilience, but at cost of extra time and energy
//
// This will sleep (at reasonably low power) as necessary to the start of its TX slot,
// else will return immediately if no TX needed in this slot.
//
// Iff this returns false then no further TX slots will be needed
// (and thus this routine need not be called again) on this minor cycle
bool FHT8VRadValveBase::FHT8VPollSyncAndTX_Next(const bool allowDoubleTX)
  {
  ++halfSecondCount; // Reflects count of calls since _First(), ie how many
#if 0 && defined(V0P2BASE_DEBUG)
    if(halfSecondCount > MAX_HSC) { panic(F("FHT8VPollSyncAndTX_Next() called too often")); }
#endif

#ifdef IGNORE_FHT_SYNC // Will TX on 0 and 2 half second offsets.
  if(2 == halfSecondCount)
      {
      // Sleep until 1s from start of cycle.
      sleepUntilSubCycleTimeOptionalRX(OTV0P2BASE::SUB_CYCLE_TICKS_PER_S);
      // Transmit correct valve-setting command that should already be in the buffer...
      valveSettingTX(allowDoubleTX);
      return(false); // Don't need any slots after this.
      }
  return(true); // Need to do further TXes this minor cycle.
#else

  // Give priority to getting in sync over all other tasks, though pass control to them afterwards...
  // NOTE: startup state, or state to force resync is: syncedWithFHT8V = 0 AND syncStateFHT8V = 0
  // Always make maximum effort to be heard by valve when syncing (ie do double TX).
  if(!syncedWithFHT8V) { return(doSync(true)); }

  // TX is due this slot so do it (and no more will be needed this minor cycle).
  if(0 == --halfSecondsToNextFHT8VTX)
    {
    sleepUntilSubCycleTimeOptionalRX((OTV0P2BASE::SUB_CYCLE_TICKS_PER_S/2) * halfSecondCount); // Sleep.
    valveSettingTX(allowDoubleTX); // Should be heard by valve.
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_TIMESTAMP();
    V0P2BASE_DEBUG_SERIAL_PRINT(' ');
    // V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING(" FHT8V TX");
#endif
    OTV0P2BASE::serialPrintlnAndFlush(F("FHT8V TX"));
//    handleQueuedMessages(&Serial, true, radio); // Deal with any pending I/O built up while waiting.

    // Set up correct delay to next TX.
    halfSecondsToNextFHT8VTX = FHT8VRadValveBase::FHT8VTXGapHalfSeconds(getHC2(), halfSecondCount);
    return(false);
    }

  // Will need to TX in a following slot in this minor cycle...
  return(true);
#endif
  }
#endif // ARDUINO_ARCH_AVR



#ifdef ARDUINO_ARCH_AVR
// Clear both housecode parts (and thus disable local valve), in non-volatile (EEPROM) store also.
// Does nothing if FHT8V not in use.
void FHT8VRadValveBase::nvClearHC()
  {
  clearHC();
  OTV0P2BASE::eeprom_smart_erase_byte((uint8_t*)V0P2BASE_EE_START_FHT8V_HC1);
  OTV0P2BASE::eeprom_smart_erase_byte((uint8_t*)V0P2BASE_EE_START_FHT8V_HC2);
  }
#endif // ARDUINO_ARCH_AVR

#ifdef ARDUINO_ARCH_AVR
// Set (non-volatile) HC1 and HC2 for single/primary FHT8V wireless valve under control.
// Also set value in FHT8V rad valve model.
// Does nothing if FHT8V not in use.
void FHT8VRadValveBase::nvSetHC1(const uint8_t hc)
  {
  setHC1(hc);
  OTV0P2BASE::eeprom_smart_update_byte((uint8_t*)V0P2BASE_EE_START_FHT8V_HC1, hc);
  }
void FHT8VRadValveBase::nvSetHC2(const uint8_t hc)
  {
  setHC2(hc);
  OTV0P2BASE::eeprom_smart_update_byte((uint8_t*)V0P2BASE_EE_START_FHT8V_HC2, hc);
  }
#endif // ARDUINO_ARCH_AVR


#ifdef ARDUINO_ARCH_AVR
// Get (non-volatile) HC1 and HC2 for single/primary FHT8V wireless valve under control (will be 0xff until set).
// FHT8V instance values are used as a cache.
// Does nothing if FHT8V not in use.
uint8_t FHT8VRadValveBase::nvGetHC1()
  {
  const uint8_t vv = getHC1();
  // If cached value in FHT8V instance is valid, return it.
  if(OTRadValve::FHT8VRadValveBase::isValidFHTV8HouseCode(vv))
    {
    return(vv);
    }
  // Else if EEPROM value is valid, then cache it in the FHT8V instance and return it.
  const uint8_t ev = eeprom_read_byte((uint8_t*)V0P2BASE_EE_START_FHT8V_HC1);
  if(OTRadValve::FHT8VRadValveBase::isValidFHTV8HouseCode(ev))
    {
    setHC1(ev);
    }
  return(ev);
  }
uint8_t FHT8VRadValveBase::nvGetHC2()
  {
  const uint8_t vv = getHC2();
  // If cached value in FHT8V instance is valid, return it.
  if(OTRadValve::FHT8VRadValveBase::isValidFHTV8HouseCode(vv))
    {
    return(vv);
    }
  // Else if EEPROM value is valid, then cache it in the FHT8V instance and return it.
  const uint8_t ev = eeprom_read_byte((uint8_t*)V0P2BASE_EE_START_FHT8V_HC2);
  if(OTRadValve::FHT8VRadValveBase::isValidFHTV8HouseCode(ev))
    {
    setHC2(ev);
    }
  return(ev);
  }
// Load EEPROM house codes into primary FHT8V instance at start-up or once cleared in FHT8V instance.
void FHT8VRadValveBase::nvLoadHC()
  {
  // Uses side-effect to cache/save in FHT8V instance.
  nvGetHC1();
  nvGetHC2();
  }
#endif // ARDUINO_ARCH_AVR


#ifdef ARDUINO_ARCH_AVR
// CLI support.
// Clear/set house code ("H" or "H nn mm").
// Will clear/set the non-volatile (EEPROM) values and the live ones.
bool FHT8VRadValveBase::SetHouseCode::doCommand(char *const buf, const uint8_t buflen)
    {
    if(NULL == v) { OTV0P2BASE::CLI::InvalidIgnored(); return(false); } // Can't work without valve pointer/
    char *last; // Used by strtok_r().
    char *tok1;
    // Minimum 5 character sequence makes sense and is safe to tokenise, eg "H 1 2".
    if((buflen >= 5) && (NULL != (tok1 = strtok_r(buf+2, " ", &last))))
      {
      char *tok2 = strtok_r(NULL, " ", &last);
      if(NULL != tok2)
        {
        const int hc1 = atoi(tok1);
        const int hc2 = atoi(tok2);
        if((hc1 < 0) || (hc1 > 99) || (hc2 < 0) || (hc2 > 99)) { OTV0P2BASE::CLI::InvalidIgnored(); }
        else
          {
          // Set house codes and force resync if changed.
          v->nvSetHC1(hc1);
          v->nvSetHC2(hc2);
          }
        }
      }
    else if(buflen < 2) // Just 'H', possibly with trailing whitespace.
      {
      v->nvClearHC(); // Clear codes and force into unsynchronized state.
      }
    // Done: show updated status line, possibly with results of update.
    return(true);
    }
#endif // ARDUINO_ARCH_AVR

#endif // FHT8VRadValveBase_DEFINED


    }
