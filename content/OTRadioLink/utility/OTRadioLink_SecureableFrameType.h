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
 *
 * DE2017116: Outstanding issues
 * - FIXME: Currently aliasing ScratchSpace for passing buffer/length pairs.
 *   This does not allow for passing around immutable buffers. Where this is
 *   relevant, function arguments are indicated as `const OTBuf_t`.
 */

#ifndef ARDUINO_LIB_OTRADIOLINK_SECUREABLEFRAMETYPE_H
#define ARDUINO_LIB_OTRADIOLINK_SECUREABLEFRAMETYPE_H

#include <stdint.h>
#include <OTV0p2Base.h>

namespace OTRadioLink
    {

    // Alias ScratchSpace for passing around arrays of known length.
    using OTBuf_t = OTV0P2BASE::ScratchSpace;

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
    //
    // With all of these routines it is important to check and act on error codes,
    // usually aborting immediately if an error value is returned.
    // MUDDLING ON WITHOUT CHECKING FOR ERRORS MAY SEVERELY DAMAGE SYSTEM SECURITY.
    enum FrameType_Secureable : uint8_t
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

        // Reserved frame types at at 2016Q1.
        FTS_RESERVED_A                  = '*',
        FTS_RESERVED_Q                  = '?',

        // Used to indicate current flow.
        // With empty body section can indicate significant +ve half cycle flow for AC,
        // usually configured to indicate spill to grid from local microgeneration,
        // nominally synchronised/timed from start of frame transmission/receipt,
        // eg from receipt of frame sync if that time can be captured, else computed.
        // May use a light-weight security system and/or higher bit rate
        // and only be sent often enough to indicate ~0.5Wh of recent flow,
        // to meet radio duty-cycle (and energy availability) constraints.
        FTS_CURRENT                     = 'I',

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
    constexpr static uint8_t ENC_BODY_SMALL_FIXED_CTEXT_SIZE = 32;

    // For fixed-size default encrypted bodies the maximum plaintext size is one less.
    constexpr static uint8_t ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE =
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
    //
    // With all of these routines it is important to check and act on error codes,
    // usually aborting immediately if an error value is returned.
    // MUDDLING ON WITHOUT CHECKING FOR ERRORS MAY SEVERELY DAMAGE SYSTEM SECURITY.
    //
    // Create an instance as an invalid frame header ready to start with seqNum==0.
    // Make the frame length 0 (which is invalid).
    // Make the sequence number 0xf so that (pre-)incrementing will make it 0.
    // Make the ID length 0.
    struct SecurableFrameHeader final
        {
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
        static constexpr uint8_t minFrameSize = 4;

        // Maximum (small) frame size is 63, excluding fl byte.
        static constexpr uint8_t maxSmallFrameSize = 63;
        // Frame length excluding/after this byte [0,63]; zero indicates an invalid frame.
        // Appears first on the wire to support radio hardware packet handling.
        //     fl = hl-1 + bl + tl = 3+il + bl + tl
        // where hl header length, bl body length, tl trailer length
        // Should usually be set last to leave header clearly invalid until complete.
        uint8_t fl = 0;

        // Frame type nominally from FrameType_Secureable (bits 0-6, [1,126]).
        // Top bit indicates secure frame if 1/true.
        uint8_t fType = FTS_NONE;
        bool isSecure() const { return(0 != (0x80 & fType)); }

        // Frame sequence number mod 16 [0,15] (bits 4 to 7) and ID length [0,15] (bits 0-3).
        //
        // Sequence number increments from 0, wraps at 15;
        // increment is skipped for repeat TXes used for noise immunity.
        // If a counter is used as part of (eg) security IV/nonce
        // then these 4 bits may be its least significant bits.
        uint8_t seqIl = 0xf0;
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
        // Initial an`d 'small frame' implementations are limited to 8 bytes of ID
        static constexpr uint8_t maxIDLength = 8;
        uint8_t id[maxIDLength];

        // Get header length including the leading frame-length byte.
        inline uint8_t getHl() const { return(4 + getIl()); }

        // Maximum small frame body size is maximum frame size minus 4, excluding fl byte.
        // This maximum size is only achieved with non-secure frames with zero-length ID.
        static constexpr uint8_t maxSmallFrameBodySize = maxSmallFrameSize - 4;
        // Body length including any padding [0,251] but generally << 60.
        uint8_t bl;
        // Compute the offset of the body from the start of the frame starting with nominal fl byte.
        inline uint8_t getBodyOffset() const { return(getHl()); }

        // Compute tl (trailer length) [1,251]; must == 1 for insecure frame.
        // Other fields must be valid for this to return a valid answer.
        uint8_t getTl() const { return(fl - 3 - getIl() - bl); }
        // Compute the offset of the trailer from the start of the frame starting with nominal fl byte.
        uint8_t getTrailerOffset() const { return(4 + getIl() + bl); }


        /**
         * @brief   Validate parameters and encode a header into a buffer.
         * 
         * This does not permit encoding of frames with more than 64 bytes (ie 'small'
         * frames only). This does not deal with encoding the body or the trailer.
         * Having validated the parameters they are copied into the structure and then
         * into the supplied buffer, returning the number of bytes written.
         * The fl byte in the structure is set to the frame length, else 0 in case of
         * any error.
         * 
         * Performs as many as possible of the 'Quick Integrity Checks' from the spec,
         * eg SecureBasicFrame-V0.1-201601.txt:
         * 1) fl >= 4 (type, seq/il, bl, trailer bytes)
         * 2) fl may be further constrained by system limits, typically to <= 63
         * 3) type (the first frame byte) is never 0x00, 0x80, 0x7f, 0xff.
         * 4) il <= 8 for initial implementations (internal node ID is 8 bytes)
         * 5) il <= fl - 4 (ID length; minimum of 4 bytes of other overhead)
         * 6) bl <= fl - 4 - il (body length; minimum of 4 bytes of other overhead)
         * 7) NOT DONE: the final frame byte (the final trailer byte) is never 0x00 nor
         *              0xff
         * 8) tl == 1 for non-secure, tl >= 1 for secure (tl = fl - 3 - il - bl)
         * Note: fl = hl-1 + bl + tl = 3+il + bl + tl
         * 
         * @param   buf: buffer to encode header into. If NULL, the encoded form is not
         *               written. The buffer starts with fl, the frame length byte. If
         *               the buffer is too small for encoded header, the routine will
         *               fail (return 0)
         * @param   secure_: true if this is to be a secure frame.
         * @param   fType_: frame type (without secure bit) in range ]FTS_NONE,FTS_INVALID_HIGH[ ie exclusive
         * @param   seqNum_: least-significant 4 bits are 4 lsbs of frame sequence number
         * @param   id_: Source of ID bytes. Length in bytes must be <=8 (could be 15
         *               for non-small frames). NULL means pre-filled but must not
         *               start with 0xff.
         * @param   bl_: body length in bytes [0,251] at most.
         * @param   tl_: trailer length [1,251[ at most, always == 1 for non-secure frame.
         * @retval  Returns number of bytes of encoded header excluding the leading fl
         *          length byte, or 0 in case of error.
         */
        uint8_t encodeHeader(
                    OTBuf_t &buf,
                    bool secure_, FrameType_Secureable fType_,
                    uint8_t seqNum_,
                    const uint8_t *id_,
                    uint8_t il_,
                    uint8_t bl_,
                    uint8_t tl_);

        /**
         * @brief   Decode header and check parameters/validity for inbound short
         *          secureable frame.
         * 
         * Performs as many as possible of the 'Quick Integrity Checks' from the spec,
         * eg SecureBasicFrame-V0.1-201601.txt:
         * 1) fl >= 4 (type, seq/il, bl, trailer bytes)
         * 2) fl may be further constrained by system limits, typically to <= 63
         * 3) type (the first frame byte) is never 0x00, 0x80, 0x7f, 0xff.
         * 4) il <= 8 for initial implementations (internal node ID is 8 bytes)
         * 5) il <= fl - 4 (ID length; minimum of 4 bytes of other overhead)
         * 6) bl <= fl - 4 - il (body length; minimum of 4 bytes of other overhead)
         * 7) the final frame byte (the final trailer byte) is never 0x00 nor 0xff (if
         *    whole frame available)
         * 8) tl == 1 for non-secure, tl >= 1 for secure (tl = fl - 3 - il - bl)
         * Note: fl = hl-1 + bl + tl = 3+il + bl + tl
         * 
         * If the header is invalid or the buffer too small, 0 is returned to indicate
         * an error.
         * The fl byte in the structure is set to the frame length, else 0 in case of
         * any error.
         * 
         * @param   buf: buffer to decode header from, of at least length buflen. The
         *               buffer must start with the frame length byte (fl). Never NULL.
         * @param   buflen: available length in buf; if too small for encoded header
         *                  routine will fail (return 0)
         * @retval  Returns number of bytes of decoded header including
         *          nominally-leading fl length byte, or 0 in case of error.
         */
        uint8_t decodeHeader(const uint8_t *buf, uint8_t buflen);
        // Wrapper convenience function for allowing OTBuf_t to be passed into checkAndDecodeSmallFrameHeader
        uint8_t decodeHeader(const OTBuf_t &buf)
            { return(decodeHeader(buf.buf, buf.bufsize)); }

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
        uint8_t computeNonSecureCRC(const uint8_t *buf, uint8_t buflen) const;
        };


    /**
     * @brief   Struct for passing common frame data around in the encryption stack.
     *
     * @param   _ptext: Input buffer. This points to an array holding the message body in plain text.
                        May be a nullptr, to signal that there is no body to be encrypted.
     * @param   _ptextbufSize: Size of _ptext in bytes.
     *                         Should be 0 if _ptext is a nullptr, or at least 32.
     * @param   _outbuf: Output buffer to hold a frame.
     * @param   _outbufSize: Size of _outbuf in bytes. Should be at least 64.
     *
     * @note    ptextLen and fType may need to be set separately before calling
     *          some encode functions.
     * @note    The scratch space is not held in this as functions will pass
     *          the unused portions of their scratch spaces on.
     * @note    Fields may be reordered.
     */
    struct OTEncodeData_T
    {
        OTEncodeData_T(uint8_t * const _ptext, const uint8_t _ptextbufSize, uint8_t * const _outbuf, const uint8_t _outbufSize)
            : ptext(_ptext), ptextbufSize(_ptextbufSize), outbuf(_outbuf), outbufSize(_outbufSize) {}

        SecurableFrameHeader sfh;

        // Input buffer. This points to an array holding the message body in plain text.
        // This is NOT immutable, to allow in-place padding of the message buffer.
        // May be a nullptr, in which case there is no body.
        uint8_t * const ptext = nullptr;
        // Size of the ptext buffer.
        const uint8_t ptextbufSize = 0;
        // Length of data to be encrypted. Potentially unknown at time of instantiation.
        uint8_t ptextLen = 0;

        // The output buffer, into which the encoded frame is written. Must never be NULL.
        uint8_t *const outbuf = nullptr;
        // The size of the output buffer in bytes. Must be at least 64.
        const uint8_t outbufSize = 0;

        FrameType_Secureable fType = FTS_NONE;
    };

    /**
     * @brief   Struct for passing common frame data around in the decryption stack.
     *
     * @param   _inbuf: Input buffer. This points to an array holding the frame
     *                  length byte, followed by the encrypted frame.
     *                  i.e. |     _inbuf[0]     |   _inbuf[1:]      |
     *                       | frame length byte | Encrypted Message |
     *                  Never NULL.
     * @param   _ptext: Output buffer. This points to an array holding the
     *                  message body in plain text. Should be at least
     *                  ptextLenMax bytes in size. Never NULL.
     *
     * @note    The scratch space is not held in this as functions will pass
     *          the unused portions of their scratch spaces on.
     * @note    Although the frame length byte is technically not part of the
     *          frame, it is pointed too as the first byte for clarity and
     *          consistency within the decryption stack.
     * @note    Fields may be reordered.
     */
    struct OTDecodeData_T
    {
        OTDecodeData_T(const uint8_t * const _inbuf, uint8_t * const _ptext)
            : ctext(_inbuf), ptext(_ptext) {}

        SecurableFrameHeader sfh;
        uint8_t id[OTV0P2BASE::OpenTRV_Node_ID_Bytes] = {};  // Holds up to full node ID.
        // Immutable input buffer. This takes a buffer holding the frame length
        // byte, followed by the encrypted frame.
        const uint8_t * const ctext = nullptr;
        // The first byte of the input buffer is the total frame length in
        // bytes, EXCLUDING the frame length byte.
        // i.e. the total buffer length is inbufLen + 1.
        const uint8_t &ctextLen = ctext[0];

        // Output buffer. The decrypted frame is written to this.
        // Should be at least ptextLenMax bytes in length.
        uint8_t *const ptext = nullptr;
        // This is currently always  ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE bytes long.
        static constexpr uint8_t ptextLenMax = ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE;
        // Actual size of plain text held within decryptedBody. Set when ptext is populated.
        uint8_t ptextLen = 0;  // 1 byte: 1/4 words
    };

    /**
     * @brief   Compose (encode) entire non-secure small frame from header params,
     *          body and CRC trailer.
     *
     * @param   fd: Common data required for encryption.
     *              - ptext: body data.
     *              - ptextbufSize: Size of the body. FIXME Should this be ptextLen instead?
     *              - outbuf: buffer to which is written the entire frame including
     *                trailer/CRC. The supplied buffer may have to be up to 64
     *                bytes long. Never NULL.
     *              - outbufSize: Available length in buf. May need to be up to 64
     *                bytes. If too small then this routine will fail (return 0)
     *              - fType: frame type (without secure bit) in range ]FTS_NONE,FTS_INVALID_HIGH[ ie exclusive XXX
     * @param   seqNum: least-significant 4 bits are 4 lsbs of frame sequence number.
     * @param   id: ID bytes (and length) to go in the header. NULL means take ID from EEPROM.
     * @retval  Total frame length in bytes + fl byte + 1, or 0 if there is an error eg
     *          because the CRC check failed.
     *
     * @note    Uses a scratch space, allowing the stack usage to be more tightly controlled.
     */
    uint8_t encodeNonsecure(OTEncodeData_T &fd, uint8_t seqNum, const uint8_t *id, uint8_t il);

    /**
     * @brief   Decode entire non-secure small frame from raw frame bytes support.
     *
     * Typical workflow:
     * - Before calling this function, decode the header alone to extract the ID
     *   and frame type.
     * - Use the frame header's bl and getBodyOffset() to get the body and body
     *   length.
     *
     * @param   fd: Common data required for encryption.
     *              - sfh: Pre-decoded frame header. If this has not been decoded
     *                failed to decode, this routine will fail (return 0).
     *              - ctext: buffer containing the entire frame including header
     *                and trailer. Never NULL
     * @retval  Total frame length in bytes + fl byte + 1, or 0 if there is an error eg
     *          because the CRC check failed.
     */
    uint8_t decodeNonsecure(OTDecodeData_T &fd);

//        // Round up to next 16 multiple, eg for encryption that works in fixed-size blocks for input [0,240].
//        // Eg 0 -> 0, 1 -> 16, ... 16 -> 16, 17 -> 32 ...
//        // Undefined for values above 240.
//        uint8_t roundUpTo16s(uint8_t s) { return((s + 15) & 0xf0); }

    // TX&RX Base class common elements that won't consume code/RAM space eg unless actually used.
    // Mainly types, primitive constants, and a smattering of small static functions.
    // With all of these routines it is important to check and act on error codes,
    // usually aborting immediately if an error value is returned.
    // MUDDLING ON WITHOUT CHECKING FOR ERRORS MAY SEVERELY DAMAGE SYSTEM SECURITY.
    class SimpleSecureFrame32or0BodyBase
        {
        public:
            // Size of full message counter for type-0x80 AES-GCM security frames.
            static constexpr uint8_t fullMsgCtrBytes = 6;
        };

    // TX Base class for simple implementations that supports 0 or 32 byte encrypted body sections.
    // This wraps up any necessary state, persistent and ephemeral, such as message counters.
    // Some implementations make sense only as singletons,
    // eg because they store state at fixed locations in EEPROM.
    // It is possible to provide implementations not tied to any particular hardware architecture.
    // This provides stateless hardware-independent implementations of some key routines.
    //
    // With all of these routines it is important to check and act on error codes,
    // usually aborting immediately if an error value is returned.
    // MUDDLING ON WITHOUT CHECKING FOR ERRORS MAY SEVERELY DAMAGE SYSTEM SECURITY.
    class SimpleSecureFrame32or0BodyTXBase : public SimpleSecureFrame32or0BodyBase
        {
        public:
            // Get TX ID that will be used for transmission; returns false on failure.
            // Argument must be buffer of (at least) OTV0P2BASE::OpenTRV_Node_ID_Bytes bytes.
            virtual bool getTXID(uint8_t *id) const = 0;

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
            static uint8_t pad32BBuffer(uint8_t *buf, uint8_t datalen);

            // Signature of pointer to basic fixed-size text encryption/authentication function with workspace supplied.
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
            // Note that the authenticated text size is not fixed, ie is zero or
            // more bytes.
            // A workspace is passed in (and cleared on exit);
            // this routine will fail (safely, returning false) if the workspace
            // is NULL or too small.
            // The workspace requirement depends on the implementation used.
            static constexpr size_t workspaceRequred_GCM32B16B_OTAESGCM_2p0 =
                176 /* AES element */ +
                96 /* GCM element as at 20170707 */ ;
            // Returns true on success, false on failure.
            typedef bool (fixed32BTextSize12BNonce16BTagSimpleEnc_fn_t)(
                    uint8_t *workspace, size_t workspaceSize,
                    const uint8_t *key, const uint8_t *iv,
                    const uint8_t *authtext, uint8_t authtextSize,
                    const uint8_t *plaintext,
                    uint8_t *ciphertextOut, uint8_t *tagOut);

            /**
             * @brief   Encode entire secure small frame from header params and body and
             *          crypto support.
             *
             * This is a raw/partial impl that requires the IV/nonce to be supplied.
             *
             * This uses fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t style encryption.
             * The matching decryption function should be used for decoding/verifying. The
             * crypto method may need to vary based on frame type, and on negotiations
             * between the participants in the communications.
             *
             * The message counter must be strictly greater than the last message from this
             * ID, to prevent replay attacks.
             *
             * The sequence number is taken from the 4 least significant bits of the
             * message counter (at byte 6 in the nonce).
             *
             * Typical workflow:
             * - XXX
             *
             * @param   fd: Common data required for encryption.
             *              - ptext: Mutable buffer containing any ptext to be encrypted.
             *                       Note that the ptext will be padded in situ with 0s.
             *                       Should be ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE (32)
             *                       bytes in length, or NULL if no ciphertext is
             *                       expected/wanted.
             *              - ptextbufSize: The size of the ptext buffer in bytes,
             *              - ptextLen: The actual length of the ptext held in ptext,
             *                          before padding. Always less than
             *                          ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE bytes.
             *              - outbuf: Buffer to hold the entire secure frame, including the
             *                        trailer. At least 64 bytes and never NULL.
             *              - outbufSize: Size of outbuf in bytes.
             *              - fType: frame type (without secure bit). Note that values of
             *                       FTS_NONE and FTS_INVALID_HIGH will cause encryption to
             *                       fail.
             * @param   e: encryption function.
             * @param   scratch: Scratch space. Size must be large enough to contain
             *                   decodeRaw_total_scratch_usage_OTAESGCM_3p0 bytes AND the
             *                   scratch space required by the decryption function `d`.
             * @param   key: 16-byte secret key. Never NULL.
             * @param   iv: 12-byte initialisation vector/nonce. Never NULL.
             * @param   id: ID bytes (and length) to go in the header; NULL means
             *              take ID from EEPROM
             * @retval  Returns the total number of bytes written out for (the frame + the
             *          leading frame length byte + 1). Returns zero in case of error.
             *
             * @note    Uses a scratch space, allowing the stack usage to be more tightly controlled.
             */
            static constexpr uint8_t encodeRaw_scratch_usage = 0;
            static constexpr size_t encodeRaw_total_scratch_usage_OTAESGCM_2p0 =
                    workspaceRequred_GCM32B16B_OTAESGCM_2p0
                    + encodeRaw_scratch_usage;
            static uint8_t encodeRaw(
                                OTEncodeData_T &fd,
                                // const OTBuf_t &id_,
                                const uint8_t *id,
                                const uint8_t il,
                                const uint8_t *iv,
                                fixed32BTextSize12BNonce16BTagSimpleEnc_fn_t &e,
                                const OTV0P2BASE::ScratchSpaceL &scratch,
                                const uint8_t *key);

            // Get the 3 bytes of persistent reboot/restart message counter, ie 3 MSBs of message counter; returns false on failure.
            // Combines results from primary and secondary as appropriate.
            // Deals with inversion and checksum checking.
            // Output buffer (buf) must be 3 bytes long.
            // Does not increment/alter the counter.
            static const uint8_t txNVCtrPrefixBytes = 3;
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
            // Get the 3 bytes of persistent reboot/restart message counter, ie 3 MSBs of message counter; returns false on failure.
            virtual bool getTXNVCtrPrefix(uint8_t *buf) const = 0;
            // Reset the persistent reboot/restart message counter; returns false on failure.
            // TO BE USED WITH EXTREME CAUTION: reusing the message counts and resulting IVs
            // destroys the security of the cipher.
            // Probably only sensible to call this when changing either the ID or the key (or both).
            // This can reset the restart counter to all zeros (erasing the underlying EEPROM bytes),
            // or (default) reset only the most significant bits to zero (preserving device life)
            // but inject entropy into the least significant bits to reduce risk value/IV reuse in error.
            // If called with false then interrupts should not be blocked to allow entropy gathering,
            // and counter is guaranteed to be non-zero.
            virtual bool resetTXNVCtrPrefix(bool allZeros = false) = 0;
            // Increment persistent reboot/restart message counter; returns false on failure.
            // Will refuse to increment such that the top byte overflows, ie when already at 0xff.
            // TO BE USED WITH EXTREME CAUTION: calling this unnecessarily will shorten life before needing to change ID/key.
            virtual bool incrementTXNVCtrPrefix() = 0;
            // Fills the supplied 6-byte array with the incremented monotonically-increasing primary TX counter.
            // Returns true on success; false on failure for example because the counter has reached its maximum value.
            // Highest-index bytes in the array increment fastest.
            // This should never return an all-zero count.
            // Not ISR-safe.
            virtual bool getNextTXMsgCtr(uint8_t *buf) = 0;

            // Fill in 12-byte IV for 'O'-style (0x80) AESGCM security for a frame to TX; returns false on failure.
            // This uses the local node ID as-is for the first 6 bytes by default,
            // but sub-classes may allow other IDs to be supplied.
            // This uses and increments the primary message counter for the last 6 bytes.
            // Returns true on success, false on failure eg due to message counter generation failure.
            virtual bool computeIVForTX12B(uint8_t *ivBuf)
                {
                if(NULL == ivBuf) { return(false); }
                // Fetch entire ID directly to ivBuf for simplicity; lsbytes will be overwritten with message counter.
                if(!getTXID(ivBuf)) { return(false); } // ID fetch failed.
                // Generate and fill in new message count at end of IV.
                return(getNextTXMsgCtr(ivBuf + (12-SimpleSecureFrame32or0BodyBase::fullMsgCtrBytes)));
                }

            /**
             *@brief   Create a generic secure small frame with an optional encrypted body
             *          for transmission.
             *
             * The IV is constructed from the node ID (built-in from EEPROM or as supplied)
             * and the primary TX message counter (which is incremented).
             * 
             * Note that the frame will be 27 + ID-length (up to maxIDLength) + body-length
             * bytes, so the buffer must be large enough to accommodate that.
             *
             * @param   fd: Common data required for encryption:
             *              - ptext: Plaintext to encrypt. May be nullptr if not required.
             *              - ptextbufSize: Size of plaintext buffer. 0 if ptext is a 
             *                nullptr.
             *              - ptextLen: The length of plaintext held by ptext. Must be less
             *                than ptextbufSize.
             *              - outbuf: Buffer to hold entire encrypted frame, incl trailer.
             *                Never NULL.
             *              - outbufSize: available length in buf. If it is too small then
             *                this routine will fail.
             *              - ftype: Must be set with a valid frame type before calling this
             *                function.
             * @param   il_: ID length for the header. ID is local node ID from EEPROM or
             *               other pre-supplied ID, may be limited to a 6-byte prefix
             * @param   e: Encryption function.
             * @param   scratch: Scratch space. Size must be large enough to contain
             *                   encode_total_scratch_usage_OTAESGCM_2p0 bytes AND the
             *                   scratch space required by the decryption function `e`.
             * @param   key: 16-byte secret key. Never NULL.
             * @retval  Returns number of bytes written to fd.outbuf, or 0 in case of error.
             *
             * @note    Uses a scratch space, allowing the stack usage to be more tightly
             *          controlled.
             * 
             * @FIXME   UNTESTED BY CI!
             */
            static constexpr uint8_t encode_scratch_usage = 12 + 8;
            static constexpr size_t encode_total_scratch_usage_OTAESGCM_2p0 =
                     encodeRaw_total_scratch_usage_OTAESGCM_2p0
                     + encode_scratch_usage;
            uint8_t encode(
                        OTEncodeData_T &fd,
                        uint8_t il_,
                        fixed32BTextSize12BNonce16BTagSimpleEnc_fn_t &e,
                        OTV0P2BASE::ScratchSpaceL &scratch,
                        const uint8_t *key);

            static const uint8_t generateSecureBeaconMaxBufSize = 27 + SecurableFrameHeader::maxIDLength; // FIXME Still needed?
            // Create secure Alive / beacon (FTS_ALIVE) frame with an empty body for transmission.
            // Returns number of bytes written to buffer, or 0 in case of error.
            // The IV is constructed from the node ID and the primary TX message counter.
            // Note that the frame will be 27 + ID-length (up to maxIDLength) bytes,
            // so the buffer must be large enough to accommodate that.
            //  * buf  buffer to which is written the entire frame including trailer; never NULL
            //  * buflen  available length in buf; if too small then this routine will fail (return 0)
            //  * il_  ID length for the header; ID comes from EEPROM or other pre-supplied ID
            //  * key  16-byte secret key; never NULL
            // Simple example implementation for complete O-style secure frame TX workflow.
            // NOTE: THIS API IS LIABLE TO CHANGE
            uint8_t generateSecureBeacon(
                        OTBuf_t &buf,
                        uint8_t il_,
                        fixed32BTextSize12BNonce16BTagSimpleEnc_fn_t  &e,
                        OTV0P2BASE::ScratchSpaceL &scratch,
                        const uint8_t *key)
            {
                OTEncodeData_T fd(buf.buf, buf.bufsize, NULL, 0);
                fd.fType = OTRadioLink::FTS_ALIVE;
                return(encode(fd, il_, e, scratch, key));
            }

            /**
             * @brief   Create simple 'O' (FTS_BasicSensorOrValve) frame with an optional
             *          stats section for transmission.
             *
             * The IV is constructed from the node ID (built-in from EEPROM or as supplied)
             * and the primary TX message counter (which is incremented).
             * 
             * Note that the frame will be 27 + ID-length (up to maxIDLength) + body-length
             * bytes, so the buffer must be large enough to accommodate that.
             * 
             * Uses a scratch space to allow tightly controlling stack usage.
             *
             * @param   fd: Common data required for encryption:
             *              - ptext: Plaintext to encrypt. '\0'-terminated {} JSON stats.
             *                       May be nullptr if not required.
             *              - ptextbufSize: Size of plaintext buffer. 0 if ptext is a 
             *                nullptr.
             *              - outbuf: Buffer to hold entire encrypted frame, incl trailer.
             *                        Never NULL.
             *              - outbufSize: available length in buf. If it is too small then
             *                            this routine will fail.
             * @param   il_: ID length for the header. ID is local node ID from EEPROM or
             *               other pre-supplied ID, may be limited to a 6-byte prefix
             * @param   valvePC: Percentage valve is open or 0x7f if no valve to report on.
             * @param   e: Encryption function.
             * @param   scratch: Scratch space. Size must be large enough to contain
             *                   encodeValveFrame_total_scratch_usage_OTAESGCM_2p0 
             *                   bytes AND the scratch space required by the decryption
             *                   function `e`.
             * @param   key: 16-byte secret key. Never NULL.
             * @param   firstIDMatchOnly: IGNORED! If the 'firstMatchIDOnly' is true
             *              (the default) then this only checks the first ID prefix
             *              match found if any, else all possible entries may be tried
             *              depending on the implementation  and, for example,
             *              time/resource limits.
             * @retval  Returns number of bytes written to fd.outbuf, or 0 in case of error.
             */
            static constexpr uint8_t encodeValveFrame_scratch_usage = 12; // + 32; as bbuf moved out to level above.
            static constexpr size_t encodeValveFrame_total_scratch_usage_OTAESGCM_2p0 =
                    encodeRaw_total_scratch_usage_OTAESGCM_2p0
                    + encodeValveFrame_scratch_usage;
            uint8_t encodeValveFrame(
                        OTEncodeData_T &fd,
                        uint8_t il_,
                        uint8_t valvePC,
                        fixed32BTextSize12BNonce16BTagSimpleEnc_fn_t &e,
                        const OTV0P2BASE::ScratchSpaceL &scratch,
                        const uint8_t *key);
        };

    // RX Base class for simple implementations that supports 0 or 32 byte encrypted body sections.
    // This wraps up any necessary state, persistent and ephemeral, such as message counters.
    // Some implementations make sense only as singletons,
    // eg because they store state at fixed locations in EEPROM.
    // It is possible to provide implementations not tied to any particular hardware architecture.
    // This provides stateless hardware-independent implementations of some key routines.
    //
    // Implementations may keep a cache of node associations and RX message counters
    // eg to allow ISR-/thread- safe filtering of inbound frames in interrupt RX routines.
    //
    // Wit>portant to check and act on error codes,
    // usually aborting immediately if an error value is returned.
    // MUDDLING ON WITHOUT CHECKING FOR ERRORS MAY SEVERELY DAMAGE SYSTEM SECURITY.
    class SimpleSecureFrame32or0BodyRXBase : public SimpleSecureFrame32or0BodyBase
        {
        private:
            virtual int8_t _getNextMatchingNodeID(const uint8_t index, const SecurableFrameHeader *const sfh, uint8_t *nodeID) const = 0;

        public:
            // Check one (6-byte) message counter against another for magnitude.
            // Returns 0 if they are identical, +ve if the first counter is greater, -ve otherwise.
            // Logically like getting the sign of counter1 - counter2.
            static int16_t msgcountercmp(const uint8_t *counter1, const uint8_t *counter2)
                { return(int16_t(memcmp(counter1, counter2, fullMsgCtrBytes))); }

            // Add specified small unsigned value to supplied counter value in place; false if failed.
            // This will fail (returning false) if the counter would overflow, leaving it unchanged.
            static bool msgcounteradd(uint8_t *counter, uint8_t delta);


            // Unpads plain-text in place prior to encryption with 32-byte fixed length padded output.
            // Reverses/validates padding applied by addPaddingTo32BTrailing0sAndPadCount().
            // Returns unpadded data length (at start of buffer) or 0xff in case of error.
            //
            // Parameters:
            //  * buf  buffer containing the plain-text; must be >= 32 bytes, never NULL
            //
            // NOTE: does not check that all padding bytes are actually zero.
            // 
            // FIXME    Actually returns unpadded data length, or 0 on fail.
            // TODO     figure out what it should do.
            static uint8_t unpad32BBuffer(const uint8_t *buf);


            // Signature of pointer to basic fixed-size text decryption/authentication function with workspace supplied.
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
            // Note that the authenticated text size is not fixed,
            // ie is zero or more bytes.
            // Decrypts/authenticates the output of a
            // fixed32BTextSize12BNonce16BTagSimpleEnc_ptr_t function.)
            // A workspace is passed in (and cleared on exit);
            // this routine will fail (safely, returning false)
            // if the workspace is NULL or too small.
            // The workspace requirement depends on the implementation used.
            static constexpr size_t workspaceRequred_GCM32B16B_OTAESGCM_2p0 =
                SimpleSecureFrame32or0BodyTXBase::workspaceRequred_GCM32B16B_OTAESGCM_2p0;
            // Returns true on success, false on failure.
            typedef bool (fixed32BTextSize12BNonce16BTagSimpleDec_fn_t)(
                    uint8_t *workspace, size_t workspaceSize,
                    const uint8_t *key, const uint8_t *iv,
                    const uint8_t *authtext, uint8_t authtextSize,
                    const uint8_t *ciphertext, const uint8_t *tag,
                    uint8_t *plaintextOut);

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
             *              - ptextLen: The size of the decoded body in ptext.
             * @param   d: Decryption function.
             * @param   scratch: Scratch space. Size must be large enough to contain
             *                   decodeRaw_total_scratch_usage_OTAESGCM_3p0 bytes AND the
             *                   scratch space required by the decryption function `d`.
             * @param   key: 16-byte secret key. Never NULL.
             * @param   iv: 12-byte initialisation vector/nonce. Never NULL.
             * @retval  - Returns the total number of bytes read for the frame including,
             *          and with a value one higher than the first 'fl' bytes).
             *          - Returns zero in case of error, eg because authentication failed.
             *          - Returns 1 and sets ptextLen to 0 if ptext is NULL but auth passes.
             *
             * @note    Uses a scratch space, allowing the stack usage to be more tightly controlled.
             */
            static constexpr uint8_t decodeRaw_scratch_usage =
                ENC_BODY_SMALL_FIXED_CTEXT_SIZE;
            static constexpr size_t decodeRaw_total_scratch_usage_OTAESGCM_3p0 =
                0 /* Any additional callee space would be for d(). */ +
                decodeRaw_scratch_usage;
            static uint8_t decodeRaw(
                                OTDecodeData_T &fd,
                                fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &d,
                                const OTV0P2BASE::ScratchSpaceL &scratch,
                                const uint8_t *key,
                                const uint8_t *iv);

            // Design notes on use of message counters vs non-volatile storage life, eg for ATMega328P.
            //
            // Note that the message counter is designed to:
            //  a) prevent reuse of IVs, which can fatally weaken the cipher,
            //  b) avoid replay attacks.
            //
            // The implementation on both TX and RX sides should:
            //  a) allow nominally 10 years' life from the non-volatile store and thus the unit,
            //  b) be resistant to (for example) deliberate power-cycling during update,
            //  c) random non-volatile memory failures.
            //
            // Some assumptions:
            //  a) aiming for 10 years' continuous product life at transmitters and receivers,
            //  b) around one TX per sensor/valve node per 4 minutes,
            //
            // Read current (last-authenticated) RX message count for specified node, or return false if failed.
            // Will fail for invalid node ID and for unrecoverable memory corruption.
            // Both args must be non-NULL, with counter pointing to enough space to copy the message counter value to.
            virtual bool getLastRXMsgCtr(const uint8_t * const ID, uint8_t *counter) const = 0;
            // Check message counter for given ID, ie that it is high enough to be eligible for authenticating/processing.
            // ID is full (8-byte) node ID; counter is full (6-byte) counter.
            // Returns false if this counter value is not higher than the last received authenticated value.
            bool validateRXMsgCtr(const uint8_t *ID, const uint8_t *counter) const;
            // Update persistent message counter for received frame AFTER successful authentication.
            // ID is full (8-byte) node ID; counter is full (6-byte) counter.
            // Returns false on failure, eg if message counter is not higher than the previous value for this node.
            // The implementation should allow several years of life typical message rates (see above).
            // The implementation should be robust in the face of power failures / reboots, accidental or malicious,
            // not allowing replays nor other cryptographic attacks, nor forcing node dissociation.
            // Must only be called once the RXed message has passed authentication.
            virtual bool authAndUpdateRXMsgCtr(const uint8_t *ID, const uint8_t *newCounterValue) = 0;

        protected:
            /**
             * @brief   Decode a frame from a given ID. NOT A PUBLIC ENTRY POINT!
             * 
             * The frame should already have some checks carried out, e.g. by decode.
             *
             * Passed a candidate node/counterparty ID derived from:
             * - The frame ID in the incoming header.
             * - Possible other adjustments, such as forcing bit values for reverse flows.
             * The expanded ID must be at least length 6 for 'O' / 0x80 style enc/auth.
             *
             * This routine constructs an IV from this expanded ID and other information
             * in the header and then returns the result of calling decodeRaw().
             *
             * If several candidate nodes share the ID prefix in the frame header (in
             * the extreme case with a zero-length header ID for an anonymous frame)
             * then they may all have to be tested in turn until one succeeds.
             *
             * Generally, this should be called AFTER checking that the aggregate RXed
             * message counter is higher than for the last soutbufuccessful receive for 
             * this node and flow direction. On success, those message counters should be
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
             * @param   d: Decryption function.
             * @param   adjID: Adjusted candidate ID based on the received ID in the
             *                 header. Must be able to hold >= 6 bytes. Never NULL.
             * @param   scratch: Scratch space. Size must be large enough to contain
             *                   _decodeFromID_total_scratch_usage_OTAESGCM_3p0 bytes AND
             *                   the scratch space required by the decryption function `d`.
             * @param   key: 16-byte secret key. Never NULL.
             * @retval  Total number of bytes read for the frame (including, and with a
             *          value one higher than the first 'fl' bytes).
             *          Returns zero in case of error, eg because authentication failed.
             *
             * @note    Uses a scratch space, allowing the stack usage to be more tightly controlled.
             */
            static constexpr uint8_t _decodeFromID_scratch_usage =
                12; // Space for constructed IV.
            static constexpr size_t _decodeFromID_total_scratch_usage_OTAESGCM_3p0 =
                decodeRaw_total_scratch_usage_OTAESGCM_3p0 +
                _decodeFromID_scratch_usage;
            uint8_t _decodeFromID(
                        OTDecodeData_T &fd,
                        fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &d,
                        const OTBuf_t adjID,
                        OTV0P2BASE::ScratchSpaceL &scratch,
                        const uint8_t *key);

        public:
            /**
             * @brief   Decode a structurally correct secure small frame.
             *          THIS IS THE PREFERRED ENTRY POINT FOR DECODING AND RECEIVING
             *          SECURE FRAMES AND PERFORMS EXCRUCIATINGLY CAREFUL CHECKING.
             *
             * From a structurally correct secure frame, looks up the ID, checks the
             * message counter, decodes, and updates the counter if successful.
             * (Pre-filtering by type and ID and message counter may already have
             * happened.)
             * 
             * Note that this is for frames being send from the ID in the header,
             * not for lightweight return traffic to the specified ID.
             * 
             * Uses a scratch space to allow tightly controlling stack usage.
             *
             * @param   fd: Common data required for decryption.
             *              - inbuf: Never NULL.
             *              - ptext: If null, only authentication will be performed.
             *                No plaintext will be provided.
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
             * @retval  Total frame length + fl byte + 1, or 0 if there is an error, eg.
             *          because authentication failed, or this is a duplicate message.
             *          - If this returns 1, the frame was authenticated but had no body.
             *          - If this returns >1 then the frame is authenticated, and the
             *            decrypted body is available if present and a buffer was
             *            provided.
             */
            static constexpr uint8_t decode_scratch_usage =
                OTV0P2BASE::OpenTRV_Node_ID_Bytes +
                SimpleSecureFrame32or0BodyBase::fullMsgCtrBytes;
            static constexpr size_t decode_total_scratch_usage_OTAESGCM_3p0 =
                _decodeFromID_total_scratch_usage_OTAESGCM_3p0 +
                decode_scratch_usage;
            uint8_t decode(
                        OTDecodeData_T &fd,
                        fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &d,
                        OTV0P2BASE::ScratchSpaceL &scratch,
                        const uint8_t *key,
                        bool firstIDMatchOnly = true);

        };


    /**
     * @brief   NULL basic fixed-size text 'encryption' function. DOES NOT ENCRYPT
     *          OR AUTHENTICATE SO DO NOT USE IN PRODUCTION SYSTEMS.
     * 
     * Emulates some aspects of the process to test real implementations against,
     * and that some possible gross errors in the use of the crypto are absent.
     * 
     * - Copies the plaintext to the ciphertext, unless plaintext is NULL.
     * - Copies the nonce/IV to the tag and pads with trailing zeros.
     * - The key is ignored (though one must be supplied).
     *
     * XXX param
     * @param   key: 16-byte secret key. Never NULL.
     * @retval  Returns true on success, false on failure.
     *
     * @note    Uses a scratch space, allowing the stack usage to be more tightly
     *          controlled.
     */
    SimpleSecureFrame32or0BodyTXBase::fixed32BTextSize12BNonce16BTagSimpleEnc_fn_t fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL;

    /**
     * @brief   NULL basic fixed-size text 'decryption' function. DOES NOT DECRYPT
     *          OR AUTHENTICATE SO DO NOT USE IN PRODUCTION SYSTEMS.
     * 
     * Emulates some aspects of the process to test real implementations against,
     * and that some possible gross errors in the use of the crypto are absent.
     * 
     * - Undoes/checks fixed32BTextSize12BNonce16BTagSimpleEnc_NULL_IMPL(). 
     * - Copies the ciphertext to the plaintext, unless ciphertext is NULL.
     * - Verifies that the tag seems to have been constructed appropriately.
     *
     * XXX param
     * @param   key: 16-byte secret key. Never NULL.
     * @retval  Returns true on success, false on failure.
     *
     * @note    Uses a scratch space, allowing the stack usage to be more tightly
     *          controlled.
     */
    SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_fn_t fixed32BTextSize12BNonce16BTagSimpleDec_NULL_IMPL;


    // CONVENIENCE/BOILERPLATE METHODS

    /**
     * @brief   Create non-secure Alive / beacon (FTS_ALIVE) frame with an empty
     *          body.
     *
     * @param   buf: buffer to which is written the entire frame including trailer.
     *               Never NULL. Note that the frame will be at least 4 + ID-length
     *               (up to maxIDLength) bytes, so the buffer must be large enough
     *               to accommodate that. If too small the routine will fail.
     * @param   seqNum: least-significant 4 bits are 4 lsbs of frame sequence
     *                  number
     * @param   id: ID bytes (and length) to go in the header; NULL means take ID
     *              from EEPROM
     * @retval  Returns number of bytes written to fd.outbuf, or 0 in case of error.
     */
    static const uint8_t generateNonsecureBeaconMaxBufSize = 5 + SecurableFrameHeader::maxIDLength;
    uint8_t generateNonsecureBeacon(OTBuf_t &buf, uint8_t seqNum, const uint8_t *id, uint8_t il);


    /**
     * @brief   A fixed counter implementation of SimpleSecureFrame32or0BodyRXBase.
     *
     * This is intended primarily for unit testing purposes and allows a mock ID and counter value
     * to be set in order to simplify using existing test frames.
     * - Call the setMock... methods to initialise the ID and counter values.
     * - Note that the counter value must be LESS than the value you expect to use or decryption will fail!
     * - The counter will not be incremented between calls and the methods will always act as if they have
     *   succeeded.
     *
     * @note    See FrameHandlerTest.cpp for example use.
     */
    class SimpleSecureFrame32or0BodyRXFixedCounter final : public SimpleSecureFrame32or0BodyRXBase
    {
    private:
        uint8_t mockID[8];
        uint8_t mockCounter[6];

        SimpleSecureFrame32or0BodyRXFixedCounter()
        {
            memset(mockID, 0, sizeof(mockID));
            memset(mockCounter, 0, sizeof(mockCounter));
        }
        /**
         * @brief   Copies mockID into the provided buffer.
         * @param   index: The index the caller expects you to search first. Should be >= 0
         * @param   nodeID: Buffer to copy mockID too. Must be at least 6 bytes.
         * @retval  always true.
         */
        virtual int8_t _getNextMatchingNodeID(const uint8_t index, const SecurableFrameHeader *const /* sfh */, uint8_t *nodeID) const override
        {
            memcpy(nodeID, mockID, 6);
            return (index);
        }
    public:
        static SimpleSecureFrame32or0BodyRXFixedCounter &getInstance()
        {
            // Lazily create/initialise singleton on first use, NOT statically.
            static SimpleSecureFrame32or0BodyRXFixedCounter instance;
            return(instance);
        }

        // Read current (last-authenticated) RX message count for specified node, or return false if failed.
        // Will fail for invalid node ID and for unrecoverable memory corruption.
        // Both args must be non-NULL, with counter pointing to enough space to copy the message counter value to.
        /**
         * @brief   Copies mockCounter into the provided buffer.
         * @param   index: The index the caller expects you to search first. Should be >= 0
         * @param   nodeID: Buffer to copy mockID too. Must be at least 6 bytes.
         * @retval  always true.
         */
        virtual bool getLastRXMsgCtr(const uint8_t * const /*ID*/, uint8_t * counter) const override
        {
            memcpy(counter, mockCounter, 6);
            return (true);
        }
        // // Check message counter for given ID, ie that it is high enough to be eligible for authenticating/processing.
        // // ID is full (8-byte) node ID; counter is full (6-byte) counter.
        // // Returns false if this counter value is not higher than the last received authenticated value.
        // virtual bool validateRXMessageCount(const uint8_t *ID, const uint8_t *counter) const override { return (true); }
        // Update persistent message counter for received frame AFTER successful authentication.
        // ID is full (8-byte) node ID; counter is full (6-byte) counter.
        // Returns false on failure, eg if message counter is not higher than the previous value for this node.
        // The implementation should allow several years of life typical message rates (see above).
        // The implementation should be robust in the face of power failures / reboots, accidental or malicious,
        // not allowing replays nor other cryptographic attacks, nor forcing node dissociation.
        // Must only be called once the RXed message has passed authentication.
        virtual bool authAndUpdateRXMsgCtr(const uint8_t * /*ID*/, const uint8_t * /*newCounterValue*/) override
        {
            return (true);
        }

        /**
         * @brief   Set the value of the internal 8 byte ID to allow us to decode a frame.
         */
        void setMockIDValue(const uint8_t * newID)
        {
            memcpy(mockID, newID, 8);
        }
        /**
         * @brief   Set the value of the internal 6 byte counter to allow us to decode a frame.
         * @param   newCounter: The new value to set. Should be less than the counter in the message to be decoded.
         */
        void setMockCounterValue(const uint8_t * newCounter)
        {
            memcpy(mockCounter, newCounter, 6);
        }
    };


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
