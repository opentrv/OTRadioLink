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
        // Appears first to assist radio hardware packet handling.
        uint8_t fl;

        // Frame type (top bit indicates secure if 1/true).
        uint8_t frameType;

        // TODO
        };


    }

#endif
