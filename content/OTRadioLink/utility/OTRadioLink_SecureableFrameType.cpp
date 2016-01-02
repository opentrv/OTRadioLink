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
// Parameters:
//  * buf  buffer to encode header to, of at least length buflen; never NULL
//  * buflen  available length in buf; if too small for encoded header routine will fail (return 0)
//  * secure_ true if this is to be a secure frame
//  * fType_  frame type (without secure bit) in range ]FTS_NONE,FTS_INVALID_HIGH[ ie exclusive
//  * seqNum_  least-significant 4 bits are 4 lsbs of frame sequence number
//  * il_  ID length in bytes at most 8 (could be 15 for non-small frames)
//  * id_  source of ID bytes, at least il_ long, non-NULL
//  * bl_  body length in bytes [0,251] at most
//  * tl_  trailer length [1,251[ at most, always == 1 for non-secure frame
//
// This does not permit encoding of frames with more than 64 bytes (ie 'small' frames only).
// This does not deal with encoding the body or the trailer.
// Having validated the parameters they are copied into the structure
// and then into the supplied buffer, returning the number of bytes written.
//
// Performs at least the 'Quick Integrity Checks' from the spec, eg SecureBasicFrame-V0.1-201601.txt
//  * fl >= 4 (type, seq/il, bl, trailer bytes)
//  * fl may be further constrained by system limits, typically to <= 64
//  * type (the first frame byte) is never 0x00, 0x80, 0x7f, 0xff.
//  * il <= 8 for initial implementations (internal node ID is 8 bytes)
//  * il <= fl - 4 (ID length; minimum of 4 bytes of other overhead)
//  * bl <= fl - 4 - il (body length; minimum of 4 bytes of other overhead)
//  * the final frame byte (the final trailer byte) is never 0x00 nor 0xff
//  * tl == 1 for non-secure, tl >= 1 for secure (tl = fl - 3 - il - bl)
// Note: fl = hl-1 + bl + tl = 3+il + bl + tl
//
// (If the parameters are invalid or the buffer too small, 0 is returned to indicate an error.)
// The fl byte in the structure is set to the frame length, else 0 in case of any error.
// Returns number of bytes of encoded header excluding nominally-leading fl length byte; 0 in case of error.
uint8_t SecurableFrameHeader::checkAndEncodeSmallFrameHeader(uint8_t *const buf, const uint8_t bufLen,
                                               const bool secure_, const FrameType_Secureable fType_,
                                               const uint8_t seqNum_,
                                               const uint8_t il_, const uint8_t *const id_,
                                               const uint8_t bl_,
                                               const uint8_t tl_)
    {
    // Make frame 'invalid' until everything is finished and checks out.
    fl = 0;

    // Quick integrity checks.
    // Involves setting some fields as this progresses to enable others to be checked.
    //
    //  * type (the first frame byte) is never 0x00, 0x80, 0x7f, 0xff.
    // Frame type must be valid (in particular precluding all-0s and all-1s values).
    if((FTS_NONE == fType_) || (fType_ >= FTS_INVALID_HIGH)) { return(0); } // ERROR
    fType = secure_ ? (0x80 | (uint8_t) fType_) : (0x7f & (uint8_t) fType_);
    //  * il <= 8 for initial implementations (internal node ID is 8 bytes)
    // ID must be of a valid size, and have a non-NULL pointer.
    if((il_ > maxIDLength) || (NULL == id_)) { return(0); } // ERROR
    // Copy the ID length and bytes to the header struct.
    il = il_;
    memcpy(id, id_, il_);
    // TODO
    // TODO
    // Trailer length must be exactly 1 for non-secure frame.
    if(!secure) { if(1 != _tl) { return(0); } } // ERROR



    // fl = ...
//    if((fl_ < 4) || (fl_ > maxSmallFrameSize)) { return(0); } // ERROR
    return(0); // ERROR TODO/FIXME
    }


    }


