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
 * V0p2-specific implementation of secure frame code,
 * using EEPROM for non-volatile storage of (eg) message counters.
 *
 * V0p2/AVR only.
 */

#ifndef ARDUINO_LIB_OTRADIOLINK_SECUREABLEFRAMETYPE_V0P2IMPL_H
#define ARDUINO_LIB_OTRADIOLINK_SECUREABLEFRAMETYPE_V0P2IMPL_H

#include <stdint.h>
#include <OTV0p2Base.h>

#include "OTRadioLink_SecureableFrameType.h"


namespace OTRadioLink
    {


#ifdef ARDUINO_ARCH_AVR

    // V0p2 TX implementation for 0 or 32 byte encrypted body sections.
    //
    // With all of these routines it is important to check and act on error codes,
    // usually aborting immediately if an error value is returned.
    // MUDDLING ON WITHOUT CHECKING FOR ERRORS MAY SEVERELY DAMAGE SYSTEM SECURITY.
    //
    // Storage format for primary TX message counter.
    // The ephemeral 3 bytes are held in RAM.
    // The restart/reboot 3 bytes is stored in a primary and secondary copy in EEPROM,
    // along with an 8-bit CRC each, all stored inverted,
    // so that the all-1s erased state of counter and CRC is valid (counter value 0).
#define SimpleSecureFrame32or0BodyTXV0p2_DEFINED
    class SimpleSecureFrame32or0BodyTXV0p2 : public SimpleSecureFrame32or0BodyTXBase
        {
        protected:
            // Constructor is protected to force use of factory method to return singleton.
            // Else deriving class can construct some other way.
            SimpleSecureFrame32or0BodyTXV0p2() { }

        public:
            // Factory method to get singleton instance.
            static SimpleSecureFrame32or0BodyTXV0p2 &getInstance();

            // Get TX ID that will be used for transmission; returns false on failure.
            // Argument must be buffer of (at least) OTV0P2BASE::OpenTRV_Node_ID_Bytes bytes.
            virtual bool getTXID(uint8_t *id);

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
            // EEPROM-based implementation...
            // Load the raw form of the persistent reboot/restart message counter from EEPROM into the supplied array.
            // Deals with inversion, but does not interpret the data or check CRCs etc.
            // Separates the EEPROM access from the data interpretation to simplify unit testing.
            // Buffer must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
            // Not ISR-safe.
            static void loadRaw3BytePersistentTXRestartCounterFromEEPROM(uint8_t *loadBuf);
            // Interpret RAM copy of persistent reboot/restart message counter, ie 3 MSBs of message counter; returns false on failure.
            // Combines results from primary and secondary as appropriate,
            // for example to recover from message counter corruption due to a failure during write.
            // TODO: should still do more to (for example) rewrite failed copy for resilience against multiple write failures.
            // Deals with inversion and checksum checking.
            // Input buffer (loadBuf) must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
            // Output buffer (buf) must be 3 bytes long.
            // Will report failure when count is all 0xff values.
            static bool read3BytePersistentTXRestartCounter(const uint8_t *loadBuf, uint8_t *buf);
            // Increment RAM copy of persistent reboot/restart message counter; returns false on failure.
            // Will refuse to increment such that the top byte overflows, ie when already at 0xff.
            // Updates the CRC.
            // Input/output buffer (loadBuf) must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
            static bool increment3BytePersistentTXRestartCounter(uint8_t *loadBuf);
            // Reset the persistent reboot/restart message counter in EEPROM; returns false on failure.
            // TO BE USED WITH EXTREME CAUTION: reusing the message counts and resulting IVs
            // destroys the security of the cipher.
            // Probably only sensible to call this when changing either the ID or the key (or both).
            // This can reset the restart counter to all zeros (erasing the underlying EEPROM bytes),
            // or (default) reset only the most significant bits to zero (preserving device life)
            // but inject entropy into the least significant bits to reduce risk value/IV reuse in error.
            // If called with false then interrupts should not be blocked to allow entropy gathering,
            // and counter is guaranteed to be non-zero.
            //
            // Clears the primary building key first.
            static bool resetRaw3BytePersistentTXRestartCounterInEEPROM(bool allZeros = false);

            // Get the 3 bytes of persistent reboot/restart message counter, ie 3 MSBs of message counter; returns false on failure.
            // Combines results from primary and secondary as appropriate.
            // Deals with inversion and checksum checking.
            // Output buffer (buf) must be 3 bytes long.
            virtual bool get3BytePersistentTXRestartCounter(uint8_t *buf) const;
            // Reset the persistent reboot/restart message counter in EEPROM; returns false on failure.
            // TO BE USED WITH EXTREME CAUTION: reusing the message counts and resulting IVs
            // destroys the security of the cipher.
            // Probably only sensible to call this when changing either the ID or the key (or both).
            // This can reset the restart counter to all zeros (erasing the underlying EEPROM bytes),
            // or (default) reset only the most significant bits to zero (preserving device life)
            // but inject entropy into the least significant bits to reduce risk value/IV reuse in error.
            // If called with false then interrupts should not be blocked to allow entropy gathering,
            // and counter is guaranteed to be non-zero.
            virtual bool resetRaw3BytePersistentTXRestartCounter(bool allZeros = false)
                { return(resetRaw3BytePersistentTXRestartCounterInEEPROM(allZeros)); }
            // Conditional and statically callable version of resetRaw3BytePersistentTXRestartCounter(); returns false on failure.
            // Creates a new persistent/reboot counter and thus message counter, to reduce IV reuse risk.
            // TO BE USED WITH EXTREME CAUTION: reusing the message counts and resulting IVs
            // destroys the security of the cipher.
            // Probably only sensible to call this when changing either the ID or the key (or both).
            // Resets (to a randomised value) the restart counter if significant life has been used, else increments it.
            // Uses singleton instance.
            static bool resetRaw3BytePersistentTXRestartCounterCond()
                {
                SimpleSecureFrame32or0BodyTXV0p2 &i = getInstance();
                uint8_t buf[primaryPeristentTXMessageRestartCounterBytes];
                if(!i.get3BytePersistentTXRestartCounter(buf)) { return(false); }
                if(buf[0] < 0x20) { return(i.increment3BytePersistentTXRestartCounter()); }
                return(i.resetRaw3BytePersistentTXRestartCounterInEEPROM());
                }
            // Increment persistent reboot/restart message counter; returns false on failure.
            // Will refuse to increment such that the top byte overflows, ie when already at 0xff.
            // TO BE USED WITH EXTREME CAUTION: calling this unnecessarily will shorten life before needing to change ID/key.
            virtual bool increment3BytePersistentTXRestartCounter();
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
            //
            // Fills the supplied 6-byte array with the monotonically-increasing primary TX counter.
            // Returns true on success; false on failure for example because the counter has reached its maximum value.
            // Highest-index bytes in the array increment fastest.
            // Not ISR-safe.
            virtual bool incrementAndGetPrimarySecure6BytePersistentTXMessageCounter(uint8_t *buf);

            // Fill in 12-byte IV for 'O'-style (0x80) AESGCM security for a frame to TX.
            // This uses the local node ID as-is for the first 6 bytes if not overridden.
            // This uses and increments the primary message counter for the last 6 bytes.
            // Returns true on success, false on failure eg due to message counter generation failure.
            virtual bool compute12ByteIDAndCounterIVForTX(uint8_t *ivBuf);
        };


    // Variant that allows ID for TX to be fetched on demand, not directly using local node ID.
#define SimpleSecureFrame32or0BodyTXV0p2SuppliedID_DEFINED
    class SimpleSecureFrame32or0BodyTXV0p2SuppliedID : public SimpleSecureFrame32or0BodyTXV0p2
        {
        public:
            // Type of pointer to function to fill in 8-byte ID for TX; returns false on failure.
            typedef bool (*getTXID_t)(uint8_t *);

        private:
            // Function to dynamically compute and fill in 8-byte ID for TX if not NULL.
            const getTXID_t getID;

            // Settable ID to be used for TX ID for subsequent messages.
            // The supplied buffer must be OTV0P2BASE::OpenTRV_Node_ID_Bytes bytes.
            // The supplied ID is copied to internal state, ie the supplied buffer can be temporary.
            // ID must be composed in accordance with the spec, eg if sending TO targeted ID.
            // This will only be used if no function was supplied to the constructor.
            // Note that the primary TX counter will still be used,
            // so gaps will be seen in sequence numbers by recipients.
            //
            // id[0] is 0xff initially, which is nominally invalid, so entire ID is invalid.
            uint8_t id[OTV0P2BASE::OpenTRV_Node_ID_Bytes];

        public:
            // Construct with function that fetches/computes the ID to use for TX or NULL.
            // Where NULL is supplied (the default) then the buffer set by setID is used.
            SimpleSecureFrame32or0BodyTXV0p2SuppliedID(getTXID_t _getID = NULL) : getID(_getID)
              { id[0] = 0xff; }

            // Get TX ID that will be used for transmission; returns false on failure.
            // Argument must be buffer of (at least) OTV0P2BASE::OpenTRV_Node_ID_Bytes bytes.
            virtual bool getTXID(uint8_t *id);

            // Set ID to be used for TX ID for subsequent messages.
            // The supplied buffer must be OTV0P2BASE::OpenTRV_Node_ID_Bytes bytes.
            // The supplied ID is copied to internal state, ie the supplied buffer can be temporary.
            // ID must be composed in accordance with the spec, eg if sending TO targeted ID.
            // This will only be used if no function was supplied to the constructor.
            // Note that the primary TX counter will still be used,
            // so gaps will be seen in sequence numbers by recipients.
            void setTXID(const uint8_t *buf) { memcpy(id, buf, sizeof(id)); }
        };



    // V0p2 RX implementation for 0 or 32 byte encrypted body sections.
    //
    // With all of these routines it is important to check and act on error codes,
    // usually aborting immediately if an error value is returned.
    // MUDDLING ON WITHOUT CHECKING FOR ERRORS MAY SEVERELY DAMAGE SYSTEM SECURITY.
    //
    // Storage format for RX message counters.
    // There are primary and secondary copies at offset 8 and 16 from the start of each association,
    // ie stored in EEPROM against the ID of the leaf being received from.
    // Each has some redundancy so that errors can be detected,
    // eg from partial writes/updated arising from code or power failures.
    //  1) The first 6 bytes of each are the message count, sorted inverted, so as:
    //  1a) to be all zeros from fresh/erased EEPROM
    //  1b) to reduce wear on normal increment
    //      (lsbit goes from 1 to 0 and and nothing else changes
    //      allowing a write without erase on half the increments)
    //  2) The next 'CRC byte contains two elements:
    //  2a) The top bit is cleared/written to zero while the message counter is being updated,
    //      and erased to high when the CRC is written in after the 6 bytes have been updated.
    //      Thus if this is found to be low during a read, a write has failed to complete.
    //  2b) A 7-bit CRC of the message counter bytes, stored inverted,
    //      so that the all-1s erased state of counter and CRC is valid (counter value 0).
#define SimpleSecureFrame32or0BodyRXV0p2_DEFINED
    class SimpleSecureFrame32or0BodyRXV0p2 : public SimpleSecureFrame32or0BodyRXBase
        {
        private:
            // Constructor is private to force use of factory method to return singleton.
            SimpleSecureFrame32or0BodyRXV0p2() { }

        public:
            // Factory method to get singleton instance.
            static SimpleSecureFrame32or0BodyRXV0p2 &getInstance();

            // Read current (last-authenticated) RX message count for specified node, or return false if failed.
            // Will fail for invalid node ID or for unrecoverable memory corruption.
            // Both args must be non-NULL, with counter pointing to enough space to copy the message counter value to.
            virtual bool getLastRXMessageCounter(const uint8_t * const ID, uint8_t *counter) const;
            // Update persistent message counter for received frame AFTER successful authentication.
            // ID is full (8-byte) node ID; counter is full (6-byte) counter.
            // Returns false on failure, eg if message counter is not higher than the previous value for this node.
            // The implementation should allow several years of life typical message rates (see above).
            // The implementation should be robust in the face of power failures / reboots, accidental or malicious,
            // not allowing replays nor other cryptographic attacks, nor forcing node dissociation.
            // Must only be called once the RXed message has passed authentication.
            virtual bool updateRXMessageCountAfterAuthentication(const uint8_t *ID, const uint8_t *newCounterValue);

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
            //
            // TO AVOID RELAY ATTACKS: verify the counter is higher than any previous authed message from this sender
            // then update the RX message counter after a successful auth with this routine.
            virtual uint8_t _decodeSecureSmallFrameFromID(const SecurableFrameHeader *sfh,
                                            const uint8_t *buf, uint8_t buflen,
                                            fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t d,
                                            const uint8_t *adjID, uint8_t adjIDLen,
                                            void *state, const uint8_t *key,
                                            uint8_t *decryptedBodyOut, uint8_t decryptedBodyOutBuflen, uint8_t &decryptedBodyOutSize);

            // From a structurally correct secure frame, looks up the ID, checks the message counter, decodes, and updates the counter if successful.
            // THIS IS THE PREFERRED ENTRY POINT FOR DECODING AND RECEIVING SECURE FRAMES.
            // (Pre-filtering by type and ID and message counter may already have happened.)
            // Note that this is for frames being send from the ID in the header,
            // not for lightweight return traffic to the specified ID.
            // Returns the total number of bytes read for the frame
            // (including, and with a value one higher than the first 'fl' bytes).
            // Returns zero in case of error, eg because authentication failed or this is a duplicate message.
            // If this returns true then the frame is authenticated,
            // and the decrypted body is available if present and a buffer was provided.
            // If the 'firstMatchIDOnly' is true (the default)
            // then this only checks the first ID prefix match found if any,
            // else all possible entries may be tried depending on the implementation
            // and, for example, time/resource limits.
            // This overloading accepts the decryption function, state and key explicitly.
            //
            //  * ID if non-NULL is filled in with the full authenticated sender ID, so must be >= 8 bytes
            virtual uint8_t decodeSecureSmallFrameSafely(const SecurableFrameHeader *sfh,
                                            const uint8_t *buf, uint8_t buflen,
                                            fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t d,
                                            void *state, const uint8_t *key,
                                            uint8_t *decryptedBodyOut, uint8_t decryptedBodyOutBuflen, uint8_t &decryptedBodyOutSize,
                                            uint8_t *ID,
                                            bool firstIDMatchOnly = true);
        };

#endif // ARDUINO_ARCH_AVR


    }


#endif
