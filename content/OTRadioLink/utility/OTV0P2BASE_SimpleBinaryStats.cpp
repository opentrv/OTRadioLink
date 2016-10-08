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

Author(s) / Copyright (s): Damon Hart-Davis 2014--2016
*/

/*
 Lightweight support for encoding/decoding simple compact binary stats.

 Some of these have been used as trailers on FS20/FHT8V frames, or stand-alone,
 in non-secure frames, circa 2014/2015.
 */

#include <stddef.h>

#include "OTV0P2BASE_SimpleBinaryStats.h"

#include "OTV0P2BASE_CRC.h"
#include "OTV0P2BASE_Security.h"


namespace OTV0P2BASE
{


//#ifdef ENABLE_FS20_ENCODING_SUPPORT
// Return true if header/structure and CRC looks valid for (3-byte) buffered stats payload.
bool verifyHeaderAndCRCForTrailingMinimalStatsPayload(uint8_t const *const buf)
  {
  return((MESSAGING_TRAILING_MINIMAL_STATS_HEADER_MSBS == ((buf[0]) & MESSAGING_TRAILING_MINIMAL_STATS_HEADER_MASK)) && // Plausible header.
         (0 == (buf[1] & 0x80)) && // Top bit is clear on this byte also.
         (buf[2] == OTV0P2BASE::crc7_5B_update(buf[0], buf[1]))); // CRC validates, top bit implicitly zero.
  }
//#endif // ENABLE_FS20_ENCODING_SUPPORT

//#ifdef ENABLE_FS20_ENCODING_SUPPORT
// Store minimal stats payload into (2-byte) buffer from payload struct (without CRC); values are coerced to fit as necessary..
//   * payload  must be non-NULL
// Used for minimal and full packet forms,
void writeTrailingMinimalStatsPayloadBody(uint8_t *buf, const trailingMinimalStatsPayload_t *payload)
  {
#ifdef DEBUG
  if(NULL == payload) { panic(); }
#endif
  // Temperatures coerced to fit between MESSAGING_TRAILING_MINIMAL_STATS_TEMP_BIAS (-20C) and 0x7ff_MESSAGING_TRAILING_MINIMAL_STATS_TEMP_BIAS (107Cf).
#if MESSAGING_TRAILING_MINIMAL_STATS_TEMP_BIAS > 0
#error MESSAGING_TRAILING_MINIMAL_STATS_TEMP_BIAS must be negative
#endif
  const int16_t bitmask = 0x7ff;
  const int16_t minTempRepresentable = MESSAGING_TRAILING_MINIMAL_STATS_TEMP_BIAS;
  const int16_t maxTempRepresentable = bitmask + MESSAGING_TRAILING_MINIMAL_STATS_TEMP_BIAS;
#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("payload->tempC16: ");
  DEBUG_SERIAL_PRINTFMT(payload->tempC16, DEC);
  DEBUG_SERIAL_PRINT_FLASHSTRING(" min=");
  DEBUG_SERIAL_PRINTFMT(minTempRepresentable, DEC);
  DEBUG_SERIAL_PRINT_FLASHSTRING(" max=");
  DEBUG_SERIAL_PRINTFMT(maxTempRepresentable, DEC);
  DEBUG_SERIAL_PRINTLN();
#endif
  int16_t temp16Cbiased = payload->tempC16;
  if(temp16Cbiased < minTempRepresentable) { temp16Cbiased = minTempRepresentable; }
  else if(temp16Cbiased > maxTempRepresentable) { temp16Cbiased = maxTempRepresentable; }
  temp16Cbiased -= MESSAGING_TRAILING_MINIMAL_STATS_TEMP_BIAS; // Should now be strictly positive.
#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("temp16Cbiased: ");
  DEBUG_SERIAL_PRINTFMT(temp16Cbiased, DEC);
  DEBUG_SERIAL_PRINTLN();
#endif
  const uint8_t byte0 = MESSAGING_TRAILING_MINIMAL_STATS_HEADER_MSBS | (payload->powerLow ? 0x10 : 0) | (temp16Cbiased & 0xf);
  const uint8_t byte1 = (uint8_t) (temp16Cbiased >> 4);
  buf[0] = byte0;
  buf[1] = byte1;
#if 0 && defined(DEBUG)
  for(uint8_t i = 0; i < 2; ++i) { if(0 != (buf[i] & 0x80)) { panic(); } } // MSBits should be clear.
#endif
  }
//#endif // ENABLE_FS20_ENCODING_SUPPORT

//#ifdef ENABLE_FS20_ENCODING_SUPPORT
// Store minimal stats payload into (3-byte) buffer from payload struct and append CRC; values are coerced to fit as necessary..
//   * payload  must be non-NULL
void writeTrailingMinimalStatsPayload(uint8_t *buf, const trailingMinimalStatsPayload_t *payload)
  {
  writeTrailingMinimalStatsPayloadBody(buf, payload);
  buf[2] = OTV0P2BASE::crc7_5B_update(buf[0], buf[1]);
#if 0 && defined(DEBUG)
  for(uint8_t i = 0; i < 3; ++i) { if(0 != (buf[i] & 0x80)) { panic(); } } // MSBits should be clear.
#endif
  }
//#endif // ENABLE_FS20_ENCODING_SUPPORT

//#ifdef ENABLE_FS20_ENCODING_SUPPORT
// Extract payload from valid (3-byte) header+payload+CRC into payload struct; only 2 bytes are actually read.
// Input bytes (eg header and check value) must already have been validated.
void extractTrailingMinimalStatsPayload(const uint8_t *const buf, trailingMinimalStatsPayload_t *const payload)
  {
#ifdef DEBUG
  if(NULL == payload) { panic(); }
#endif
  payload->powerLow = (0 != (buf[0] & 0x10));
  payload->tempC16 = ((((int16_t) buf[1]) << 4) | (buf[0] & 0xf)) + MESSAGING_TRAILING_MINIMAL_STATS_TEMP_BIAS;
  }
//#endif // ENABLE_FS20_ENCODING_SUPPORT


//#ifdef ENABLE_FS20_ENCODING_SUPPORT
// Send (valid) core binary stats to specified print channel, followed by "\r\n".
// This does NOT attempt to flush output nor wait after writing.
// Will only write stats with a source ID.
void outputCoreStats(Print *p, bool secure, const FullStatsMessageCore_t *stats)
  {
  if(stats->containsID)
    {
    // Dump (remote) stats field '@<hexnodeID>;TnnCh[P;]'
    // where the T field shows temperature in C with a hex digit after the binary point indicated by C
    // and the optional P field indicates low power.
    p->print((char) OTV0P2BASE::SERLINE_START_CHAR_RSTATS);
    p->print((((uint16_t)stats->id0) << 8) | stats->id1, 16); // HEX
    if(stats->containsTempAndPower)
      {
      p->print(F(";T"));
      p->print(stats->tempAndPower.tempC16 >> 4, 10); // DEC
      p->print('C');
      p->print(stats->tempAndPower.tempC16 & 0xf, 16); // HEX
      if(stats->tempAndPower.powerLow) { p->print(F(";P")); } // Insert power-low field if needed.
      }
    if(stats->containsAmbL)
      {
      p->print(F(";L"));
      p->print(stats->ambL);
      }
    if(0 != stats->occ)
      {
      p->print(F(";O"));
      p->print(stats->occ);
      }
    p->println();
    }
  }
//#endif // ENABLE_FS20_ENCODING_SUPPORT

//#ifdef ENABLE_FS20_ENCODING_SUPPORT
// Send (valid) minimal binary stats to specified print channel, followed by "\r\n".
// This does NOT attempt to flush output nor wait after writing.
void outputMinimalStats(Print *p, bool secure, uint8_t id0, uint8_t id1, const trailingMinimalStatsPayload_t *stats)
    {
    // Convert to full stats for output.
    FullStatsMessageCore_t fullstats;
    clearFullStatsMessageCore(&fullstats);
    fullstats.id0 = id0;
    fullstats.id1 = id1;
    fullstats.containsID = true;
    memcpy((void *)&fullstats.tempAndPower, stats, sizeof(fullstats.tempAndPower));
    fullstats.containsTempAndPower = true;
    outputCoreStats(p, secure, &fullstats);
    }
//#endif // ENABLE_FS20_ENCODING_SUPPORT


//#ifdef ENABLE_FS20_ENCODING_SUPPORT
// Send core/common 'full' stats message.
//   * content contains data to be sent in the message; must be non-NULL
// Note that up to 7 bytes of payload is optimal for the CRC used.
// If successful, returns pointer to terminating 0xff at end of message.
// Returns NULL if failed (eg because of bad inputs or insufficient buffer space);
// part of the message may have have been written in this case and in particular the previous terminating 0xff may have been overwritten.
uint8_t *encodeFullStatsMessageCore(uint8_t * const buf, const uint8_t buflen, const OTV0P2BASE::stats_TX_level secLevel, const bool secureChannel,
    const FullStatsMessageCore_t * const content)
  {
  if(NULL == buf) { return(NULL); } // Could be an assert/panic instead at a pinch.
  if(NULL == content) { return(NULL); } // Could be an assert/panic instead at a pinch.
  if(secureChannel) { return(NULL); } // TODO: cannot create secure message yet.
//  if(buflen < FullStatsMessageCore_MIN_BYTES_ON_WIRE+1) { return(NULL); } // Need space for at least the shortest possible message + terminating 0xff.
//  if(buflen < FullStatsMessageCore_MAX_BYTES_ON_WIRE+1) { return(NULL); } // Need space for longest possible message + terminating 0xff.

  // Compute message payload length (excluding CRC and terminator).
  // Fail immediately (return NULL) if not enough space for message content.
  const uint8_t payloadLength =
      1 + // Initial header.
      (content->containsID ? 2 : 0) +
      (content->containsTempAndPower ? 2 : 0) +
      1 + // Flags header.
      (content->containsAmbL ? 1 : 0);
  if(buflen < payloadLength + 2)  { return(NULL); }

  // Validate some more detail.
  // ID
  if(content->containsID)
    {
    if((content->id0 == (uint8_t)0xff) || (content->id1 == (uint8_t)0xff)) { return(NULL); } // ID bytes cannot be 0xff.
    if((content->id0 & 0x80) != (content->id1 & 0x80)) { return(NULL); } // ID top bits don't match.
    }
  // Ambient light.
  if(content->containsAmbL)
    {
    if((content->ambL == 0) || (content->ambL == (uint8_t)0xff)) { return(NULL); } // Forbidden values.
    }

  // WRITE THE MESSAGE!
  // Pointer to next byte to write in message.
  uint8_t *b = buf;

  // Construct the header.
  // * byte 0 :  |  0  |  1  |  1  |  1  |  R0 | IDP | IDH | SEC |   header, 1x reserved 0 bit, ID Present, ID High, SECure
//#define MESSAGING_FULL_STATS_HEADER_MSBS 0x70
//#define MESSAGING_FULL_STATS_HEADER_MASK 0xf0
//#define MESSAGING_FULL_STATS_HEADER_BITS_ID_PRESENT 4
//#define MESSAGING_FULL_STATS_HEADER_BITS_ID_HIGH 2
//#define MESSAGING_FULL_STATS_HEADER_BITS_ID_SECURE 1
  const uint8_t header = MESSAGING_FULL_STATS_HEADER_MSBS |
      (content->containsID ? MESSAGING_FULL_STATS_HEADER_BITS_ID_PRESENT : 0) |
      ((content->containsID && (0 != (content->id0 & 0x80))) ? MESSAGING_FULL_STATS_HEADER_BITS_ID_HIGH : 0) |
      0; // TODO: cannot do secure messages yet.
  *b++ = header;

  // Insert ID if requested.
  if(content->containsID)
    {
    *b++ = content->id0 & 0x7f;
    *b++ = content->id1 & 0x7f;
    }

  // Insert basic temperature and power status if requested.
  if(content->containsTempAndPower)
    {
    writeTrailingMinimalStatsPayloadBody(b, &(content->tempAndPower));
    b += 2;
    }

  // Always insert flags header , and downstream optional values.
// Flags indicating which optional elements are present:
// AMBient Light, Relative Humidity %.
// OC1/OC2 = Occupancy: 00 not disclosed, 01 probably, 10 possibly, 11 not occupied recently.
// IF EXT is 1 a futher flags byte follows.
// * byte b+2: |  0  |  1  |  1  | EXT | ABML| RH% | OC1 | OC2 |   EXTension-follows flag, plus optional section flags.
//#define MESSAGING_FULL_STATS_FLAGS_HEADER_MSBS 0x60
//#define MESSAGING_FULL_STATS_FLAGS_HEADER_MASK 0xe0
//#define MESSAGING_FULL_STATS_FLAGS_HEADER_AMBL 8
//#define MESSAGING_FULL_STATS_FLAGS_HEADER_RHP 4
  // Omit occupancy data unless encoding for a secure channel or at a very permissive stats TX security level.
  const uint8_t flagsHeader = MESSAGING_FULL_STATS_FLAGS_HEADER_MSBS |
    (content->containsAmbL ? MESSAGING_FULL_STATS_FLAGS_HEADER_AMBL : 0) |
    ((secureChannel || (secLevel <= OTV0P2BASE::stTXalwaysAll)) ? (content->occ & 3) : 0);
  *b++ = flagsHeader;
  // Now insert extra fields as flagged.
  if(content->containsAmbL)
    { *b++ = content->ambL; }
  // TODO: RH% etc

  // Finish off message by computing and appending the CRC and then terminating 0xff (and return pointer to 0xff).
  // Assumes that b now points just beyond the end of the payload.
  uint8_t crc = MESSAGING_FULL_STATS_CRC_INIT; // Initialisation.
  for(const uint8_t *p = buf; p < b; ) { crc = OTV0P2BASE::crc7_5B_update(crc, *p++); }
  *b++ = crc;
  *b = 0xff;
#if 0 && defined(DEBUG)
  if(b - buf != payloadLength + 1) { panic(F("msg gen err")); }
#endif
  return(b);
  }

// Decode core/common 'full' stats message.
// If successful returns pointer to next byte of message, ie just after full stats message decoded.
// Returns NULL if failed (eg because of corrupt/insufficient message data) and state of 'content' result is undefined.
// This will avoid copying into the result data (possibly tainted) that has arrived at an inappropriate security level.
//   * content will contain data decoded from the message; must be non-NULL
const uint8_t *decodeFullStatsMessageCore(const uint8_t * const buf, const uint8_t buflen, const OTV0P2BASE::stats_TX_level secLevel, const bool secureChannel,
    FullStatsMessageCore_t * const content)
  {
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("dFSMC");
  if(NULL == buf) { return(NULL); } // Could be an assert/panic instead at a pinch.
//DEBUG_SERIAL_PRINTLN_FLASHSTRING(" chk c");
  if(NULL == content) { return(NULL); } // Could be an assert/panic instead at a pinch.
  if(buflen < FullStatsMessageCore_MIN_BYTES_ON_WIRE)
    {
#if 0
    DEBUG_SERIAL_PRINT_FLASHSTRING("buf too small: ");
    DEBUG_SERIAL_PRINT(buflen);
    DEBUG_SERIAL_PRINTLN();
#endif
    return(NULL); // Not long enough for even a minimal message to be present...
    }

//DEBUG_SERIAL_PRINTLN_FLASHSTRING(" clr");
  // Conservatively clear the result completely.
  clearFullStatsMessageCore(content);

  // READ THE MESSAGE!
  // Pointer to next byte to read in message.
  const uint8_t *b = buf;

  // Validate the message header and start to fill in structure.
  const uint8_t header = *b++;
  // Deconstruct the header.
  // * byte 0 :  |  0  |  1  |  1  |  1  |  R0 | IDP | IDH | SEC |   header, 1x reserved 0 bit, ID Present, ID High, SECure
//#define MESSAGING_FULL_STATS_HEADER_MSBS 0x70
//#define MESSAGING_FULL_STATS_HEADER_MASK 0x70
//#define MESSAGING_FULL_STATS_HEADER_BITS_ID_PRESENT 4
//#define MESSAGING_FULL_STATS_HEADER_BITS_ID_HIGH 2
//#define MESSAGING_FULL_STATS_HEADER_BITS_ID_SECURE 0x80
//DEBUG_SERIAL_PRINTLN_FLASHSTRING(" chk msb");
  if(MESSAGING_FULL_STATS_HEADER_MSBS != (header & MESSAGING_FULL_STATS_HEADER_MASK)) { return(NULL); } // Bad header.
//DEBUG_SERIAL_PRINTLN_FLASHSTRING(" chk sec");
  if(0 != (header & MESSAGING_FULL_STATS_HEADER_BITS_ID_SECURE)) { return(NULL); } // TODO: cannot do secure messages yet.
  // Extract ID if present.
//DEBUG_SERIAL_PRINTLN_FLASHSTRING(" chk ID");
  const bool containsID = (0 != (header & MESSAGING_FULL_STATS_HEADER_BITS_ID_PRESENT));
  if(containsID)
    {
    content->containsID = true;
//DEBUG_SERIAL_PRINTLN_FLASHSTRING(" containsID");
    const uint8_t idHigh = ((0 != (header & MESSAGING_FULL_STATS_HEADER_BITS_ID_HIGH)) ? 0x80 : 0);
    content->id0 = *b++ | idHigh;
    content->id1 = *b++ | idHigh;
    }

  // If next header is temp/power then extract it, else must be the flags header.
  if(b - buf >= buflen) { return(NULL); } // Fail if next byte not available.
  if(MESSAGING_TRAILING_MINIMAL_STATS_HEADER_MSBS == (*b & MESSAGING_TRAILING_MINIMAL_STATS_HEADER_MASK))
    {
//DEBUG_SERIAL_PRINTLN_FLASHSTRING(" chk msh");
    if(b - buf >= buflen-1) { return(NULL); } // Fail if 2 bytes not available for this section.
    if(0 != (0x80 & b[1])) { return(NULL); } // Following byte does not have msb correctly cleared.
    extractTrailingMinimalStatsPayload(b, &(content->tempAndPower));
    b += 2;
    content->containsTempAndPower = true;
    }

  // If next header is flags then extract it.
  // FIXME: risk of misinterpretting CRC.
//DEBUG_SERIAL_PRINTLN_FLASHSTRING(" chk flg");
  if(b - buf >= buflen) { return(NULL); } // Fail if next byte not available.
  if(MESSAGING_FULL_STATS_FLAGS_HEADER_MSBS != (*b & MESSAGING_FULL_STATS_FLAGS_HEADER_MASK)) { return(NULL); } // Corrupt message.
  const uint8_t flagsHeader = *b++;
  content->occ = flagsHeader & 3;
  const bool containsAmbL = (0 != (flagsHeader & MESSAGING_FULL_STATS_FLAGS_HEADER_AMBL));
  if(containsAmbL)
    {
    if(b - buf >= buflen) { return(NULL); } // Fail if next byte not available.
    const uint8_t ambL = *b++;
//DEBUG_SERIAL_PRINTLN_FLASHSTRING(" chk aml");
    if((0 == ambL) || (ambL == (uint8_t)0xff)) { return(NULL); } // Illegal value.
    content->ambL = ambL;
    content->containsAmbL = true;
    }

  // Finish off by computing and checking the CRC (and return pointer to just after CRC).
  // Assumes that b now points just beyond the end of the payload.
  if(b - buf >= buflen) { return(NULL); } // Fail if next byte not available.
  uint8_t crc = MESSAGING_FULL_STATS_CRC_INIT; // Initialisation.
  for(const uint8_t *p = buf; p < b; ) { crc = OTV0P2BASE::crc7_5B_update(crc, *p++); }
//DEBUG_SERIAL_PRINTLN_FLASHSTRING(" chk CRC");
  if(crc != *b++) { return(NULL); } // Bad CRC.

  return(b); // Point to just after CRC.
  }

//#endif // ENABLE_FS20_ENCODING_SUPPORT


} // OTV0P2BASE
