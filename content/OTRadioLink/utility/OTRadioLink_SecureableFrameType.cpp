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
 * Radio message secureable frame types and related information.
 *
 * Based on 2015Q4 spec and successors:
 *     http://www.earth.org.uk/OpenTRV/stds/network/20151203-DRAFT-SecureBasicFrame.txt
 *     https://raw.githubusercontent.com/DamonHD/OpenTRV/master/standards/protocol/IoTCommsFrameFormat/SecureBasicFrame-*.txt
 */

#include "OTRadioLink_SecureableFrameType.h"

namespace OTRadioLink
    {


// Check parameters for, and if valid then encode into the given buffer, the header for a small secureable frame.
// This does not permit encoding of frames with more than 64 bytes (ie 'small' frames only).
// This does not deal with encoding the body or the trailer.
// Having validated the parameters they are copied into the structure
// and then into the supplied buffer, returning the number of bytes written.
// Performs at least the 'Quick Integrity Checks' from the spec, eg SecureBasicFrame-V0.1-201601.txt
//  * fl >= 4 (type, seq/il, bl, trailer bytes)
//  * fl may be further constrained by system limits, typically to <= 64
//  * type (the first frame byte) is never 0x00, 0x80, 0x7f, 0xff.
//  * il <= 8 for initial implementations (internal node ID is 8 bytes)
//  * il <= fl - 4 (ID length; minimum of 4 bytes of other overhead)
//  * bl <= fl - 4 - il (body length; minimum of 4 bytes of other overhead)
//  * the final frame byte (the final trailer byte) is never 0x00 nor 0xff
//  * tl == 1 for non-secure, tl >= 1 for secure (tl = fl - 3 - il - bl)
// (If the parameters are invalid or the buffer too small, 0 is returned to indicate an error.)
// The fl byte in the structure is set to the frame length, else 0 in case of any error.
// Returns number of bytes of encoded header excluding nominally-leading fl length byte; 0 in case of error.
uint8_t SecurableFrameHeader::checkAndEncodeSmallFrameHeader(uint8_t *const buf, const uint8_t bufLen,
                                               const uint8_t fl_,
                                               const bool secure_, const FrameType_Secureable fType_,
                                               const uint8_t seqNum_,
                                               const uint8_t il_, const uint8_t *const id_,
                                               const uint8_t bl_,
                                               const uint8_t tl_)
    {
    return(0); // ERROR TODO/FIXME
    }


    }


