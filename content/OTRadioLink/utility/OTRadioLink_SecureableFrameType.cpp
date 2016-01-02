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

#include <string.h>

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
//  * id_  source of ID bytes, at least il_ long; NULL means pre-filled but must not start with 0xff.
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
//  1) fl >= 4 (type, seq/il, bl, trailer bytes)
//  2) fl may be further constrained by system limits, typically to <= 64
//  3) type (the first frame byte) is never 0x00, 0x80, 0x7f, 0xff.
//  4) il <= 8 for initial implementations (internal node ID is 8 bytes)
//  5) il <= fl - 4 (ID length; minimum of 4 bytes of other overhead)
//  6) bl <= fl - 4 - il (body length; minimum of 4 bytes of other overhead)
//  7) the final frame byte (the final trailer byte) is never 0x00 nor 0xff
//  8) tl == 1 for non-secure, tl >= 1 for secure (tl = fl - 3 - il - bl)
// Note: fl = hl-1 + bl + tl = 3+il + bl + tl
//
// (If the parameters are invalid or the buffer too small, 0 is returned to indicate an error.)
// The fl byte in the structure is set to the frame length, else 0 in case of any error.
// Returns number of bytes of encoded header excluding nominally-leading fl length byte; 0 in case of error.
uint8_t SecurableFrameHeader::checkAndEncodeSmallFrameHeader(uint8_t *const buf, const uint8_t buflen,
                                               const bool secure_, const FrameType_Secureable fType_,
                                               const uint8_t seqNum_,
                                               const uint8_t il_, const uint8_t *const id_,
                                               const uint8_t bl_,
                                               const uint8_t tl_)
    {
    // Make frame 'invalid' until everything is finished and checks out.
    fl = 0;

    // Quick integrity checks from spec.
    //
    // (Because it the spec is primarily focused on checking received packets,
    // things happen in a different order here.)
    //
    // Involves setting some fields as this progresses to enable others to be checked.
    // Must be done in an order that avoids overflow from even egregious bad values,
    // and that is efficient since this will be on every TX code path.
    //
    //  1) NOT APPLICABLE FOR ENCODE: fl >= 4 (type, seq/il, bl, trailer bytes)
    //  3) type (the first frame byte) is never 0x00, 0x80, 0x7f, 0xff.
    // Frame type must be valid (in particular precluding all-0s and all-1s values).
    if((FTS_NONE == fType_) || (fType_ >= FTS_INVALID_HIGH)) { return(0); } // ERROR
    fType = secure_ ? (0x80 | (uint8_t) fType_) : (0x7f & (uint8_t) fType_);
    //  4) il <= 8 for initial implementations (internal node ID is 8 bytes)
    //  5) NOT APPLICABLE FOR ENCODE: il <= fl - 4 (ID length; minimum of 4 bytes of other overhead)
    // ID must be of a legitimate size and have a non-NULL pointer,
    // else if prefilled (id_ is NULL) must not start with 0xff.
    const bool idPreFilled = (NULL == id_);
    if((il_ > maxIDLength) || (idPreFilled && (0xff == id[0]))) { return(0); } // ERROR
    // Copy the ID length and bytes, and sequence number lsbs, to the header struct.
    seqIl = il_ | (seqNum_ << 4);
    if(!idPreFilled) { memcpy(id, id_, il_); }
    // Header length minus frame length byte.
    const uint8_t hlmfl = 3 + il_;
    // Error return if not enough space in buf for encoded header.
    if(hlmfl > buflen) { return(0); } // ERROR
    //  6) bl <= fl - 4 - il (body length; minimum of 4 bytes of other overhead)
    //  2) fl may be further constrained by system limits, typically to <= 64
    if(bl_ > maxSmallFrameSize - 1 - hlmfl) { return(0); } // ERROR
    //  8) NON_SECURE: tl == 1 for non-secure
    if(!secure_) { if(1 != tl_) { return(0); } } // ERROR
    // Trailer length must be at least 1 for non-secure frame.
    else
        {
        // Zero-length trailer never allowed.
        if(0 == tl_) { return(0); } // ERROR
        //  8) OVERSIZE WHEN SECURE: tl >= 1 for secure (tl = fl - 3 - il - bl)
        //  2) fl may be further constrained by system limits, typically to <= 64
        if(tl_ > maxSmallFrameSize - hlmfl - bl_) { return(0); } // ERROR
        }

    const uint8_t fl_ = hlmfl + bl_ + tl_;
    // Should not get here if true // if((fl_ > maxSmallFrameSize)) { return(0); } // ERROR

    // Write encoded header to buf.
    buf[0] = fType;
    buf[1] = seqIl;
    memcpy(buf + 2, id, il_);
    buf[2 + il_] = bl_;

    fl = fl_; // Set fl field to valid value as last action / side-effect.

    // Return header length (without logically-first frame-length byte).
    return(hlmfl);
    }


    }


