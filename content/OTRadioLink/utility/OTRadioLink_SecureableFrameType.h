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
 * Radio message secureable frame types and related information.
 */

#ifndef ARDUINO_LIB_OTRADIOLINK_SECUREABLEFRAMETYPE_H
#define ARDUINO_LIB_OTRADIOLINK_SECUREABLEFRAMETYPE_H

#include <stdint.h>

namespace OTRadioLink
    {


    // Secureable (V0p2) messages.
    // Based on 2015Q4 spec and successors:
    //     http://www.earth.org.uk/OpenTRV/stds/network/20151203-DRAFT-SecureBasicFrame.txt
    // The leading byte received indicates the length of frame that follows,
    // with the following byte indicating the frame type.
    // The leading frame-length byte allows efficient packet RX with many low-end radios.
    enum FrameType_Secureable
        {
        // No message should be type 0x00 (nor 0xff).
        FTS_NONE                     = 0,

        // OpenTRV basic valve/sensor leaf-to-hub frame (secure if high-bit set).
        FTS_BasicSensorOrValve       = 'O', // 0x4f
        };

    // A high bit set (0x80) in the type indicates a secure message format variant.
    // The frame type is part of the authenticated data.
    const static uint8_t SECUREABLE_FRAME_TYPE_SEC_FLAG = 0x80;

    // Logical header for the securable frame format.
    // Organised to be efficient to get in and out of wire format.
    struct SecurableFrameHeader
        {
        // Frame length excluding/after this byte.
        // Appears first on the wire to support radio hardware packet handling.
        //     fl = hl-1 + bl + tl
        // where hl header length, bl body length, tl trailer length
        uint8_t fl;

        // Frame type nominally from FrameType_Secureable.
        // Top bit indicates secure frame if 1/true.
        uint8_t frameType;

        // Frame sequence number mod 16 [0,15] (bits 4 to 7) and ID length [0,15] (bits 0-3).
        //
        // Sequence number increments from 0, wraps at 15;
        // increment is skipped for double/multiple TX for noise immunity.
        // If a counter is used as part of (eg) security IV/nonce
        // then these 4 bits may be its least significant bits.
        uint8_t seqAndIl;

        // ID bytes (0 implies anonymous, 1 or 2 typical domestic, length il)
        //
        // This is the first il bytes of the leaf's (typically 64-bit) full ID.
        // Thus this is typically the ID of the sending sensor/valve/etc,
        // but may under some circumstances (depending on message type)
        // be the ID of the target/recipient.
        const static uint8_t maxIDLength = 8;
        unit8_t id[maxIDLength];

        // Body length including any padding [0,249] but generally << 60.
        uint8_t bl;
        };


    }

#endif
