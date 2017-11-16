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
                           Deniz Erbilgin 2017
*/

/*
 * Radio message secureable frame types and related information.
 *
 * Based on 2015Q4 spec and successors:
 *     http://www.earth.org.uk/OpenTRV/stds/network/20151203-DRAFT-SecureBasicFrame.txt
 *     https://raw.githubusercontent.com/DamonHD/OpenTRV/master/standards/protocol/IoTCommsFrameFormat/SecureBasicFrame-*.txt
 */

#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#endif

#include <string.h>

#include "OTRadioLink_SecureableFrameType.h"

#include "OTV0P2BASE_CRC.h"
#include "OTV0P2BASE_EEPROM.h"


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
uint8_t SecurableFrameHeader::encodeHeader(
        OTBuf_t &buf,
        bool secure_, FrameType_Secureable fType_,
        uint8_t seqNum_,
        const OTBuf_t &id_,  // FIXME ScratchSpace can't be const!
        const uint8_t bl_,
        const uint8_t tl_)
    {
    // Buffer args locally + add constness.1
    uint8_t *const buffer = buf.buf;
    const uint8_t buflen = buf.bufsize;
    const uint8_t * const idbuf = id_.buf;
    const uint8_t idlen = id_.bufsize;

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
    // ID must be of a legitimate size.
    // A non-NULL pointer is a RAM source, else an EEPROM source.
    if(idlen > maxIDLength) { return(0); } // ERROR
    // Copy the ID length and bytes, and sequence number lsbs, to the header struct.
    seqIl = uint8_t(idlen | (seqNum_ << 4));
    if(idlen > 0)
      {
      // Copy in ID if not zero length, from RAM or EEPROM as appropriate.
      const bool idFromEEPROM = (NULL == idbuf);
      if(!idFromEEPROM) { memcpy(id, idbuf, idlen); }
      else
#ifdef ARDUINO_ARCH_AVR
          { eeprom_read_block(id, (uint8_t *)V0P2BASE_EE_START_ID, il_); }
#else
          { return(0); } // ERROR
#endif
      }
    // Header length including frame length byte.
    const uint8_t hlifl = 4 + idlen;
    // Error return if not enough space in buf for the complete encoded header.
    if(hlifl > buflen) { return(0); } // ERROR
    //  6) bl <= fl - 4 - il (body length; minimum of 4 bytes of other overhead)
    //  2) fl may be further constrained by system limits, typically to <= 63
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
        //  2) fl may be further constrained by system limits, typically to <= 63
        if(tl_ > maxSmallFrameSize+1 - hlifl - bl_) { return(0); } // ERROR
        }

    const uint8_t fl_ = hlifl-1 + bl_ + tl_;
    // Should not get here if true // if((fl_ > maxSmallFrameSize)) { return(0); } // ERROR

    // Write encoded header to buf starting with fl if buf is non-NULL.
    if(NULL != buffer)
        {
        buffer[0] = fl_;
        buffer[1] = fType;
        buffer[2] = seqIl;
        memcpy(buffer + 3, id, idlen);
        buffer[3 + idlen] = bl_;
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
//
// Returns number of bytes of decoded header
// including nominally-leading fl length byte;
// 0 in case of error.
uint8_t SecurableFrameHeader::decodeHeader(const uint8_t *const buf, uint8_t buflen)
    {
    // Make frame 'invalid' until everything is finished and checks out.
    fl = 0;

    // If buf is NULL or clearly too small to contain a valid header then return an error.
    if(NULL == buf) { return(0); } // ERROR
    if(buflen < 5) { return(0); } // ERROR


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
    // Capture the ID bytes, in the storage in the instance, if any.
    if(il_ > 0) { memcpy(id, buf+3, il_); }
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
    const uint8_t tl_ = fl_ - 3 - il_ - bl; // Same calc, but getTl() can't be used as fl not yet set.
    if(!secure_) { if(1 != tl_) { return(0); } } // ERROR
    else if(0 == tl_) { return(0); } // ERROR

    // Set fl field to valid value as last action / side-effect.
    fl = fl_;

    // Return decoded header length including frame-length byte; body should immediately follow.
    return(hlifl); // SUCCESS!
    }

// Compute and return CRC for non-secure frames; 0 indicates an error.
// This is the value that should be at getTrailerOffset() / offset fl.
// Can be called after checkAndEncodeSmallFrameHeader() or checkAndDecodeSmallFrameHeader()
// to compute the correct CRC value;
// the equality check (on decode) or write (on encode) will then need to be done.
// Note that the body must already be in place in the buffer.
//
// Parameters:
//  * buf     buffer containing the entire frame except trailer/CRC; never NULL XXX Frame should start with a leading length byte.
//  * buflen  available length in buf; if too small then this routine will fail (return 0)
uint8_t SecurableFrameHeader::computeNonSecureCRC(const uint8_t *const buf, uint8_t buflen) const
    {
    // Check that struct has been computed.
    if(isInvalid()) { return(0); } // ERROR
    // Check that buffer is at least large enough for all but the CRC byte itself.
    if(buflen < fl) { return(0); } // ERROR
    // Initialise CRC with 0x7f;
    uint8_t crc = 0x7f;
    const uint8_t *p = buf;
    // Include in calc all bytes up to but not including the trailer/CRC byte.
    for(uint8_t i = fl; i > 0; --i) { crc = OTV0P2BASE::crc7_5B_update(crc, *p++); }
    // Ensure 0x00 result is converted to avoid forbidden value.
    if(0 == crc) { crc = 0x80; }
    return(crc);
    }

// Compose (encode) entire non-secure small frame from header params, body and CRC trailer.
// Returns the total number of bytes written out for the frame
// (including, and with a value one higher than the first 'fl' bytes).
// Returns zero in case of error.
// The supplied buffer may have to be up to 64 bytes long.
//
// Parameters:
//  * buf  buffer to which is written the entire frame including trailer/CRC; never NULL
//  * buflen  available length in buf; if too small then this routine will fail (return 0)
//  * fType_  frame type (without secure bit) in range ]FTS_NONE,FTS_INVALID_HIGH[ ie exclusive
//  * seqNum_  least-significant 4 bits are 4 lsbs of frame sequence number
//  * id_ / il_  ID bytes (and length) to go in the header; NULL means take ID from EEPROM
//  * body / bl_  body data (and length)
uint8_t encodeNonsecureOnStack(
            OTEncodeData_T &fd,
            uint8_t seqNum_,
            const OTBuf_t &id_)
    {
    // Let checkAndEncodeSmallFrameHeader() validate buf and id_.
    // If necessary (bl_ > 0) body is validated below.
    OTBuf_t buf(fd.outbuf, fd.outbufSize);  // XXX
    const uint8_t hl = fd.sfh.encodeHeader(
                                buf,
                                false,
                                fd.fType, // Not secure.
                                seqNum_,
                                id_,
                                fd.ptextbufSize,
                                1); // 1-byte CRC trailer.
    // Fail if header encoding fails.
    if(0 == hl) { return(0); } // ERROR
    // Fail if buffer is not large enough to accommodate full frame.
    const uint8_t fl = fd.sfh.fl;
    if(fl >= fd.outbufSize) { return(0); } // ERROR
    // Copy in body, if any.
    if(fd.ptextbufSize > 0)
        {
        memcpy(fd.outbuf + fd.sfh.getBodyOffset(), fd.ptext, fd.ptextbufSize);
        }
    // Compute and write in the CRC trailer...
    const uint8_t crc = fd.sfh.computeNonSecureCRC(fd.outbuf, fd.outbufSize);
    fd.outbuf[fl] = crc;
    // Done.
    return(fl + 1);
    }

// Decode entire non-secure small frame from raw frame bytes support.
// Returns the total number of bytes read for the frame
// (including, and with a value one higher than the first 'fl' bytes).
// Returns zero in case of error, eg because the CRC check failed.
//
// Typical workflow:
//   * decode the header alone to extract the ID and frame type
//   * use the frame header's bl and getBodyOffset() to get the body and body length
//
// Parameters:
//  * buf  buffer containing the entire frame including header and trailer; never NULL
//  * buflen  available length in buf; if too small then this routine will fail (return 0)
//  * sfh  decoded frame header; never NULL
uint8_t decodeNonsecureOnStack(OTDecodeData_T &fd)
    {
    if(NULL == fd.ctext) { return(0); } // ERROR
    // Abort if header was not decoded properly.
    if(fd.sfh.isInvalid()) { return(0); } // ERROR
    // Abort if expected constraints for simple fixed-size secure frame are not met.
    const uint8_t fl = fd.sfh.fl;
    if(1 != fd.sfh.getTl()) { return(0); } // ERROR
    // Compute the expected CRC trailer...
    const uint8_t crc = fd.sfh.computeNonSecureCRC(fd.ctext, fd.ctextLen);
    if(0 == crc) { return(0); } // ERROR
    if(fd.ctext[fl] != crc) { return(0); } // ERROR
    // Done
    return(fl + 1);
    }


// Add specified small unsigned value to supplied counter value in place; false if failed.
// This will fail (returning false) if the counter would overflow, leaving it unchanged.
bool SimpleSecureFrame32or0BodyRXBase::msgcounteradd(uint8_t *const counter, const uint8_t delta)
    {
    if(0 == delta) { return(true); } // Optimisation: nothing to do.
    // Add to last byte, if it overflows ripple up the increment as needed,
    // but refuse if the counter would roll over.
    const uint8_t lsbyte = counter[fullMsgCtrBytes-1];
    const uint8_t bumped = lsbyte + delta;
    // If lsbyte does not wrap, as it won't much of the time, update it and return immediately.
    if(bumped > lsbyte) { counter[fullMsgCtrBytes-1] = bumped; return(true); }
    // Carry will need to ripple up, so check that that wouldn't cause an overflow.
    bool allFF = true;
    for(uint8_t i = 0; i < fullMsgCtrBytes-1; ++i) { if(0xff != counter[i]) { allFF = false; break; } }
    if(allFF) { return(false); }
    // Safe from overflow, set lsbyte and ripple up the carry as necessary.
    counter[fullMsgCtrBytes-1] = bumped;
    for(int8_t i = fullMsgCtrBytes-1; --i > 0; ) { if(0 != ++counter[i]) { break; } }
    // Success!
    return(true);
    }

// Encode entire secure small frame from header params and body and crypto support.
// Buffer for body must be large enough to allow padding to be applied IN PLACE.
// This is a raw/partial impl that requires the IV/nonce to be supplied.
// This uses fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t style encryption/authentication.
// The matching decryption function should be used for decoding/verifying.
// The crypto method may need to vary based on frame type,
// and on negotiations between the participants in the communications.
// Returns the total number of bytes written out for the frame
// (including, and with a value one higher than the first 'fl' bytes).
// Returns zero in case of error.
// The supplied buffer may have to be up to 64 bytes long.
//
// Note that the sequence number is taken from the 4 least significant bits
// of the message counter (at byte 6 in the nonce).
//
// Parameters:
//  * buf  buffer to which is written the entire frame including trailer; never NULL
//  * buflen  available length in buf; if too small then this routine will fail (return 0)
//  * fType_  frame type (without secure bit) in range ]FTS_NONE,FTS_INVALID_HIGH[ ie exclusive
//  * id_ / il_  ID bytes (and length) to go in the header; NULL means take ID from EEPROM
//  * body / bl_  body data (and length), before padding/encryption, no larger than ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE
//  * iv  12-byte initialisation vector / nonce; never NULL
//  * e  encryption function; never NULL
//  * state  pointer to state for e, if required, else NULL
//  * key  secret key; never NULL
// Note: the body is passed non-const and must be
// (nominally a multiple of) 32 bytes large enough to contain the body
// and have padding applied *in situ*.
//static constexpr uint8_t encodeSecureSmallFrameRawPadInPlace_scratch_usage = 1;
//static constexpr uint8_t encodeSecureSmallFrameRawPadInPlace_total_scratch_usage_OTAESGCM_2p0 =
//        workspaceRequred_GCM32B16BWithWorkspace_OTAESGCM_2p0
//        + encodeSecureSmallFrameRawPadInPlace_scratch_usage;
uint8_t SimpleSecureFrame32or0BodyTXBase::encodeRaw(
            OTEncodeData_T &fd,
            const OTBuf_t &id_,
            const uint8_t *iv,
            fixed32BTextSize12BNonce16BTagSimpleEnc_fn_t &e,
            const OTV0P2BASE::ScratchSpaceL &scratch,
            const uint8_t *const key)
    {
    if(NULL == key) { return(0); } // ERROR

    // buffer local variables/consts
    uint8_t * const buffer = fd.outbuf;
    const uint8_t bodylen = fd.ptextLen;

    // Capture possible (near) peak of stack usage, eg when called from ISR.
    OTV0P2BASE::MemoryChecks::recordIfMinSP();

    // Stop if unencrypted body is too big for this scheme.
    if(bodylen > ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE) { return(0); } // ERROR
    const uint8_t encryptedBodyLength = (0 == bodylen) ? 0 : ENC_BODY_SMALL_FIXED_CTEXT_SIZE;
    // Let checkAndEncodeSmallFrameHeader() validate buf and id_.
    // If necessary (bl_ > 0) body is validated below.
    const uint8_t seqNum_ = iv[11] & 0xf;

    OTBuf_t body(fd.ptext, fd.ptextbufSize);
    OTBuf_t buf(fd.outbuf, fd.outbufSize);
    const uint8_t hl = fd.sfh.encodeHeader(
                                    buf,
                                    true, fd.fType,
                                    seqNum_,
                                    id_,
                                    encryptedBodyLength,
                                    23); // 23-byte authentication trailer.
    // Fail if header encoding fails.
    if(0 == hl) { return(0); } // ERROR
    // Fail if buffer is not large enough to accommodate full frame.
    const uint8_t fl = fd.sfh.fl;
    if(fl >= fd.outbufSize) { return(0); } // ERROR
    // Pad body, if any, IN SITU.
    if(0 != bodylen)
        {
        if(0 == pad32BBuffer(fd.ptext, bodylen)) { return(0); } // ERROR
        }
    // Encrypt body (if any) from its now-padded buffer to the output buffer.
    // Insert the tag directly into the buffer (before the final byte).
    if(!e(scratch.buf, scratch.bufsize, key, iv, buffer, hl, (0 == bodylen) ? NULL : fd.ptext, buffer + hl, buffer + fl - 16)) { return(0); } // ERROR
    // Copy the counters part (last 6 bytes of) the nonce/IV into the trailer...
    memcpy(buffer + fl - 22, iv + 6, 6);
    // Set final trailer byte to indicate encryption type and format.
    buffer[fl] = 0x80;
    // Done.
    return(fl + 1);
    }


/**
 * @brief   Decode entire secure small frame from raw frame bytes and crypto support.
 *
 * This is a raw/partial impl that requires the IV/nonce to be supplied.
 *
 * This uses fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t style
 * encryption/authentication. The matching encryption function should have been
 * used for encoding this frame. The crypto method may need to vary based on
 * frame type, and on negotiations between the participants in the
 * communications.
 *
 * Also checks that the lsbs of the header sequence number match those of the
 * last 4 lsbs of byte 11 of the IV message counter.
 * - This check is nominally dependent on frame type and/or trailing tag
 *   byte/type.
 * - Note that this implies that the sequence number is not arbitrary but is
 *   derived (redundantly) from the IV.
 * - MAY NEED FIXING eg message counter moved to last IV byte or dependent and
 *   above.
 * The incoming message counter must be:
 * - strictly greater than the last authenticated message from this
 *   ID, to prevent replay attacks.
 * - quick and can also be done early to save processing energy.  XXX what is this supposed to mean?
 *
 * Typical workflow:
 * - decode the header to extract the ID and frame type
 *   e.g. fd.sfh.decodeHeader().
 * - use those to select a candidate key, construct an iv/nonce
 * - call this routine with that decoded header and the full buffer
 *   to authenticate and decrypt the frame.
 *
 * @param   fd: Common data required for decryption.
 *              - sfh: Frame header. Must already be decoded with decodeHeader.
 *              - inbuf: buffer containing the entire frame including header
 *                       and trailer. Must be large enough to hold the
 *                       encrypted frame. Never NULL.
 *              - ptext: body, if any, will be decoded into this. Should be
 *                       ptextLenMax bytes in length.  XXX is this approprate?
 *                       NULL if no plaintext is expected/wanted.
 *              - ptextLen: Set to the size of the decoded body in ptext.
 * @param   d: Decryption function.
 * @param   scratch: Scratch space. Size must be large enough to contain
 *                   decodeRaw_total_scratch_usage_OTAESGCM_3p0 bytes AND the
 *                   scratch space required by the decryption function `d`.
 * @param   key: 16-byte secret key. Never NULL.
 *          iv: 12-byte initialisation vector/nonce. Never NULL.
 * @retval  Returns the total number of bytes read for the frame including, and
 *          with a value one higher than the first 'fl' bytes).
 *          Returns zero in case of error, eg because authentication failed.
 *
 * @note    Uses a scratch space, allowing the stack usage to be more tightly controlled.
 */
uint8_t SimpleSecureFrame32or0BodyRXBase::decodeRaw(
            OTDecodeData_T &fd,
            fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &d,
            const OTV0P2BASE::ScratchSpaceL &scratch,
            const uint8_t *const key,
            const uint8_t *const iv)
    {
    // Scratch space for this function call alone (not called fns).
    constexpr uint8_t scratchSpaceNeededHere =
        decodeRaw_scratch_usage;
    if(scratchSpaceNeededHere > scratch.bufsize) { return(0); }
    // Create a new sub scratch space for callee.
    OTV0P2BASE::ScratchSpaceL subScratch(scratch, scratchSpaceNeededHere);

    if((NULL == fd.ctext) || (NULL == key) || (NULL == iv)) { return(0); } // ERROR

    const uint8_t *const buf = fd.ctext;
    const uint8_t buflen = buf[0] + 1;
    const SecurableFrameHeader &sfh = fd.sfh;

    // Abort if header was not decoded properly.
    if(sfh.isInvalid()) { return(0); } // ERROR
    // Abort if expected constraints for simple fixed-size secure frame are not met.
    const uint8_t fl = sfh.fl;
    if(fl >= buflen) { return(0); } // ERROR
    if(23 != sfh.getTl()) { return(0); } // ERROR
    if(0x80 != buf[fl]) { return(0); } // ERROR
    const uint8_t bl = sfh.bl;
    if((0 != bl) && (ENC_BODY_SMALL_FIXED_CTEXT_SIZE != bl)) { return(0); } // ERROR
    // Check that header sequence number lsbs match nonce counter 4 lsbs.
    if(sfh.getSeq() != (iv[11] & 0xf)) { return(0); } // ERROR
    // Note if plaintext is actually wanted/expected.
    const bool plaintextWanted = (NULL != fd.ptext);
    // Attempt to authenticate and decrypt.
    uint8_t * const decryptBuf = scratch.buf;
    //uint8_t decryptBuf[ENC_BODY_SMALL_FIXED_CTEXT_SIZE];
    if(!d(scratch.buf+scratchSpaceNeededHere, scratch.bufsize-scratchSpaceNeededHere,
                key, iv, buf, sfh.getHl(),
                (0 == bl) ? NULL : buf + sfh.getBodyOffset(), buf + fl - 16,
                decryptBuf)) { return(0); } // ERROR
    if(plaintextWanted && (0 != bl))
        {
        // Unpad the decrypted text in place.
        const uint8_t upbl = unpad32BBuffer(decryptBuf);
        if(upbl > ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE) { return(0); } // ERROR
        if(upbl > fd.ptextLenMax) { return(0); } // ERROR
        memcpy(fd.ptext, decryptBuf, upbl);
        fd.ptextLen = upbl;
        // TODO: optimise later if plaintext not required but ciphertext present.
        }
    // Ensure that decryptedBodyOutSize is not left initialised even if no frame body RXed/wanted.
    else { fd.ptextLen = 0; }
    // Done.
    return(fl + 1);
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
uint8_t SimpleSecureFrame32or0BodyTXBase::pad32BBuffer(uint8_t *const buf, const uint8_t datalen)
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
//
// NOTE: does not check that all padding bytes are actually zero.
uint8_t SimpleSecureFrame32or0BodyRXBase::unpad32BBuffer(const uint8_t *const buf)
    {
    const uint8_t paddingZeros = buf[ENC_BODY_SMALL_FIXED_CTEXT_SIZE - 1];
    if(paddingZeros > 31) { return(0); } // ERROR
    const uint8_t datalen = ENC_BODY_SMALL_FIXED_CTEXT_SIZE - 1 - paddingZeros;
    return(datalen); // FAIL FIXME
    }

// Check message counter for given ID, ie that it is high enough to be eligible for authenticating/processing.
// ID is full (8-byte) node ID; counter is full (6-byte) counter.
// Returns false if this counter value is not higher than the last received authenticated value.
bool SimpleSecureFrame32or0BodyRXBase::validateRXMsgCtr(const uint8_t *ID, const uint8_t *counter) const
    {
    // Validate args (rely on getLastRXMessageCounter() to validate ID).
    if(NULL == counter) { return(false); } // FAIL
    // Fetch the current counter; instant fail if not possible.
    uint8_t currentCounter[fullMsgCtrBytes];
    if(!getLastRXMsgCtr(ID, currentCounter)) { return(false); } // FAIL
    // New counter must be larger to be acceptable.
    return(msgcountercmp(counter, currentCounter) > 0);
    }

// NULL basic fixed-size text 'encryption' function.
// DOES NOT ENCRYPT OR AUTHENTICATE SO DO NOT USE IN PRODUCTION SYSTEMS.
// Emulates some aspects of the process to test real implementations against,
// and that some possible gross errors in the use of the crypto are absent.
// Returns true on success, false on failure.
//
// Does not use state so that pointer may be NULL but all others must be non-NULL except plaintext.
// Copies the plaintext to the ciphertext, unless plaintext is NULL.
// Copies the nonce/IV to the tag and pads with trailing zeros.
// The key is ignored (though one must be supplied).
bool fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL(
        uint8_t *const workspace, const size_t /*workspaceSize*/,
        const uint8_t *const key, const uint8_t *const iv,
        const uint8_t *const authtext, const uint8_t /*authtextSize*/,
        const uint8_t *const plaintext,
        uint8_t *const ciphertextOut, uint8_t *const tagOut)
    {
    // Does not use state, but checks that all other pointers are non-NULL.
    if((nullptr == workspace) || (nullptr == key) || (nullptr == iv) ||
       (nullptr == authtext) || (nullptr == ciphertextOut) || (nullptr == tagOut)) { return(false); } // ERROR
    // Copy the plaintext to the ciphertext, and the nonce to the tag padded with trailing zeros.
    if(nullptr != plaintext) { memcpy(ciphertextOut, plaintext, 32); }
    memcpy(tagOut, iv, 12);
    memset(tagOut+12, 0, 4);
    // Done.
    return(true);
    }

// NULL basic fixed-size text 'decryption' function.
// DOES NOT DECRYPT OR AUTHENTICATE SO DO NOT USE IN PRODUCTION SYSTEMS.
// Emulates some aspects of the process to test real implementations against,
// and that some possible gross errors in the use of the crypto are absent.
// Returns true on success, false on failure.
//
// Does not use state so that pointer may be NULL but all others must be non-NULL except ciphertext.
// Undoes/checks fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL().
// Copies the ciphertext to the plaintext, unless ciphertext is NULL.
// Verifies that the tag seems to have been constructed appropriately.
bool fixed32BTextSize12BNonce16BTagSimpleDec_NULL_IMPL(
        uint8_t *const workspace, const size_t /* workspaceSize */,
        const uint8_t *const key, const uint8_t *const iv,
        const uint8_t *const authtext, const uint8_t /*authtextSize*/,
        const uint8_t *const ciphertext, const uint8_t *const tag,
        uint8_t *const plaintextOut)
    {
    // Does not use state, but checks that all other pointers are non-NULL.
    if((nullptr == workspace) || (nullptr == key) || (nullptr == iv) ||
       (nullptr == authtext) || (nullptr == tag) || (nullptr == plaintextOut)) { return(false); } // ERROR
    // Verify that the first and last bytes of the tag look correct.
    if((tag[0] != iv[0]) || (0 != tag[15])) { return(false); } // ERROR
    // Copy the ciphertext to the plaintext.
    if(nullptr != ciphertext) { memcpy(plaintextOut, ciphertext, 32); }
    // Done.
    return(true);
    }


// CONVENIENCE/BOILERPLATE METHODS

// Create non-secure Alive / beacon (FTS_ALIVE) frame with an empty body.
// Returns number of bytes written to buffer, or 0 in case of error.
// Note that the frame will be at least 4 + ID-length (up to maxIDLength) bytes,
// so the buffer must be large enough to accommodate that.
//  * buf  buffer to which is written the entire frame including trailer; never NULL
//  * buflen  available length in buf; if too small then this routine will fail (return 0)
//  * seqNum_  least-significant 4 bits are 4 lsbs of frame sequence number
//  * id_ / il_  ID bytes (and length) to go in the header; NULL means take ID from EEPROM
uint8_t generateNonsecureBeacon(OTBuf_t &buf, const uint8_t seqNum_, const OTBuf_t &id_)
    {
    OTEncodeData_T fd(nullptr, 0, buf.buf, buf.bufsize);
    fd.fType = OTRadioLink::FTS_ALIVE;

    // "I'm Alive!" / beacon message.
    return(encodeNonsecureOnStack(fd,
                                    seqNum_,
                                    id_));
    }

/**
 * @brief   Create a generic secure small frame with an optional encrypted body for transmission.
 *
 * The IV is constructed from the node ID (local from EEPROM, or as supplied)
 * and the primary TX message counter (which is incremented).
 * Note that the frame will be 27 + ID-length (up to maxIDLength) + body-length bytes,
 * so the buffer must be large enough to accommodate that.
 *
 * @param   fd: Common data required for encryption.
 *              - ptext may be null for frames containing no encrypted body.
 *              - ctext must never be null.  // XXX
 *              - ftype must be set with a valid frame type before calling this function.
 * @param   il_: ID length for the header; ID is local node ID from EEPROM or other pre-supplied ID
 * @param   e: Encryption function.
 * @param   scratch: Scratch space. Size must be larger than
 *                   encode_total_scratch_usage_OTAESGCM_2p0 bytes.
 * @param   key: 16-byte secret key; never NULL
 * @retval  Number of bytes written to buffer, or 0 in case of error.
 * *note    Uses a scratch space, allowing the stack usage to be more tightly controlled.
 */
// FIXME UNTESTED!
uint8_t SimpleSecureFrame32or0BodyTXBase::encode(
            OTEncodeData_T &fd,
            uint8_t il_,
            fixed32BTextSize12BNonce16BTagSimpleEnc_fn_t  &e,
            OTV0P2BASE::ScratchSpaceL &scratch,
            const uint8_t *const key)
    {
    constexpr uint8_t IV_size = 12;
    static_assert(encode_scratch_usage == IV_size + OTV0P2BASE::OpenTRV_Node_ID_Bytes, "self-use scratch size wrong");
    static_assert(encode_scratch_usage < encode_total_scratch_usage_OTAESGCM_2p0, "scratch size calc wrong");
    if(scratch.bufsize < encode_total_scratch_usage_OTAESGCM_2p0) { return(0); } // ERROR

    if((fd.fType >= FTS_INVALID_HIGH) || (fd.fType == FTS_NONE)) { return(0); } // FAIL
    // iv at start of scratch space
    uint8_t *const iv = scratch.buf; // uint8_t iv[IV_size];
    if(!computeIVForTX(iv)) { return(0); }

    // XXX This is the special case! Will potentially need an extra 8 bytes to store the id.
    // If ID is short then we can cheat by reusing start of IV, else fetch again explicitly...
    const bool longID = (il_ > 6);
    uint8_t *const id = iv + IV_size;  // FIXME
    if(longID && !getTXID(id)) { return(0); } // FAIL

    const OTBuf_t txID((longID ? id : iv), sizeof((longID ? id : iv)));
    return(encodeRaw(fd, txID, iv, e, scratch, key));
    }

// Create simple 'O' (FTS_BasicSensorOrValve) frame with an optional stats section for transmission.
// Returns number of bytes written to buffer, or 0 in case of error.
// The IV is constructed from the node ID (built-in from EEPROM or as supplied)
// and the primary TX message counter (which is incremented).
// Note that the frame will be 27 + ID-length (up to maxIDLength) + body-length bytes,
// so the buffer must be large enough to accommodate that.
//  * buf  buffer to which is written the entire frame including trailer; never NULL
//  * buflen  available length in buf; if too small then this routine will fail (return 0)
//  * valvePC  percentage valve is open or 0x7f if no valve to report on
//  * statsJSON  '\0'-terminated {} JSON stats, or NULL if none.
//  * il_  ID length for the header; ID is local node ID from EEPROM or other pre-supplied ID, may be limited to a 6-byte prefix
//  * key  16-byte secret key; never NULL
// NOTE: THIS API IS LIABLE TO CHANGE
uint8_t SimpleSecureFrame32or0BodyTXBase::encodeValveFrame(
                                            OTEncodeData_T &fd,
                                            uint8_t il_,
                                            uint8_t valvePC,
                                            fixed32BTextSize12BNonce16BTagSimpleEnc_fn_t &e,
                                            const OTV0P2BASE::ScratchSpaceL &scratch,
                                            const uint8_t *const key)
{
    constexpr uint8_t IV_size = 12;
    static_assert(encodeValveFrame_scratch_usage == IV_size, "self-use scratch size wrong");
    static_assert(encodeValveFrame_scratch_usage < encodeValveFrame_total_scratch_usage_OTAESGCM_2p0, "scratch size calc wrong");
    if(scratch.bufsize < encodeValveFrame_total_scratch_usage_OTAESGCM_2p0) { return(0); } // ERROR

    // buffer args and consts
    uint8_t * const ptext = fd.ptext;

    // iv at start of scratch space
    uint8_t *const iv = scratch.buf; // uint8_t iv[IV_size];
    if(!computeIVForTX(iv)) { return(0); }

    const char *const statsJSON = (const char *const)&ptext[2];
    const bool hasStats = (NULL != ptext) && ('{' == statsJSON[0]);
    const size_t slp1 = hasStats ? strlen(statsJSON) : 1; // Stats length including trailing '}' (not sent).
    if(slp1 > ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE-1) { return(0); } // ERROR
    const uint8_t statslen = (uint8_t)(slp1 - 1); // Drop trailing '}' implicitly.
    ptext[0] = (valvePC <= 100) ? valvePC : 0x7f;
    ptext[1] = hasStats ? 0x10 : 0; // Indicate presence of stats.

    // Create a new scratchspace from the old one in order to pass on.
    const OTV0P2BASE::ScratchSpaceL subscratch(scratch, encodeValveFrame_scratch_usage);
    if(il_ > 6) { return(0); } // ERROR: cannot supply that much of ID easily.

    // Create id buffer
    const OTBuf_t id(iv, il_);
    fd.ptextLen = (hasStats ? 2+statslen : 2); // Note: callee will pad beyond this.
    fd.fType = OTRadioLink::FTS_BasicSensorOrValve;

    // note: id and iv are both passed in here despite pointing at the same
    //       place. They may not necessarily be the same when encodeRaw is
    //       called elsewhere.
    return(encodeRaw(fd, id, iv, e, subscratch, key));
}

/**
 * @brief XXX
 *
 * Passed a candidate node/counterparty ID derived from:
 * - The frame ID in the incoming header.
 * - Possible other adjustments, such as forcing bit values for reverse
 *   flows.
 * The expanded ID must be at least length 6 for 'O' / 0x80 style enc/auth.
 *
 * This routine constructs an IV from this expanded ID and other
 * information in the header and then returns the result of calling
 * decodeRaw().
 *
 * If several candidate nodes share the ID prefix in the frame header (in
 * the extreme case with a zero-length header ID for an anonymous frame)
 * then they may all have to be tested in turn until one succeeds.
 *
 * Generally, this should be called AFTER checking that the aggregate RXed
 * message counter is higher than for the last successful receive for this
 * node and flow direction. On success, those message counters should be
 * updated to the new values to prevent replay attacks.
 *
 * TO AVOID REPLAY ATTACKS:
 * - Verify the counter is higher than any previous authed message from
 *   this sender
 * - Update the RX message counter after a successful auth with this
 *   routine.
 *
 * @param   fd: Must contain a validated header, in addition to the
 *              conditions outlined in decode().
 * @param   adjID: Adjusted candidate ID based on the received ID in the
 *                 header. Must be able to hold >= 6 bytes. Never NULL.
 * XXX Other params
 * @retval  Total number of bytes read for the frame (including, and with a
 *          value one higher than the first 'fl' bytes).
 *          Returns zero in case of error, eg because authentication failed.
 *
 * @note    Uses a scratch space, allowing the stack usage to be more tightly controlled.
 */
uint8_t SimpleSecureFrame32or0BodyRXBase::_decodeFromID(
                                OTDecodeData_T &fd,
                                fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &d,
                                const OTBuf_t adjID,
                                OTV0P2BASE::ScratchSpaceL &scratch, const uint8_t *const key)
    {
    // Scratch space for this function call alone (not called fns).
    constexpr uint8_t scratchSpaceNeededHere =
        _decodeFromID_scratch_usage;
    if(scratchSpaceNeededHere > scratch.bufsize) { return(0); }
    // Create a new sub scratch space for callee.
    OTV0P2BASE::ScratchSpaceL subScratch(scratch, scratchSpaceNeededHere);

    if(adjID.bufsize < 6) { return(0); } // ERROR

    const uint8_t buflen = fd.ctext[0] + 1;
    if(fd.sfh.getTrailerOffset() + 6 > buflen) { return(0); } // ERROR

    // Construct IV from supplied (possibly adjusted) ID
    // + counters from (start of) trailer.
    uint8_t *iv = scratch.buf;
    memcpy(iv, adjID.buf, 6);
    memcpy(iv + 6, fd.ctext + fd.sfh.getTrailerOffset(), SimpleSecureFrame32or0BodyBase::fullMsgCtrBytes);
    // Now do actual decrypt/auth.
    return(decodeRaw(fd, d, subScratch, key, iv));
    }


/**
 * @brief   Decode a structurally correct secure small frame.
 *          THIS IS THE PREFERRED ENTRY POINT FOR DECODING AND RECEIVING
 *          SECURE FRAMES.
 *
 * From a structurally correct secure frame, looks up the ID, checks the
 * message counter, decodes, and updates the counter if successful.
 * (Pre-filtering by type and ID and message counter may already have
 * happened.)
 * Note that this is for frames being send from the ID in the header,
 * not for lightweight return traffic to the specified ID.
 *
 * @param   fd: Common data required for decryption. inbuf and ptext must
 *              never be null.
 * @param   d: Decryption function.
 * @param   scratch: Scratch space. Size must be large enough to contain
 *                   decode_total_scratch_usage_OTAESGCM_3p0 bytes AND the
 *                   scratch space required by the decryption function `d`.
 * @param   key: 16-byte secret key. Never NULL.
 * @param   firstIDMatchOnly: IGNORED! If the 'firstMatchIDOnly' is true
 *              (the default) then this only checks the first ID prefix
 *              match found if any, else all possible entries may be tried
 *              depending on the implementation  and, for example,
 *              time/resource limits.
 * @retval  The total number of bytes read for the frame, including, and
 *          with a value one higher than the first 'fl' bytes. XXX what does the last bit mean?
 *          - Returns zero in case of error, eg because authentication failed
 *            or this is a duplicate message.
 *          - If this returns >=1 then the frame is authenticated, and the
 *            decrypted body is available if present and a buffer was
 *            provided.  XXX What happens when buffer not provided?
 *
 * @note    Uses a scratch space, allowing the stack usage to be more tightly controlled.
 */
uint8_t SimpleSecureFrame32or0BodyRXBase::decode(
            OTDecodeData_T &fd,
            fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &d,
            OTV0P2BASE::ScratchSpaceL &scratch,
            const uint8_t *const key,
            bool /*firstIDMatchOnly*/)
    {
    // Scratch space for this function call alone (not called fns).
    constexpr uint8_t scratchSpaceNeededHere =
        decode_scratch_usage;
    if(scratchSpaceNeededHere > scratch.bufsize) { return(0); }
    // Create a new sub scratch space for callee.
    OTV0P2BASE::ScratchSpaceL subScratch(scratch, scratchSpaceNeededHere);

    // Rely on _decodeSecureSmallFrameFromID() for validation of items
    // not directly needed here.
    if(nullptr == fd.ctext) { return(0); } // ERROR
    // Abort if header was not decoded properly.
    if(fd.sfh.isInvalid()) { return(0); } // ERROR
    #if 0
    // FIXME Why not checked?
    // Abort if frame is not secure.
    if(sfh.isSecure()) { return(0); } // ERROR
    #endif
    // Abort if trailer not large enough to extract message counter from
    // safely (and not expected size/flavour).
    if(23 != fd.sfh.getTl()) { return(0); } // ERROR
    // Look up the full node ID of the sender in the associations table.
    // NOTE: this only tries the first match, ignoring firstIDMatchOnly.
    // Use start of scratch space. This buffer should not be visible
    // outside the decode stack (e.g. should not be part of fd).
    OTBuf_t senderNodeID(scratch.buf, OTV0P2BASE::OpenTRV_Node_ID_Bytes);
    const int8_t index = _getNextMatchingNodeID(0, &fd.sfh, senderNodeID.buf);
    if(index < 0) { return(0); } // ERROR
    // Extract the message counter and validate it
    // (that it is higher than previously seen)...
    // Append to scratch space, after node id.
    uint8_t * const messageCounter = scratch.buf + OTV0P2BASE::OpenTRV_Node_ID_Bytes;
    // Assume counter positioning as for 0x80 type trailer,
    // ie 6 bytes at start of trailer.
    // Destination and source known large enough for copy to be safe.
    memcpy(messageCounter,
           fd.ctext + fd.sfh.getTrailerOffset(),
           SimpleSecureFrame32or0BodyBase::fullMsgCtrBytes);
    if(!validateRXMsgCtr(senderNodeID.buf, messageCounter)) { return(0); } // ERROR

    // Now attempt to decrypt.
    // Assumed no need to 'adjust' ID for this form of RX.
    const uint8_t decodeResult = _decodeFromID( fd, d, senderNodeID, subScratch, key);
    if(0 == decodeResult) { return(0); } // ERROR
    // Successfully decoded: update the RX message counter to avoid duplicates/replays.
    if(!authAndUpdateRXMsgCtr(senderNodeID.buf, messageCounter)) { return(0); } // ERROR
    // Success: copy sender ID to output buffer (if non-NULL) as last action.
    memcpy(fd.id, senderNodeID.buf, OTV0P2BASE::OpenTRV_Node_ID_Bytes);
    return(decodeResult);
    }

}
