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

#ifndef ARDUINO_LIB_OTRADIOLINK_SECUREABLEFRAMETYPE_H
#define ARDUINO_LIB_OTRADIOLINK_SECUREABLEFRAMETYPE_H

#include <stdint.h>
#include <OTV0p2Base.h>


namespace OTRadioLink
    {


    // Secureable (V0p2) messages.
    //
    // Based on 2015Q4 spec and successors:
    //     http://www.earth.org.uk/OpenTRV/stds/network/20151203-DRAFT-SecureBasicFrame.txt
    //     https://raw.githubusercontent.com/DamonHD/OpenTRV/master/standards/protocol/IoTCommsFrameFormat/SecureBasicFrame-V0.1-201601.txt
    //
    // This is primarily intended for local wireless communications
    // between sensors/actuators and a local hub/concentrator,
    // but should be robust enough to traverse public WANs in some circumstances.
    //
    // This can be used in a lightweight non-secure form,
    // or in a secured form,
    // with the security nominally including authentication and encryption,
    // with algorithms and parameters agreed in advance between leaf and hub,
    // and possibly varying by message type.
    // The initial supported auth/enc crypto mechanism (as of 2015Q4)
    // is AES-GCM with 128-bit pre-shared keys (and pre-shared IDs).
    //
    // The leading byte received indicates the length of frame that follows,
    // with the following byte indicating the frame type.
    // The leading frame-length byte allows efficient packet RX with many low-end radios.
    //
    // Frame types of 32/0x20 or above are reserved to OpenTRV to define.
    // Frame types < 32/0x20 (ignoring secure bit) are defined as
    // local-use-only and may be defined and used privately
    // (within a local radio network ~100m max or local wired network)
    // for any reasonable purpose providing use is generally consistent with
    // the rest of the protocol,
    // and providing that frames are not allowed to escape the local network.
    enum FrameType_Secureable
        {
        // No message should be type 0x00/0x01 (nor 0x7f/0xff).
        FTS_NONE                        = 0,
        FTS_INVALID_HIGH                = 0x7f,

        // Frame types < 32/0x20 (ignoring secure bit) are defined as local-use-only.
        FTS_MAX_LOCAL_TYPE              = 31,
        // Frame types of 32/0x20 or above are reserved to OpenTRV to define.
        FTS_MIN_PUBLIC_TYPE             = 32,

        // "I'm alive" / beacon message generally with empty (zero-length) message body.
        // Uses same crypto algorithm as 'O' frame type when secure.
        // This message can be sent asynchronously,
        // or after a short randomised delay in response to a broadcast liveness query.
        // ID should usually not be zero length (or any non-unique prefix)
        // as the computational burden on the receiver could be large.
        FTS_ALIVE                       = '!',

        // OpenTRV basic valve/sensor leaf-to-hub frame (secure if high-bit set).
        FTS_BasicSensorOrValve          = 'O', // 0x4f
        };

    // A high bit set (0x80) in the type indicates the secure message format variant.
    // The frame type is part of the authenticated data.
    const static uint8_t SECUREABLE_FRAME_TYPE_SEC_FLAG = 0x80;

    // For most small frames generally the maximum encrypted body size is 32.
    // That represents ~50% of the potential payload of a small (~63) byte frame.
    // Always padding to that size is simple and makes traffic analysis harder.
    // More sophisticated padding schemes are allowed to pad to smaller than 32,
    // eg to 16 bytes for 16-byte-block encryption mechanisms,
    // to conserve bandwidth.
    const static uint8_t ENC_BODY_SMALL_FIXED_CTEXT_SIZE = 32;

    // For fixed-size default encrypted bodies the maximum plaintext size is one less.
    const static uint8_t ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE =
        ENC_BODY_SMALL_FIXED_CTEXT_SIZE - 1;

    // Logical header for the secureable frame format.
    // Intended to be efficient to hold and work with in memory
    // and to convert to and from wire format.
    // All of this header should be (in wire format) authenticated for secure frames.
    // Note: fl = hl-1 + bl + tl = 3+il + bl + tl
    //
    // Frame format excluding logical leading length (fl) byte:
    // +------+--------+-----------------+----+--------------------+------------------+
    // | type | seqidl | ID [0,15] bytes | bl | body [0,254] bytes | trailer 1+ bytes |
    // +------+--------+-----------------+----+--------------------+------------------+
    struct SecurableFrameHeader
        {
        // Create an instance as an invalid frame header ready to start with seqNum==0.
        // Make the frame length 0 (which is invalid).
        // Make the sequence number 0xf so that (pre-)incrementing will make it 0.
        // Make the ID length 0.
        SecurableFrameHeader() : fl(0), seqIl(0xf0) { }

        // Returns true if the frame header in this struct instance is invalid.
        // This is only reliable if all manipulation of struct content
        // is by the member functions.
        bool isInvalid() const { return(0 == fl); }

        // Minimum possible frame size is 4, excluding fl byte.
        // Minimal frame (excluding logical leading length fl byte) is:
        //   type, seq/idlen, zero-length ID, bl, zero-length body, 1-byte trailer.
        // +------+--------+----+----------------+
        // | type | seqidl | bl | 1-byte-trailer |
        // +------+--------+----+----------------+
        static const uint8_t minFrameSize = 4;

        // Maximum (small) frame size is 63, excluding fl byte.
        static const uint8_t maxSmallFrameSize = 63;
        // Frame length excluding/after this byte [0,63]; zero indicates an invalid frame.
        // Appears first on the wire to support radio hardware packet handling.
        //     fl = hl-1 + bl + tl = 3+il + bl + tl
        // where hl header length, bl body length, tl trailer length
        // Should usually be set last to leave header clearly invalid until complete.
        uint8_t fl;

        // Frame type nominally from FrameType_Secureable (bits 0-6, [1,126]).
        // Top bit indicates secure frame if 1/true.
        uint8_t fType;
        bool isSecure() const { return(0 != (0x80 & fType)); }

        // Frame sequence number mod 16 [0,15] (bits 4 to 7) and ID length [0,15] (bits 0-3).
        //
        // Sequence number increments from 0, wraps at 15;
        // increment is skipped for repeat TXes used for noise immunity.
        // If a counter is used as part of (eg) security IV/nonce
        // then these 4 bits may be its least significant bits.
        uint8_t seqIl;
        // Get frame sequence number mod 16 [0,15].
        uint8_t getSeq() const { return((seqIl >> 4) & 0xf); }
        // Get il (ID length) [0,15].
        uint8_t getIl() const { return(seqIl & 0xf); }

        // ID bytes (0 implies anonymous, 1 or 2 typical domestic, length il).
        //
        // This is the first il bytes of the leaf's (64-bit) full ID.
        // Thus this is typically the ID of the sending sensor/valve/etc,
        // but may under some circumstances (depending on message type)
        // be the ID of the target/recipient.
        //
        // Initial and 'small frame' implementations are limited to 8 bytes of ID
        const static uint8_t maxIDLength = 8;
        uint8_t id[maxIDLength];

        // Get header length including the leading frame-length byte.
        inline uint8_t getHl() const { return(4 + getIl()); }

        // Body length including any padding [0,251] but generally << 60.
        uint8_t bl;
        // Compute the offset of the body from the start of the frame starting wth nominal fl byte.
        inline uint8_t getBodyOffset() const { return(getHl()); }

        // Compute tl (trailer length) [1,251]; must == 1 for insecure frame.
        // Other fields must be valid for this to return a valid answer.
        uint8_t getTl() const { return(fl - 3 - getIl() - bl); }
        // Compute the offset of the trailer from the start of the frame starting with nominal fl byte.
        uint8_t getTrailerOffset() const { return(4 + getIl() + bl); }


        // Check parameters for, and if valid then encode into the given buffer, the header for a small secureable frame.
        // The buffer starts with the fl frame length byte.
        //
        // Parameters:
        //  * buf  buffer to encode header to, of at least length buflen; never NULL
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
        uint8_t checkAndEncodeSmallFrameHeader(uint8_t *buf, uint8_t buflen,
                                               bool secure_, FrameType_Secureable fType_,
                                               uint8_t seqNum_,
                                               const uint8_t *id_, uint8_t il_,
                                               uint8_t bl_,
                                               uint8_t tl_);

        // Loads the node ID from the EEPROM or other non-volatile ID store.
        // Can be called before a call to checkAndEncodeSmallFrameHeader() with id_ == NULL.
        // Pads at end with 0xff if the EEPROM ID is shorter than the maximum 'short frame' ID.
        uint8_t loadIDFromEEPROM();

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
        // Returns number of bytes of decoded header including nominally-leading fl length byte; 0 in case of error.
        uint8_t checkAndDecodeSmallFrameHeader(const uint8_t *buf, uint8_t buflen);

        // Compute and return CRC for non-secure frames; 0 indicates an error.
        // This is the value that should be at getTrailerOffset() / offset fl.
        // Can be called after checkAndEncodeSmallFrameHeader() or checkAndDecodeSmallFrameHeader()
        // to compute the correct CRC value;
        // the equality check (on decode) or write (on encode) will then need to be done.
        // Note that the body must already be in place in the buffer.
        //
        // Parameters:
        //  * buf  buffer containing the entire frame except trailer/CRC; never NULL
        //  * buflen  available length in buf; if too small then this routine will fail (return 0)
        uint8_t computeNonSecureFrameCRC(const uint8_t *buf, uint8_t buflen) const;
        };


    // Encode entire non-secure small frame from header params and body.
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
    //  * id_ / il_  ID bytes (and length) to go in the header
    //  * body / bl_  body data (and length)
    uint8_t encodeNonsecureSmallFrame(uint8_t *buf, uint8_t buflen,
                                        FrameType_Secureable fType_,
                                        uint8_t seqNum_,
                                        const uint8_t *id_, uint8_t il_,
                                        const uint8_t *body, uint8_t bl_);

//        // Round up to next 16 multiple, eg for encryption that works in fixed-size blocks for input [0,240].
//        // Eg 0 -> 0, 1 -> 16, ... 16 -> 16, 17 -> 32 ...
//        // Undefined for values above 240.
//        uint8_t roundUpTo16s(uint8_t s) { return((s + 15) & 0xf0); }

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
    uint8_t addPaddingTo32BTrailing0sAndPadCount(uint8_t *buf, uint8_t datalen);

    // Unpads plain-text in place prior to encryption with 32-byte fixed length padded output.
    // Reverses/validates padding applied by addPaddingTo32BTrailing0sAndPadCount().
    // Returns unpadded data length (at start of buffer) or 0xff in case of error.
    //
    // Parameters:
    //  * buf  buffer containing the plain-text; must be >= 32 bytes, never NULL
    //
    // NOTE: mqy not check that all padding bytes are actually zero.
    uint8_t removePaddingTo32BTrailing0sAndPadCount(const uint8_t *buf);


    // Signature of pointer to basic fixed-size text encryption/authentication function.
    // (Suitable for type 'O' valve/sensor small frame for example.)
    // Can be fulfilled by AES-128-GCM for example
    // where:
    //   * textSize is 32 (or zero if plaintext is NULL)
    //   * keySize is 16
    //   * nonceSize is 12
    //   * tagSize is 16
    // The plain-text (and identical cipher-text) size is picked to be
    // a multiple of the cipher's block size, or zero,
    // which implies likely requirement for padding of the plain text.
    // Note that the authenticated text size is not fixed, ie is zero or more bytes.
    // Returns true on success, false on failure.
    typedef bool (*fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t)(void *state,
            const uint8_t *key, const uint8_t *iv,
            const uint8_t *authtext, uint8_t authtextSize,
            const uint8_t *plaintext,
            uint8_t *ciphertextOut, uint8_t *tagOut);

    // Signature of pointer to basic fixed-size text decryption/authentication function.
    // (Suitable for type 'O' valve/sensor small frame for example.)
    // Can be fulfilled by AES-128-GCM for example
    // where:
    //   * textSize is 32 (or zero if ciphertext is NULL)
    //   * keySize is 16
    //   * nonceSize is 12
    //   * tagSize is 16
    // The plain-text (and identical cipher-text) size is picked to be
    // a multiple of the cipher's block size, or zero,
    // which implies likely requirement for padding of the plain text.
    // Note that the authenticated text size is not fixed, ie is zero or more bytes.
    // Decrypts/authenticates the output of a fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t function.)
    // Returns true on success, false on failure.
    typedef bool (*fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t)(void *state,
            const uint8_t *key, const uint8_t *iv,
            const uint8_t *authtext, uint8_t authtextSize,
            const uint8_t *ciphertext, const uint8_t *tag,
            uint8_t *plaintextOut);


    // NULL basic fixed-size text 'encryption' function.
    // DOES NOT ENCRYPT OR AUTHENTICATE SO DO NOT USE IN PRODUCTION SYSTEMS.
    // Emulates some aspects of the process to test real implementations against,
    // and that some possible gross errors in the use of the crypto are absent.
    // Returns true on success, false on failure.
    //
    // Does not use state so that pointer may be NULL but all others must be non-NULL.
    // Copies the plaintext to the ciphertext.
    // Copies the nonce/IV to the tag and pads with trailing zeros.
    // The key is not used (though one must be supplied).
    bool fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL(void *state,
            const uint8_t *key, const uint8_t *iv,
            const uint8_t *authtext, uint8_t authtextSize,
            const uint8_t *plaintext,
            uint8_t *ciphertextOut, uint8_t *tagOut);

    // NULL basic fixed-size text 'decryption' function.
    // DOES NOT DECRYPT OR AUTHENTICATE SO DO NOT USE IN PRODUCTION SYSTEMS.
    // Emulates some aspects of the process to test real implementations against,
    // and that some possible gross errors in the use of the crypto are absent.
    // Returns true on success, false on failure.
    //
    // Does not use state so that pointer may be NULL but all others must be non-NULL.
    // Undoes/checks fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL().
    // Undoes/checks fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL().
    // Copies the ciphertext to the plaintext.
    // Verifies that the tag seems to have been constructed appropriately.
    bool fixed32BTextSize12BNonce16BTagSimpleDec_NULL_IMPL(void *state,
            const uint8_t *key, const uint8_t *iv,
            const uint8_t *authtext, uint8_t authtextSize,
            const uint8_t *ciphertext, const uint8_t *tag,
            uint8_t *plaintextOut);

    // Encode entire secure small frame from header params and body and crypto support.
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
    // Parameters:
    //  * buf  buffer to which is written the entire frame including trailer; never NULL
    //  * buflen  available length in buf; if too small then this routine will fail (return 0)
    //  * fType_  frame type (without secure bit) in range ]FTS_NONE,FTS_INVALID_HIGH[ ie exclusive
    //  * seqNum_  least-significant 4 bits are 4 lsbs of frame sequence number
    //  * id_ / il_  ID bytes (and length) to go in the header
    //  * body / bl_  body data (and length), before padding/encryption, no larger than ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE
    //  * iv  12-byte initialisation vector / nonce; never NULL
    //  * e  encryption function; never NULL
    //  * state  pointer to state for e, if required, else NULL
    //  * key  secret key; never NULL
    uint8_t encodeSecureSmallFrameRaw(uint8_t *buf, uint8_t buflen,
                                    FrameType_Secureable fType_,
                                    uint8_t seqNum_,
                                    const uint8_t *id_, uint8_t il_,
                                    const uint8_t *body, uint8_t bl_,
                                    const uint8_t *iv,
                                    fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t e,
                                    void *state, const uint8_t *key);

    // Decode entire secure small frame from raw frame bytes and crypto support.
    // This is a raw/partial impl that requires the IV/nonce to be supplied.
    // This uses fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t style encryption/authentication.
    // The matching encryption function should have been used for encoding this frame.
    // The crypto method may need to vary based on frame type,
    // and on negotiations between the participants in the communications.
    // Returns the total number of bytes read for the frame
    // (including, and with a value one higher than the first 'fl' bytes).
    // Returns zero in case of error, eg because authentication failed.
    //
    // Typical workflow:
    //   * decode the header alone to extract the ID and frame type
    //   * use those to select a candidate key, construct an iv/nonce
    //   * call this routine with that decoded header and the full buffer
    //     to authenticate and decrypt the frame.
    //
    // Parameters:
    //  * buf  buffer containing the entire frame including header and trailer; never NULL
    //  * buflen  available length in buf; if too small then this routine will fail (return 0)
    //  * sfh  decoded frame header; never NULL
    //  * decodedBodyOut  body, if any, will be decoded into this; never NULL
    //  * decodedBodyOutBuflen  size of decodedBodyOut to decode in to;
    //        if too small the routine will exist with an error (0)
    //  * decodedBodyOutSize  is set to the size of the decoded body in decodedBodyOut
    //  * iv  12-byte initialisation vector / nonce; never NULL
    //  * d  decryption function; never NULL
    //  * state  pointer to state for d, if required, else NULL
    //  * key  secret key; never NULL
    uint8_t decodeSecureSmallFrameRaw(const SecurableFrameHeader *sfh,
                                    const uint8_t *buf, uint8_t buflen,
                                    fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t d,
                                    void *state, const uint8_t *key, const uint8_t *iv,
                                    uint8_t *decryptedBodyOut, uint8_t decodedBodyOutBuflen, uint8_t &decodedBodyOutSize);



    // CONVENIENCE/BOILERPLATE METHODS

    // Create (insecure) Alive / beacon (FTS_ALIVE) frame with an empty body.
    // Returns number of bytes written to buffer, or 0 in case of error.
    // Note that the frame will be at least 4 + ID-length (up to maxIDLength) bytes,
    // so the buffer must be large enough to accommodate that.
    //  * sh  workspace for constructing header,
    //        also extracts the previous sequence number and increments before using,
    //        so that sending a series of (insecure) frames with the same sh
    //        will generate a contiguous stream of sequence numbers
    //        in the absense of errors
    //  * buf  buffer to which is written the entire frame including trailer; never NULL
    //  * buflen  available length in buf; if too small then this routine will fail (return 0)
    //  * id_ / il_  ID bytes (and length) to go in the header
    static const uint8_t generateInsecureBeaconBufSize = 4 + SecurableFrameHeader::maxIDLength;
    extern uint8_t generateInsecureBeacon(SecurableFrameHeader &sh,
                                        uint8_t *buf, uint8_t buflen,
                                        const uint8_t *id_, uint8_t il_);


    }


/* LIBRARY INTERDEPENDENCY POLICY FOR CRYPTO AND SECURE FRAMES

DHD20160106

I have been deciding how best to get at the encryption needed for secure frame support in
OTRadioLink/BASELIB without making the OTAESGCM and OTRadioLink libraries interdependent.

In particular I want to keep OTAESGCM as lightweight as possible and in no way dependent
on our V0p2 hardware support for example.

I also don’t want to force occasional developers against V0P2 code to load up lots of code
and libs that they may not even need (eg OTAESGCM) in some kind of DLL hell equivalent.

I propose to require pointers to enc/dec routines with the right signature to be made available
at run-time to the OTRadioLink frame RX/TX support routines, which means that only the top level
code that needs the secure frame functionality need link in the OTAESGCM lib, and other apps
get no dependency.  (Note, I’m relying on C++ type safety on signatures here ideally,
to completely minimise mutual dependencies, though there’s still the issue of retained/opaque
state to deal with, etc, etc.)

It also means that different enc/auth mechanisms can be selected at run-time or compile-time.
 */



#endif
