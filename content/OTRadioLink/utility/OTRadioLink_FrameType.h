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
 * Radio message frame types and related information.
 */

#ifndef ARDUINO_LIB_OTRADIOLINK_FRAMETYPE_H
#define ARDUINO_LIB_OTRADIOLINK_FRAMETYPE_H

#include <stdint.h>

namespace OTRadioLink
    {
    // For V0p2 messages on an FS20 carrier (868.35MHz, OOK, 5kbps raw)
    // the leading byte received indicates the frame time that follows.
    enum FrameType_V0p2_FS20
        {
        // An FS20 encoded message is indicated by one or more leading 0xcc bytes.
        FS20_native             = 0xcc,

        FullStatsIDL            = 'x', // 0x78
        FullStatsIDH            = 'z', // 0x7a

        // (Trailing '}' must have high bit set and be followed by (7_5B) CRC byte.)
        JSONRaw                 = '{', // 0x7b

        // Messages to/from the REV9 variant.
        REV9Alert               = '!', // 0x21
        REV9PollAndCmd          = '?', // 0x3f
        REV9PollResponse        = '*', // 0x2a

        BAD_MSG                 = 0 // No message should start with 0x00.
        };

    // For those that are *not* FS20 a high bit set (0x80) indicates a secure message format variant.
    // (For such secure frames the frame type should generally be part of the authenticated data.)
    const static uint8_t V0P2_FRAME_TYPE_NONFS20_SEC_FLAG = 0x80;

    // V0p2 Full Stats Message (short ID)
    // ==================================
    // Can be sent on its own or as a trailer for (say) an FS20/FHT8V message (from V0p2 device).
    // Can be recognised by the msbits of the leading (header) byte
    // Nominally allows support for security (auth/enc),
    // some predefined environmental stats beyond temperature,
    // and the ability for an arbitrary ASCII payload.
    // Note that the message frame never contains 0xff (would be taken to be a message terminator; one can be appended)
    // and is avoids runs of more than about two bytes of all zeros to help keep RF sync depending on the carrier.
    // The ID is two bytes (though effectively 15 bits since the top bits of both bytes must match)
    // and is never encrypted.
    // If this is at the start of a radio frame then ID must be present (IDP==1).
    // If IDH is 1, the top bits of both header bytes is 1, else both are 0 and may be FS20-compatible 'house codes'.
    // The CRC is computed in a conventional way over the header and all data bytes
    // starting with an all-ones initialisation value, and is never encrypted.
    // The ID plus the CRC may be used in an ACK from the hub to semi-uniquely identify this frame,
    // with additional secure/authenticated data for secure links to avoid replay attacks/ambiguity.
    // (Note that if secure transmission is expected a recipient must generally ignore all frames with SEC==0.)
    //
    //           BIT  7     6     5     4     3     2     1    0
    // * byte 0 :  | SEC |  1  |  1  |  1  |R0=0 |IDP=1| IDH | 0 |   header, 1x reserved 0 bit (=0), ID Present(=1), ID High, SECure
    // That resolves as 'x'/0x78 and 'z'/0x7a leading byte for ID low and ID high bits in non-secure variants.
    // See V0p2 code for format and semantics of rest of message.
    const static uint8_t V0P2_MESSAGING_LEADING_FULL_STATS_HEADER_MSBS = 0x74;
    const static uint8_t V0P2_MESSAGING_LEADING_FULL_STATS_HEADER_MASK = 0xfc;
    const static uint8_t V0P2_MESSAGING_LEADING_FULL_STATS_HEADER_BITS_ID_PRESENT = 4;
    const static uint8_t V0P2_MESSAGING_LEADING_FULL_STATS_HEADER_BITS_ID_HIGH = 2;

    // Maximum length of raw JSON (ASCII7 printable text) object {...} message payload.
    // Maximum length of JSON (text) message payload.
    // A little bit less than a power of 2
    // to enable packing along with other info.
    // A little bit smaller than typical radio module frame buffers (eg RFM23B) of 64 bytes
    // to allow other explicit preamble and postamble (such as CRC) to be added,
    // and to allow time from final byte arriving to collect the data without overrun.
    //
    // Absolute maximum, eg with RFM23B / FS20 OOK carrier (and interrupt-serviced RX at hub).
    const static int MSG_JSON_ABS_MAX_LENGTH = 55;
    // Typical/recommended maximum.
    const static int MSG_JSON_MAX_LENGTH = 54;
    // Maximum for frames in 'secure' format, eg with authentication and encryption wrappers.
    const static int MSG_JSON_MAX_LENGTH_SECURE = 32;
    }

#endif
