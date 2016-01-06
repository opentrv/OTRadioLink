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

#include "OTRadioLink_CRC.h"

namespace OTRadioLink
    {


// Check parameters for, and if valid then encode into the given buffer, the header for a small secureable frame.
// The buffer starts with the fl frame length byte.
//
// Parameters:
//  * buf  buffer to encode header to, of at least length buflen; if NULL the encoded form is not written
//  * buflen  available length in buf; if too small for encoded header routine will fail (return 0)
//  * secure_ true if this is to be a secure frame
//  * fType_  frame type (without secure bit) in range ]FTS_NONE,FTS_INVALID_HIGH[ ie exclusive
//  * seqNum_  least-significant 4 bits are 4 lsbs of frame sequence number
//  * il_  ID length in bytes at most 8 (could be 15 for non-small frames)
//  * id_  source of ID bytes, at least il_ long; NULL means pre-filled but must not start with 0xff.
//  * bl_  body length in bytes [0,251] at most
//  * tl_  trailer length [1,251[ at most, always == 1 for non-secure frame
//
// This does not permit encoding of frames with more than 64 bytes (ie 'small' frames only).
// This does not deal with encoding the body or the trailer.
// Having validated the parameters they are copied into the structure
// and then into the supplied buffer, returning the number of bytes written.
//
// Performs as many as possible of the 'Quick Integrity Checks' from the spec, eg SecureBasicFrame-V0.1-201601.txt
//  1) fl >= 4 (type, seq/il, bl, trailer bytes)
//  2) fl may be further constrained by system limits, typically to <= 63
//  3) type (the first frame byte) is never 0x00, 0x80, 0x7f, 0xff.
//  4) il <= 8 for initial implementations (internal node ID is 8 bytes)
//  5) il <= fl - 4 (ID length; minimum of 4 bytes of other overhead)
//  6) bl <= fl - 4 - il (body length; minimum of 4 bytes of other overhead)
//  7) NOT DONE: the final frame byte (the final trailer byte) is never 0x00 nor 0xff
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
    // Must be done in a manner that avoids overflow with even egregious/malicious bad values,
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
    // Header length including frame length byte.
    const uint8_t hlifl = 4 + il_;
    // Error return if not enough space in buf for the complete encoded header.
    if(hlifl > buflen) { return(0); } // ERROR
    //  6) bl <= fl - 4 - il (body length; minimum of 4 bytes of other overhead)
    //  2) fl may be further constrained by system limits, typically to <= 64
    if(bl_ > maxSmallFrameSize - hlifl) { return(0); } // ERROR
    bl = bl_;
    //  8) NON_SECURE: tl == 1 for non-secure
    if(!secure_) { if(1 != tl_) { return(0); } } // ERROR
    // Trailer length must be at least 1 for non-secure frame.
    else
        {
        // Zero-length trailer never allowed.
        if(0 == tl_) { return(0); } // ERROR
        //  8) OVERSIZE WHEN SECURE: tl >= 1 for secure (tl = fl - 3 - il - bl)
        //  2) fl may be further constrained by system limits, typically to <= 64
        if(tl_ > maxSmallFrameSize+1 - hlifl - bl_) { return(0); } // ERROR
        }

    const uint8_t fl_ = hlifl-1 + bl_ + tl_;
    // Should not get here if true // if((fl_ > maxSmallFrameSize)) { return(0); } // ERROR

    // Write encoded header to buf starting with fl if buf is non-NULL.
    if(NULL != buf)
        {
        buf[0] = fl_;
        buf[1] = fType;
        buf[2] = seqIl;
        memcpy(buf + 3, id, il_);
        buf[3 + il_] = bl_;
        }

    // Set fl field to valid value as last action / side-effect.
    fl = fl_;

    // Return encoded header length including frame-length byte; body should immediately follow.
    return(hlifl); // SUCCESS!
    }

// Decode header and check parameters/validity for inbound short secureable frame.
// The buffer starts with the fl frame length byte.
//
// Parameters:
//  * buf  buffer to decode header from, of at least length buflen; never NULL
//  * buflen  available length in buf; if too small for encoded header routine will fail (return 0)
//
// Performs as many as possible of the 'Quick Integrity Checks' from the spec, eg SecureBasicFrame-V0.1-201601.txt
//  1) fl >= 4 (type, seq/il, bl, trailer bytes)
//  2) fl may be further constrained by system limits, typically to <= 63
//  3) type (the first frame byte) is never 0x00, 0x80, 0x7f, 0xff.
//  4) il <= 8 for initial implementations (internal node ID is 8 bytes)
//  5) il <= fl - 4 (ID length; minimum of 4 bytes of other overhead)
//  6) bl <= fl - 4 - il (body length; minimum of 4 bytes of other overhead)
//  7) the final frame byte (the final trailer byte) is never 0x00 nor 0xff (if whole frame available)
//  8) tl == 1 for non-secure, tl >= 1 for secure (tl = fl - 3 - il - bl)
// Note: fl = hl-1 + bl + tl = 3+il + bl + tl
//
// (If the header is invalid or the buffer too small, 0 is returned to indicate an error.)
// The fl byte in the structure is set to the frame length, else 0 in case of any error.
// Returns number of bytes of decoded header excluding nominally-leading fl length byte; 0 in case of error.
uint8_t SecurableFrameHeader::checkAndDecodeSmallFrameHeader(const uint8_t *const buf, uint8_t buflen)
    {
    // Make frame 'invalid' until everything is finished and checks out.
    fl = 0;

    // If buf is NULL or clearly too small to contain a valid header then return an error.
    if(NULL == buf) { return(0); } // ERROR
    if(buflen < 4) { return(0); } // ERROR

    // Quick integrity checks from spec.
    //  1) fl >= 4 (type, seq/il, bl, trailer bytes)
    const uint8_t fl_ = buf[0];
    if(fl_ < 4) { return(0); } // ERROR
    //  2) fl may be further constrained by system limits, typically to < 64, eg for 'small' frame.
    if(fl_ > maxSmallFrameSize) { return(0); } // ERROR
    //  3) type (the first frame byte) is never 0x00, 0x80, 0x7f, 0xff.
    fType = buf[1];
    const bool secure_ = isSecure();
    const FrameType_Secureable fType_ = (FrameType_Secureable)(fType & 0x7f);
    if((FTS_NONE == fType_) || (fType_ >= FTS_INVALID_HIGH)) { return(0); } // ERROR
    //  4) il <= 8 for initial implementations (internal node ID is 8 bytes)
    seqIl = buf[2];
    const uint8_t il_ = getIl();
    if(il_ > maxIDLength) { return(0); } // ERROR
    //  5) il <= fl - 4 (ID length; minimum of 4 bytes of other overhead)
    if(il_ > fl_ - 4) { return(0); } // ERROR
    // Header length including frame length byte.
    const uint8_t hlifl = 4 + il_;
    // If buffer doesn't contain enough data for the full header then return an error.
    if(hlifl > buflen) { return(0); } // ERROR
    // Capture the ID bytes.
    memcpy(id, buf+3, il_);
    //  6) bl <= fl - 4 - il (body length; minimum of 4 bytes of other overhead)
    const uint8_t bl_ = buf[hlifl - 1];
    if(bl_ > fl_ - hlifl) { return(0); } // ERROR
    bl = bl_;
    //  7) ONLY CHECKED IF FULL FRAME AVAILABLE: the final frame byte (the final trailer byte) is never 0x00 nor 0xff
    if(buflen > fl_)
        {
        const uint8_t lastByte = buf[fl_];
        if((0x00 == lastByte) || (0xff == lastByte)) { return(0); } // ERROR
        }
    //  8) tl == 1 for non-secure, tl >= 1 for secure (tl = fl - 3 - il - bl)
    const uint8_t tl_ = getTl();
    if(secure_) { if(1 != tl_) { return(0); } } // ERROR
    else
        {
        if(tl_ < 1) { return(0); } // ERROR
        }

    // Set fl field to valid value as last action / side-effect.
    fl = fl_;

    // Return decoded header length including frame-length byte; body should immediately follow.
    return(hlifl); // SUCCESS!
    }

// Compute and return CRC for non-secure frames; 0 indicates an error.
// This is the value that should be at getTrailerOffset().
// Can be called after checkAndEncodeSmallFrameHeader() or checkAndDecodeSmallFrameHeader()
// to compute the correct 7-bit CRC value (which can never be 0x00 or 0xff);
// the equality check (on decode) or write (on encode) will then need to be done.
uint8_t SecurableFrameHeader::computeNonSecureFrameCRC(const uint8_t *const buf, uint8_t buflen) const
    {
    // Check that struct has been computed.
    if(isInvalid()) { return(0); } // ERROR
    // Check that buffer is at least large enough for all but the CRC byte itself.
    if(buflen < fl) { return(0); } // ERROR
    // Initialise CRC with 0x7f;
    uint8_t crc = 0x7f;
    const uint8_t *p = buf;
    // Include in calc all bytes up to but not including the trailer/CRC byte.
    for(uint8_t i = fl; i > 0; --i) { crc = crc7_5B_update(crc, *p++); }
    // Ensure 0x00 result is converted to avoid forbidden value.
    if(0 == crc) { crc = 0x80; }
    return(crc);
    }


// Pads plain-text in place prior to encryption with 32-byte fixed length padded output.
// Simple method that allows unpadding at receiver, does padding in place.
// Padded size is (ENC_BODY_SMALL_FIXED_CTEXT_SIZE) 32, maximum unpadded size is 31.
// All padding bytes after input text up to final byte are zero.
// Final byte gives number of zero bytes of padding added from plain-text to final byte itself [0,31].
// Returns padded size in bytes (32), or zero in case of error.
//
// Parameters:
//  * buf  buffer containing the plain-text; must be >= 32 bytes, never NULL
//  * datalen  unpadded data size at start of buf; if too large (>31) then this routine will fail (return 0)
uint8_t addPaddingTo32BTrailing0sAndPadCount(uint8_t *const buf, const uint8_t datalen)
    {
    if(NULL == buf) { return(0); } // ERROR
    if(datalen > ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE) { return(0); } // ERROR
    const uint8_t paddingZeros = ENC_BODY_SMALL_FIXED_CTEXT_SIZE - 1 - datalen;
    buf[ENC_BODY_SMALL_FIXED_CTEXT_SIZE - 1] = paddingZeros;
    memset(buf + datalen, 0, paddingZeros);
    return(ENC_BODY_SMALL_FIXED_CTEXT_SIZE); // DONE
    }

// Unpads plain-text in place prior to encryption with 32-byte fixed length padded output.
// Reverses/validates padding applied by addPaddingTo32BTrailing0sAndPadCount().
// Returns unpadded data length (at start of buffer) or 0xff in case of error.
//
// Parameters:
//  * buf  buffer containing the plain-text; must be >= 32 bytes, never NULL
uint8_t removePaddingTo32BTrailing0sAndPadCount(const uint8_t *const buf)
    {
    return(0xff); // FAIL FIXME
    }


    }


