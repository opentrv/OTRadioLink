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
        //
        // When received by a leaf node it identifies itself physically if possible,
        // eg through any local UI such as flashing lights or tactile actuators,
        // for example to help a field technician ID a device and verify comms.
        // Devices may refuse to do this (or limit their response)
        // for a number of reasons including minimising the scope for misuse.
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

    // Standard length of ID to transmit in a secure frame.
    // Long enough to make risk of non-unique prefixes very small even for large deployments.
    // Short enough to produce an encrypted frame shorter than the maximum permitted.
    const static uint8_t ENC_BODY_DEFAULT_ID_BYTES = 4;

    // Logical header for the secureable frame format.
    // Intended to be efficient to hold and work with in memory
    // and to convert to and from wire format.
    // All of this header should be (in wire format) authenticated for secure frames.
    // Note: fl = hl-1 + bl + tl = 3+il + bl + tl
    //
    // Frame format excluding logical leading length (fl) byte:
    // +------+--------+-----------------+----+--------------------+------------------+
    // | type | seqidl | ID [0,15] bytes | bl | body [0,251] bytes | trailer 1+ bytes |
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

        // Maximum small frame body size is maximum frame size minus 4, excluding fl byte.
        // This maximum size is only achieved with non-secure frames with zero-length ID.
        static const uint8_t maxSmallFrameBodySize = maxSmallFrameSize - 4;
        // Body length including any padding [0,251] but generally << 60.
        uint8_t bl;
        // Compute the offset of the body from the start of the frame starting with nominal fl byte.
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
        //  * id_  source of ID bytes, at least il_ long; NULL means fill from EEPROM
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
    uint8_t encodeNonsecureSmallFrame(uint8_t *buf, uint8_t buflen,
                                        FrameType_Secureable fType_,
                                        uint8_t seqNum_,
                                        const uint8_t *id_, uint8_t il_,
                                        const uint8_t *body, uint8_t bl_);

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
    uint8_t decodeNonsecureSmallFrameRaw(const SecurableFrameHeader *sfh,
                                         const uint8_t *buf, uint8_t buflen);

//        // Round up to next 16 multiple, eg for encryption that works in fixed-size blocks for input [0,240].
//        // Eg 0 -> 0, 1 -> 16, ... 16 -> 16, 17 -> 32 ...
//        // Undefined for values above 240.
//        uint8_t roundUpTo16s(uint8_t s) { return((s + 15) & 0xf0); }


    // Base class for simple implementations that supports 0 or 32 byte encrypted body sections.
    // This wraps up any necessary state, persistent and ephemeral, such as message counters.
    // Some implementations make sense only as singletons,
    // eg because they store state at fixed locations in EEPROM.
    // It is possible to provide implementations not tied to any particular hardware architecture.
    class SimpleSecureFrame32or0BodyBase
        {
        };

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
    // Does not use state so that pointer may be NULL but all others must be non-NULL except plaintext.
    // Copies the plaintext to the ciphertext, unless plaintext is NULL.
    // Copies the nonce/IV to the tag and pads with trailing zeros.
    // The key is ignored (though one must be supplied).
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
    // Does not use state so that pointer may be NULL but all others must be non-NULL except ciphertext.
    // Undoes/checks fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL().
    // Copies the ciphertext to the plaintext, unless ciphertext is NULL.
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
    uint8_t encodeSecureSmallFrameRaw(uint8_t *buf, uint8_t buflen,
                                    FrameType_Secureable fType_,
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
    // Also checks (nominally dependent on frame type and/or trailing tag byte/type) that
    // the header sequence number lsbs matches the IV message counter 4 lsbs (in byte 11),
    // ie the sequence number is not arbitrary but is derived (redundantly) from the IV.
    // (MAY NEED FIXING eg message counter moved to last IV byte or dependent and above.)
    //
    // Typical workflow:
    //   * decode the header alone to extract the ID and frame type
    //   * use those to select a candidate key, construct an iv/nonce
    //   * call this routine with that decoded header and the full buffer
    //     to authenticate and decrypt the frame.
    //
    // Note extra checks to be done:
    //   * the incoming message counter must be strictly greater than
    //     the last last authenticated message from this ID
    //     to prevent replay attacks;
    //     is quick and can also be done early to save processing energy.
    //
    // Parameters:
    //  * buf  buffer containing the entire frame including header and trailer; never NULL
    //  * buflen  available length in buf; if too small then this routine will fail (return 0)
    //  * sfh  decoded frame header; never NULL
    //  * decryptedBodyOut  body, if any, will be decoded into this;
    //        can be NULL if no plaintext is expected/wanted
    //  * decryptedBodyOutBuflen  size of decodedBodyOut to decode in to;
    //        if too small the routine will exist with an error (0)
    //  * decryptedBodyOutSize  is set to the size of the decoded body in decodedBodyOut
    //  * iv  12-byte initialisation vector / nonce; never NULL
    //  * d  decryption function; never NULL
    //  * state  pointer to state for d, if required, else NULL
    //  * key  secret key; never NULL
    uint8_t decodeSecureSmallFrameRaw(const SecurableFrameHeader *sfh,
                                    const uint8_t *buf, uint8_t buflen,
                                    fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t d,
                                    void *state, const uint8_t *key, const uint8_t *iv,
                                    uint8_t *decryptedBodyOut, uint8_t decryptedBodyOutBuflen, uint8_t &decryptedBodyOutSize);

    // As for decodeSecureSmallFrameRaw() but passed a candidate node/counterparty ID
    // derived from the frame ID in the incoming header,
    // plus possible other adjustments such has forcing bit values for reverse flows.
    // This routine constructs an IV from this expanded ID
    // (which must be at least length 6 for 'O' / 0x80 style enc/auth)
    // and other information in the header
    // and then returns the result of calling decodeSecureSmallFrameRaw().
    //
    // If several candidate nodes share the ID prefix in the frame header
    // (in the extreme case with a zero-length header ID for an anonymous frame)
    // then they may all have to be tested in turn until one succeeds.
    //
    // Generally a call to this should be done AFTER checking that
    // the aggregate RXed message counter is higher than for the last successful receive
    // (for this node and flow direction)
    // and after a success those message counters should be updated
    // (which may involve more than a simple increment)
    // to the new values to prevent replay attacks.
    //
    //   * adjID / adjIDLen  adjusted candidate ID (never NULL)
    //         and available length (must be >= 6)
    //         based on the received ID in (the already structurally validated) header
    uint8_t decodeSecureSmallFrameFromID(const SecurableFrameHeader *sfh,
                                    const uint8_t *buf, uint8_t buflen,
                                    fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t d,
                                    const uint8_t *adjID, uint8_t adjIDLen,
                                    void *state, const uint8_t *key,
                                    uint8_t *decryptedBodyOut, uint8_t decryptedBodyOutBuflen, uint8_t &decryptedBodyOutSize);

    // Design notes on use of message counters vs non-volatile storage life, eg for ATMega328P.
    //
    // Note that the message counter is designed to:
    //  a) prevent reuse of IVs, which can fatally weaken the cipher,
    //  b) avoid replay attacks.
    //
    // The implementation on both TX and RX sides should:
    //  a) allow nominally 10 years' life from the non-volatile store and thus the unit,
    //  b) be resistant to (for example) deliberate power-cycling during update,
    //  c) random EEPROM byte failures.
    //
    // Some assumptions:
    //  a) aiming for 10 years' continuous product life at transmitters and receivers,
    //  b) around one TX per sensor/valve node per 4 minutes,
    //  c) ~100k full erase/write cycles per EEPROM byte (partial writes can be cheaper), as ATmega328P.
    //
    // 100k updates over 10Y implies ~10k/y or about 1 per hour;
    // that is about one full EEPROM erase/write per 15 messages at one message per 4 minutes.
    //
    // Check message counter for given ID, ie that it is high enough to be worth authenticating.
    // ID is full (8-byte) node ID; counter is full (6-byte) counter.
    // Returns false if this counter value is not higher than the last received authenticated value.
    bool validateRXMessageCount(const uint8_t *ID, const uint8_t *counter);
    // Update persistent message counter for received frame AFTER successful authentication.
    // ID is full (8-byte) node ID; counter is full (6-byte) counter.
    // Returns false on failure, eg if message counter is not higher than the previous value for this node.
    // The implementation should allow several years of life typical message rates (see above).
    // The implementation should be robust in the face of power failures / reboots, accidental or malicious,
    // not allowing replays nor other cryptographic attacks, nor forcing node dissociation.
    // Must only be called once the RXed message has passed authentication.
    bool updateRXMessageCountAfterAuthentication(const uint8_t *ID, const uint8_t *counter);

    // Load the raw form of the persistent reboot/restart message counter from EEPROM into the supplied array.
    // Deals with inversion, but does not interpret the data or check CRCs etc.
    // Separates the EEPROM access from the data interpretation to simplify unit testing.
    // Buffer must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
    // Not ISR-safe.
    void loadRaw3BytePersistentTXRestartCounterFromEEPROM(uint8_t *loadBuf);
    // Interpret RAM copy of persistent reboot/restart message counter, ie 3 MSBs of message counter; returns false on failure.
    // Combines results from primary and secondary as appropriate,
    // for example to recover from message counter corruption due to a failure during write.
    // TODO: should still do more to (for example) rewrite failed copy for resilience against multiple write failures.
    // Deals with inversion and checksum checking.
    // Input buffer (loadBuf) must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
    // Output buffer (buf) must be 3 bytes long.
    // Will report failure when count is all 0xff values.
    static const uint8_t primaryPeristentTXMessageRestartCounterBytes = 3;
    bool read3BytePersistentTXRestartCounter(const uint8_t *loadBuf, uint8_t *buf);
    // Increment RAM copy of persistent reboot/restart message counter; returns false on failure.
    // Will refuse to increment such that the top byte overflows, ie when already at 0xff.
    // Updates the CRC.
    // Input/output buffer (loadBuf) must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
    bool increment3BytePersistentTXRestartCounter(uint8_t *loadBuf);
    // Get the 3 bytes of persistent reboot/restart message counter, ie 3 MSBs of message counter; returns false on failure.
    // Combines results from primary and secondary as appropriate.
    // Deals with inversion and checksum checking.
    // Output buffer (buf) must be 3 bytes long.
    bool get3BytePersistentTXRestartCounter(uint8_t *buf);

    // Reset the persistent reboot/restart message counter in EEPROM; returns false on failure.
    // TO BE USED WITH EXTREME CAUTION: reusing the message counts and resulting IVs
    // destroys the security of the cipher.
    // Probably only sensible to call this when changing either the ID or the key (or both).
    // This can reset the restart counter to all zeros (erasing the underlying EEPROM bytes),
    // or (default) reset only the most significant bits to zero (preserving device life)
    // but inject entropy into the least significant bits to reduce risk value/IV reuse in error.
    // If called with false then interrupts should not be blocked to allow entropy gathering,
    // and counter is guaranteed to be non-zero.
    bool resetRaw3BytePersistentTXRestartCounterInEEPROM(bool allZeros = false);
    // Increment EEPROM copy of persistent reboot/restart message counter; returns false on failure.
    // Will refuse to increment such that the top byte overflows, ie when already at 0xff.
    // TO BE USED WITH EXTREME CAUTION: calling this unnecessarily will shorten life before needing to change ID/key.
    bool increment3BytePersistentTXRestartCounter();

    // Get primary (semi-persistent) message counter for TX from an OpenTRV leaf under its own ID.
    // This counter increases monotonically
    // (and so may provide a sequence number)
    // and is designed never to repeat a value
    // which is very important for AES-GCM in particular
    // as reuse of an IV (that includes this counter)
    // badly undermines security of particular key.
    // This counter may be shared across TXes with multiple keys if need be,
    // though would normally we only associated with one key.
    // This counter can can be reset if associated with entirely new keys.
    // The top 3 of the 6 bytes of the counter are persisted in non-volatile storage
    // and incremented after a reboot/restart
    // and if the lower 3 bytes overflow into them.
    // Some of the lest significant bits of the lower three (ephemeral) bytes
    // may be initialised with entropy over a restart
    // to help make 'cracking' the key harder
    // and to reduce the chance of reuse of IVs
    // even in the face of hardware or software error.
    // When this counter reaches 0xffffffffffff then no more messages can be sent
    // until new keys are shared and the counter is reset.
    static const uint8_t primaryPeristentTXMessageCounterBytes = 6;
    // Fills the supplied 6-byte array with the monotonically-increasing primary TX counter.
    // Returns true on success; false on failure for example because the counter has reached its maximum value.
    // Highest-index bytes in the array increment fastest.
    // Not ISR-safe.
    bool getPrimarySecure6BytePersistentTXMessageCounter(uint8_t *buf);

    // Fill in 12-byte IV for 'O'-style (0x80) AESGCM security for a frame to TX.
    // This uses the local node ID as-is for the first 6 bytes.
    // This uses and increments the primary message counter for the last 6 bytes.
    // Returns true on success, false on failure eg due to message counter generation failure.
    bool compute12ByteIDAndCounterIVForTX(uint8_t *ivBuf);


    // CONVENIENCE/BOILERPLATE METHODS

    // Create non-secure Alive / beacon (FTS_ALIVE) frame with an empty body.
    // Returns number of bytes written to buffer, or 0 in case of error.
    // Note that the frame will be 5 + ID-length (up to maxIDLength) bytes,
    // so the buffer must be large enough to accommodate that.
    //  * buf  buffer to which is written the entire frame including trailer; never NULL
    //  * buflen  available length in buf; if too small then this routine will fail (return 0)
    //  * seqNum_  least-significant 4 bits are 4 lsbs of frame sequence number
    //  * id_ / il_  ID bytes (and length) to go in the header; NULL means take ID from EEPROM
    static const uint8_t generateNonsecureBeaconMaxBufSize = 5 + SecurableFrameHeader::maxIDLength;
    uint8_t generateNonsecureBeacon(uint8_t *buf, uint8_t buflen,
                                    const uint8_t seqNum_,
                                    const uint8_t *id_, uint8_t il_);

    // Create secure Alive / beacon (FTS_ALIVE) frame with an empty body.
    // Returns number of bytes written to buffer, or 0 in case of error.
    // Note that the frame will be 27 + ID-length (up to maxIDLength) bytes,
    // so the buffer must be large enough to accommodate that.
    //  * buf  buffer to which is written the entire frame including trailer; never NULL
    //  * buflen  available length in buf; if too small then this routine will fail (return 0)
    //  * id_ / il_  ID bytes (and length) to go in the header; NULL means take ID from EEPROM
    //  * iv  12-byte initialisation vector / nonce; never NULL
    //  * key  16-byte secret key; never NULL
    // NOTE: this version requires the IV to be supplied and the transmitted ID length to chosen.
    static const uint8_t generateSecureBeaconMaxBufSize = 27 + SecurableFrameHeader::maxIDLength;
    uint8_t generateSecureBeaconRaw(uint8_t *buf, uint8_t buflen,
                                    const uint8_t *id_, uint8_t il_,
                                    const uint8_t *const iv,
                                    const fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t e,
                                    void *state, const uint8_t *key);

    // Create secure Alive / beacon (FTS_ALIVE) frame with an empty body for transmission.
    // Returns number of bytes written to buffer, or 0 in case of error.
    // The IV is constructed from the node ID and the primary TX message counter.
    // Note that the frame will be 27 + ID-length (up to maxIDLength) bytes,
    // so the buffer must be large enough to accommodate that.
    //  * buf  buffer to which is written the entire frame including trailer; never NULL
    //  * buflen  available length in buf; if too small then this routine will fail (return 0)
    //  * il_  ID length for the header; ID comes from EEPROM
    //  * key  16-byte secret key; never NULL
    uint8_t generateSecureBeaconRawForTX(uint8_t *buf, uint8_t buflen,
                                    uint8_t il_,
                                    fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t e,
                                    void *state, const uint8_t *key);

    // Create simple 'O' (FTS_BasicSensorOrValve) frame with an optional stats section for transmission.
    // Returns number of bytes written to buffer, or 0 in case of error.
    // The IV is constructed from the node ID and the primary TX message counter.
    // Note that the frame will be 27 + ID-length (up to maxIDLength) bytes,
    // so the buffer must be large enough to accommodate that.
    //  * buf  buffer to which is written the entire frame including trailer; never NULL
    //  * buflen  available length in buf; if too small then this routine will fail (return 0)
    //  * valvePC  percentage valve is open or 0x7f if no valve to report on
    //  * statsJSON  '\0'-terminated {} JSON stats, or NULL if none.
    //  * il_  ID length for the header; ID comes from EEPROM
    //  * key  16-byte secret key; never NULL
    uint8_t generateSecureOFrameRawForTX(uint8_t *buf, uint8_t buflen,
                                    uint8_t il_,
                                    uint8_t valvePC,
                                    const char *statsJSON,
                                    fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t e,
                                    void *state, const uint8_t *key);


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
