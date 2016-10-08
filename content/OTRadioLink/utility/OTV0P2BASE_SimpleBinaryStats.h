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

#ifndef OTV0P2BASE_SIMPLEBINARYSTATS_H
#define OTV0P2BASE_SIMPLEBINARYSTATS_H

#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "OTV0P2BASE_Security.h"
#include "OTV0P2BASE_Serial_IO.h"


namespace OTV0P2BASE
{


//#ifdef ENABLE_FS20_ENCODING_SUPPORT
// Minimal stats trailer (for devices supporting FS20 encoding only)
// =====================
// When already sending an (FS20/FHT8V) message for some other reason
// it may be convenient to add a trailing minimal stats payload
// that will be ignored by the original recipient (eg FHT8V valve).
// Note that this never contains 0xff (would be taken to be a message terminator; one can be appended)
// and is not all zeros to help keep RF sync depending on the carrier.
// The minimal stats trailer payload contains the measured temperature and a power-level indicator.
// That is wrapped in an initial byte which positively indicates its presence
// and is unlikely to be confused with the main frame data or sync even if mis-framed,
// or data from the body of the main frame.
// This may also be nominally suitable for a frame on its own, ie with the main data elided.
// For an FHT8V frame, with sync bytes of 0xcc (and 0xaa before),
// and with the 1100 and 111000 encoding of the FHT8V data bits,
// A leading byte whose top bits are 010 should suffice if itself included in the check value.
// The trailer ends with a 7-bit CRC selected for reasonable performance on an 16-bit payload.
// NOTE: the CRC is calculated in an unusual way for speed
// (AT THE RISK OF BREAKING SOMETHING SUBTLE ABOUT THE EFFICACY OF THE CRC)
// with byte 0 used as the initial value and a single update with byte 1 to compute the final CRC.
// The full format is (MSB bits first):
//          BIT  7     6     5     4     3     2     1     0
//   byte 0 : |  0  |  1  |  0  |  PL |  T3 |  T2 |  T1 |  T0 |    header, power-low flag, temperature lsbits (C/16)
//   byte 1 : |  0  | T10 |  T9 |  T8 |  T7 |  T6 |  T5 |  T4 |    temperature msbits (C)
//   byte 2 : |  0  |  C6 |  C5 |  C5 |  C3 |  C2 |  C1 |  C0 |    7-bit CRC (crc7_5B_update)
// Temperature is in 1/16th of Celsius ranging from approx -20C (the bias value) to ~107C,
// which should cover everything from most external UK temperatures up to very hot DHW.

// Size of trailing minimal stats payload (including check values) on FHT8V frame in bytes.
static const uint8_t MESSAGING_TRAILING_MINIMAL_STATS_PAYLOAD_BYTES = 3;
static const uint8_t MESSAGING_TRAILING_MINIMAL_STATS_HEADER_MSBS = 0x40;
static const uint8_t MESSAGING_TRAILING_MINIMAL_STATS_HEADER_MASK = 0xe0;
static const int16_t MESSAGING_TRAILING_MINIMAL_STATS_TEMP_BIAS = (-(20<<4)); // C*16 offset bottom of scale / subtracted from 0C.

// Raw (not-as-transmitted) representation of minimal stats payload header.
// Should be compact in memory.
typedef struct trailingMinimalStatsPayload
  {
  int16_t tempC16 : 15; // Signed fixed-point temperature in C with 4 bits after the binary point.
  bool powerLow : 1; // True if power/battery is low.
  } trailingMinimalStatsPayload_t;

// Store minimal stats payload into (2-byte) buffer from payload struct (without CRC); values are coerced to fit as necessary..
//   * payload  must be non-NULL
// Used for minimal and full packet forms,
void writeTrailingMinimalStatsPayloadBody(uint8_t *buf, const trailingMinimalStatsPayload_t *payload);
// Store minimal stats payload into (3-byte) buffer from payload struct and append CRC; values are coerced to fit as necessary..
//   * payload  must be non-NULL
void writeTrailingMinimalStatsPayload(uint8_t *buf, const trailingMinimalStatsPayload_t *payload);
// Return true if header/structure and CRC looks valid for (3-byte) buffered stats payload.
bool verifyHeaderAndCRCForTrailingMinimalStatsPayload(uint8_t const *buf);
// Extract payload from valid (3-byte) header+payload+CRC into payload struct; only 2 bytes are actually read.
// Input data must already have been validated.
void extractTrailingMinimalStatsPayload(const uint8_t *buf, trailingMinimalStatsPayload_t *payload);
//#endif // ENABLE_FS20_ENCODING_SUPPORT


//#ifdef ENABLE_FS20_ENCODING_SUPPORT
// Full Stats Message (short ID) (for devices supporting FS20 encoding only)
// =============================
// Can be sent on its own or as a trailer for (say) an FHT8V message.
// Can be recognised by the msbits of the leading (header) byte
// Nominally allows support for security (auth/enc),
// some predefined environmental stats beyond temperature,
// and the ability for an arbitrary ASCII payload.
// Note that the message frame never contains 0xff (would be taken to be a message terminator; one can be appended)
// and is avoids runs of more than about two bytes of all zeros to help keep RF sync depending on the carrier.
// The ID is two bytes (though effectively 15 bits since the top bits of both bytes must match)
// and is never encrypted.
// If IDH is 1, the top bits of both header bytes is 1, else both are 0 and may be FS20-compatible 'house codes'.
// The CRC is computed in a conventional way over the header and all data bytes
// starting with an all-ones initialisation value, and is never encrypted.
// The ID plus the CRC may be used in an ACK from the hub to semi-uniquely identify this frame,
// with additional secure/authed data for secure links to avoid replay attacks/ambiguity.
// (Note that if secure transmission is expected a recipient must generally ignore all frames with SEC==0.)
//
// From 2015/07/14 lsb is 0 and msb is SEC for compatibility with other messages on FS20 carrier.
//
//           BIT  7     6     5     4     3     2     1    0
// * byte 0 :  | SEC |  1  |  1  |  1  |  R0 | IDP | IDH | 0 |   SECure, header, 1x reserved 0 bit, ID Present, ID High
// WAS (pre 2015/07/14) * byte 0 :  |  0  |  1  |  1  |  1  |  R0 | IDP | IDH | SEC |   header, 1x reserved 0 bit, ID Present, ID High, SECure
static const uint8_t MESSAGING_FULL_STATS_HEADER_MSBS = 0x70;
static const uint8_t MESSAGING_FULL_STATS_HEADER_MASK = 0x70;
static const uint8_t MESSAGING_FULL_STATS_HEADER_BITS_ID_PRESENT = 4;
static const uint8_t MESSAGING_FULL_STATS_HEADER_BITS_ID_HIGH = 2;
static const uint8_t MESSAGING_FULL_STATS_HEADER_BITS_ID_SECURE = 0x80;

// ?ID: node ID if present (IDP==1)
//             |  0  |            ID0                          |   7 lsbits of first ID byte, unencrypted
//             |  0  |            ID1                          |   7 lsbits of second ID byte, unencrypted

// SECURITY HEADER
// IF SEC BIT IS 1 THEN ONE OR MORE BYTES INSERTED HERE, TBD, EG INCLUDING LENGTH / NONCE.
// IF SEC BIT IS 1 then all bytes between here and the security trailer are encrypted and/or authenticated.

// Temperature and power section, optional, encoded exactly as for minimal stats payload.
//   byte b :  |  0  |  1  |  0  |  PL |  T3 |  T2 |  T1 |  T0 |   header, power-low flag, temperature lsbits (C/16)
//   byte b+1: |  0  | T10 |  T9 |  T8 |  T7 |  T6 |  T5 |  T4 |   temperature msbits (C)

// Flags indicating which optional elements are present:
// AMBient Light, Relative Humidity %.
// OC1/OC2 = Occupancy: 00 not disclosed, 01 not occupied, 10 possibly occupied, 11 probably occupied.
// IF EXT is 1 a further flags byte follows.
// ALWAYS has to be present and has a distinct header from the preceding temp/power header to allow t/p to be omitted unambiguously.
// * byte b+2: |  0  |  1  |  1  | EXT | ABML| RH% | OC1 | OC2 |   EXTension-follows flag, plus optional section flags.
static const uint8_t MESSAGING_FULL_STATS_FLAGS_HEADER_MSBS = 0x60;
static const uint8_t MESSAGING_FULL_STATS_FLAGS_HEADER_MASK = 0xe0;
static const uint8_t MESSAGING_FULL_STATS_FLAGS_HEADER_AMBL = 8;
static const uint8_t MESSAGING_FULL_STATS_FLAGS_HEADER_RHP = 4;
// If EXT = 1:
// Call For Heat, RX High (meaning TX hub can probably turn down power), (SenML) ASCII PayLoad
//   byte b+3: |  0  |  R1 |  R0 |  R0 |  R0 | CFH | RXH | APL |   1x reserved 1 bit, 4x reserved 0 bit, plus optional section flags.

// ?CFH: Call For Heat section, if present.
// May be used as a keep-alive and/or to abruptly stop calling for heat.
// Time in seconds + 1 that this node call for heat for (0--253, encoded as 0x01--0xfe to avoid 0 and 0xff).
// If this field is present and zero (encoded as 0x01) it immediately cancels any current call for heat from this node.
//             |  CFH seconds + 1, range [0,253]               |

// ?ABML: AMBient Light section, if present.
// Lighting level dark--bright 1--254, encoded as 0x01--0xfe to avoid 0 and 0xff).
// This may not be linear, and may not achieve full dynamic range.
// This may be adjusted for typical lighting levels encountered by the node over >= 24h.
//             |  Ambient light level range [1,254]            |

// ?RH%: Relative Humidity %, if present.
// Offset by 1 (encoded range [1,101]) so that a zero byte is never sent.
//             |  0  | RH% [0,100] + 1                         |

// SECURITY TRAILER
// IF SEC BIT IS 1 THEN ZERO OR MORE BYTES INSERTED HERE, TBD.

static const uint8_t MESSAGING_FULL_STATS_CRC_INIT = 0x7f; // Initialisation value for CRC.
// *           |  0  |  C6 |  C5 |  C5 |  C3 |  C2 |  C1 |  C0 |    7-bit CRC (crc7_5B_update), unencrypted

// Representation of core/common elements of a 'full' stats message.
// Flags indicate which fields are actually present.
// All-zeros initialisation ensures no fields marked as present.
// Designed to be reasonably compact in memory.
typedef struct FullStatsMessageCore
  {
  bool containsID : 1; // Keep as first field.

  bool containsTempAndPower : 1;
  bool containsAmbL : 1;

  // Node ID (mandatory, 2 bytes)
  uint8_t id0, id1; // ID bytes must share msbit value.

  // Temperature and low-power (optional, 2 bytes)
  trailingMinimalStatsPayload_t tempAndPower;

  // Ambient lighting level; zero means absent, ~0 is invalid (Optional, 1 byte)
  uint8_t ambL;

  // Occupancy; 00 not disclosed, 01 probably, 10 possibly, 11 not occupied recently.
  uint8_t occ : 2;
  } FullStatsMessageCore_t;

// Maximum size on wire including trailing CRC of core of FullStatsMessage.  TX message buffer should be one larger for trailing 0xff.
static const uint8_t FullStatsMessageCore_MAX_BYTES_ON_WIRE = 8;
// Minimum size on wire including trailing CRC of core of FullStatsMessage.  TX message buffer should be one larger for trailing 0xff.
static const uint8_t FullStatsMessageCore_MIN_BYTES_ON_WIRE = 3;

// Clear a FullStatsMessageCore_t, also indicating no optional fields present.
static inline void clearFullStatsMessageCore(FullStatsMessageCore_t *const p) { memset(p, 0, sizeof(FullStatsMessageCore_t)); }

// Send core/common 'full' stats message.
// Note that up to 7 bytes of payload is optimal for the CRC used.
// If successful, returns pointer to terminating 0xff at end of message.
// Returns NULL if failed (eg because of bad inputs or insufficient buffer space).
// This will omit from transmission data not appropriate given the channel security and the stats_TX_level.
uint8_t *encodeFullStatsMessageCore(uint8_t *buf, uint8_t buflen, OTV0P2BASE::stats_TX_level secLevel, bool secureChannel,
    const FullStatsMessageCore_t *content);

//#if defined(ENABLE_RADIO_RX)
// Decode core/common 'full' stats message.
// If successful returns pointer to next byte of message, ie just after full stats message decoded.
// Returns NULL if failed (eg because of corrupt/insufficient message data) and state of 'content' result is undefined.
// This will avoid copying into the result data (possibly tainted) that has arrived at an inappropriate security level.
//   * content will contain data decoded from the message; must be non-NULL
const uint8_t *decodeFullStatsMessageCore(const uint8_t *buf, uint8_t buflen, OTV0P2BASE::stats_TX_level secLevel, bool secureChannel,
    FullStatsMessageCore_t *content);
//#endif

// Send (valid) core binary stats to specified print channel, followed by "\r\n".
// This does NOT attempt to flush output nor wait after writing.
void outputCoreStats(Print *p, bool secure, const FullStatsMessageCore_t *stats);

// Send (valid) minimal binary stats to specified print channel, followed by "\r\n".
// This does NOT attempt to flush output nor wait after writing.
void outputMinimalStats(Print *p, bool secure, uint8_t id0, uint8_t id1, const trailingMinimalStatsPayload_t *stats);
//#endif // ENABLE_FS20_ENCODING_SUPPORT


} // OTV0P2BASE

#endif // OTV0P2BASE_JSONSTATS_H
