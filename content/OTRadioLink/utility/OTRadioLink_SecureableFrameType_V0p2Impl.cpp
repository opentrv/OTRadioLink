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
 */

#include <util/atomic.h>
#include <util/crc16.h>
#include <string.h>

#include "OTRadioLink_SecureableFrameType.h"
#include "OTRadioLink_SecureableFrameType_V0p2Impl.h"

#include "OTV0P2BASE_EEPROM.h"


namespace OTRadioLink
    {


// Factory method to get singleton TX instance.
SimpleSecureFrame32or0BodyTXV0p2 &SimpleSecureFrame32or0BodyTXV0p2::getInstance()
    {
    // Create/initialise on first use, NOT statically.
    static SimpleSecureFrame32or0BodyTXV0p2 instance;
    return(instance);
    }

// Factory method to get singleton RX instance.
SimpleSecureFrame32or0BodyRXV0p2 &SimpleSecureFrame32or0BodyRXV0p2::getInstance()
    {
    // Create/initialise on first use, NOT statically.
    static SimpleSecureFrame32or0BodyRXV0p2 instance;
    return(instance);
    }

// Load the raw form of the persistent reboot/restart message counter from EEPROM into the supplied array.
// Deals with inversion, but does not interpret the data or check CRCs etc.
// Separates the EEPROM access from the data interpretation to simplify unit testing.
// Buffer must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
// Not ISR-safe.
void SimpleSecureFrame32or0BodyTXV0p2::loadRaw3BytePersistentTXRestartCounterFromEEPROM(uint8_t *const loadBuf)
    {
    if(NULL == loadBuf) { return; }
    eeprom_read_block(loadBuf,
                    (uint8_t *)(OTV0P2BASE::VOP2BASE_EE_START_PERSISTENT_MSG_RESTART_CTR),
                    OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR);
    // Invert all the bytes.
    for(uint8_t i = 0; i < OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR; ) { loadBuf[i++] ^= 0xff; }
    }

// Save the raw form of the persistent reboot/restart message counter to EEPROM from the supplied array.
// Deals with inversion, but does not interpret the data.
// Separates the EEPROM access from the data interpretation to simplify unit testing.
// Uses a smart update for each byte and ensures that each byte appears to read back correctly
// else fails with a false return value, which may or may not leave an intact good value in place.
// Buffer must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
//   * loadBuf  area to save from; never NULL
// Not ISR-safe.
static bool saveRaw3BytePersistentTXRestartCounterToEEPROM(const uint8_t *const loadBuf)
    {
    //if(NULL == loadBuf) { return(false); }
    // Invert all the bytes and write them back carefully testing each OK before starting the next.
    for(uint8_t i = 0; i < OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR; ++i)
        {
        const uint8_t b = ~loadBuf[i];
        OTV0P2BASE::eeprom_smart_update_byte((uint8_t *)(OTV0P2BASE::VOP2BASE_EE_START_PERSISTENT_MSG_RESTART_CTR) + i, b);
        if(b != eeprom_read_byte((uint8_t *)(OTV0P2BASE::VOP2BASE_EE_START_PERSISTENT_MSG_RESTART_CTR) + i)) { return(false); }
        }
    return(true);
    }

// Reset the persistent reboot/restart message counter in EEPROM; returns false on failure.
// TO BE USED WITH EXTREME CAUTION as reusing the message counts and resulting IVs
// destroys the security of the cipher.
// Probably only sensible to call this when changing either the ID or the key (or both).
// This can reset the restart counter to all zeros (erasing the underlying EEPROM bytes),
// or (default) reset only the most significant bits to zero (preserving device life)
// but inject entropy into the least significant bits to reduce risk value/IV reuse in error.
// If called with false then interrupts should not be blocked to allow entropy gathering,
// and counter is guaranteed to be non-zero.
//
// Clears the primary building key first.
bool SimpleSecureFrame32or0BodyTXV0p2::resetRaw3BytePersistentTXRestartCounterInEEPROM(const bool allZeros)
    {
    // Clear the primary building key.
    if(!OTV0P2BASE::setPrimaryBuilding16ByteSecretKey(NULL)) { return(false); }
    // Reset the counter.
    if(allZeros)
        {
        // Erase everything, leaving counter all-zeros with correct (0) CRC.
        for(uint8_t i = 0; i < OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR; ++i)
            {
            OTV0P2BASE::eeprom_smart_erase_byte((uint8_t *)(OTV0P2BASE::VOP2BASE_EE_START_PERSISTENT_MSG_RESTART_CTR) + i);
            if(0xff != eeprom_read_byte((uint8_t *)(OTV0P2BASE::VOP2BASE_EE_START_PERSISTENT_MSG_RESTART_CTR) + i)) { return(false); }
            }
        }
    else
        {
        // Make only msbits zero, and fill rest with entropy and reset the CRC.
        // Buffer for noise bytes; msbits will be kept as zero.  Tack CRC on the end.
        // Then duplicate to second half for backup copy.
        uint8_t noise[OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR];
        for(uint8_t i = 0; i < primaryPeristentTXMessageRestartCounterBytes; ) { noise[i++] = OTV0P2BASE::getSecureRandomByte(); }
        noise[0] = 0xf & (noise[0] ^ (noise[0] >> 4)); // Keep top 4 bits clear to preserve > 90% of possible life.
        // Ensure that entire sequence is non-zero by forcing lsb to 1 (if enough of) noise seems to be 0.
        if(/*(0 == noise[primaryPeristentTXMessageRestartCounterBytes-1]) && */ (0 == noise[1]) && (0 == noise[0])) { noise[primaryPeristentTXMessageRestartCounterBytes-1] |= 1; }
        // Compute CRC for new value.
        uint8_t crc = 0;
        for(int i = 0; i < primaryPeristentTXMessageRestartCounterBytes; ++i) { crc = _crc8_ccitt_update(crc, noise[i]); }
        noise[primaryPeristentTXMessageRestartCounterBytes] = crc;
#if 0
OTV0P2BASE::serialPrintAndFlush(F("CRC computed "));
OTV0P2BASE::serialPrintAndFlush(crc, HEX);
OTV0P2BASE::serialPrintlnAndFlush();
#endif
        memcpy(noise + 4, noise, 4);
        saveRaw3BytePersistentTXRestartCounterToEEPROM(noise);
        }
    return(true);
    }

// Read exactly one of the copies of the persistent reboot/restart message counter; returns false on failure.
static bool readOne3BytePersistentTXRestartCounter(const uint8_t *const base, uint8_t *const buf)
    {
    // FIXME: for now use the primary copy only if OK: should be able to salvage from secondary, else take higher+1.
    // Fail if the CRC is not valid.
    uint8_t crc = 0;
    for(int i = 0; i < SimpleSecureFrame32or0BodyTXBase::primaryPeristentTXMessageRestartCounterBytes; ++i) { crc = _crc8_ccitt_update(crc, base[i]); }
#if 0
OTV0P2BASE::serialPrintAndFlush(F("CRC expected vs actual "));
OTV0P2BASE::serialPrintAndFlush(crc, HEX);
OTV0P2BASE::serialPrintAndFlush(' ');
OTV0P2BASE::serialPrintAndFlush(base[SimpleSecureFrame32or0BodyBase::primaryPeristentTXMessageRestartCounterBytes], HEX);
OTV0P2BASE::serialPrintlnAndFlush();
#endif
    if(crc != base[SimpleSecureFrame32or0BodyTXBase::primaryPeristentTXMessageRestartCounterBytes]) { /* OTV0P2BASE::serialPrintlnAndFlush(F("CRC failed")); */ return(false); } // CRC failed.
    // Check for all 0xff (maximum) value and fail if found.
    if((0xff == base[0]) && (0xff == base[1]) && (0xff == base[2])) { /* OTV0P2BASE::serialPrintlnAndFlush(F("counter at max")); */ return(false); } // Counter at max.
    // Copy (primary) counter to output.
    for(int i = 0; i < SimpleSecureFrame32or0BodyTXBase::primaryPeristentTXMessageRestartCounterBytes; ++i) { buf[i] = base[i]; }
    return(true);
    }

// Interpret RAM copy of persistent reboot/restart message counter, ie 3 MSBs of message counter; returns false on failure.
// Combines results from primary and secondary as appropriate,
// for example to recover from message counter corruption due to a failure during write.
// TODO: should still do more to (for example) rewrite failed copy for resilience against multiple write failures.
// Deals with inversion and checksum checking.
// Input buffer (loadBuf) must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
// Output buffer (buf) must be 3 bytes long.
// Will report failure when count is all 0xff values.
bool SimpleSecureFrame32or0BodyTXV0p2::read3BytePersistentTXRestartCounter(const uint8_t *const loadBuf, uint8_t *const buf)
    {
    // Read the primary copy.
    if(readOne3BytePersistentTXRestartCounter(loadBuf, buf)) { return(true); }
    // Failing that try the secondary copy.
    return(readOne3BytePersistentTXRestartCounter(loadBuf + 4, buf));
    }

// Get the 3 bytes of persistent reboot/restart message counter, ie 3 MSBs of message counter; returns false on failure.
// Combines results from primary and secondary as appropriate.
// Deals with inversion and checksum checking.
// Output buffer (buf) must be 3 bytes long.
bool SimpleSecureFrame32or0BodyTXV0p2::get3BytePersistentTXRestartCounter(uint8_t *const buf) const
    {
    uint8_t loadBuf[OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR];
    loadRaw3BytePersistentTXRestartCounterFromEEPROM(loadBuf);
    return(read3BytePersistentTXRestartCounter(loadBuf, buf));
    }

// Increment RAM copy of persistent reboot/restart message counter; returns false on failure.
// Will refuse to increment such that the top byte overflows, ie when already at 0xff.
// Updates the CRC.
// Input/output buffer (loadBuf) must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
bool SimpleSecureFrame32or0BodyTXV0p2::increment3BytePersistentTXRestartCounter(uint8_t *const loadBuf)
    {
    uint8_t buf[primaryPeristentTXMessageRestartCounterBytes];
    if(!read3BytePersistentTXRestartCounter(loadBuf, buf)) { return(false); }
    for(uint8_t i = primaryPeristentTXMessageRestartCounterBytes; i-- > 0; )
        {
        if(0 != ++buf[i]) { break; }
        if(0 == i) { return(false); } // Overflow from top byte not permitted.
        }
    // Compute the CRC.
    uint8_t crc = 0;
    for(int i = 0; i < primaryPeristentTXMessageRestartCounterBytes; ++i) { crc = _crc8_ccitt_update(crc, buf[i]); }
    // Write both copies, primary and alternate.
    // FIXME: alternate in future to halve write cost on LSB/CRC bytes, eg always write even to primary, odd to alt.
    for(uint8_t *base = loadBuf; base <= loadBuf + 4; base += 4)
        {
        for(int i = 0; i < primaryPeristentTXMessageRestartCounterBytes; ++i) { base[i] = buf[i]; }
        base[primaryPeristentTXMessageRestartCounterBytes] = crc;
        }
    return(true);
    }

// Increment EEPROM copy of persistent reboot/restart message counter; returns false on failure.
// Will refuse to increment such that the top byte overflows, ie when already at 0xff.
// USE WITH CARE: calling this unnecessarily will shorten life before needing to change ID/key.
bool SimpleSecureFrame32or0BodyTXV0p2::increment3BytePersistentTXRestartCounter()
    {
    // Increment the persistent part; fail entirely if not usable/incrementable (eg all 0xff).
    uint8_t loadBuf[OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR];
    loadRaw3BytePersistentTXRestartCounterFromEEPROM(loadBuf);
    if(!increment3BytePersistentTXRestartCounter(loadBuf)) { return(false); }
    if(!saveRaw3BytePersistentTXRestartCounterToEEPROM(loadBuf)) { return(false); }
    return(true);
    }

// Fills the supplied 6-byte array with the incremented monotonically-increasing primary TX counter.
// Returns true on success; false on failure for example because the counter has reached its maximum value.
// Highest-index bytes in the array increment fastest.
// This should never return an all-zero count.
// Not ISR-safe.
bool SimpleSecureFrame32or0BodyTXV0p2::incrementAndGetPrimarySecure6BytePersistentTXMessageCounter(uint8_t *const buf)
    {
    if(NULL == buf) { return(false); }

    // False when first called, ie on first call to this routine after board boot/restart.
    // Used to drive roll of persistent part
    // and initialisation of non-persistent part.
    static bool initialised;
    const bool doInitialisation = !initialised;
    if(doInitialisation) { initialised = true; }
    bool incrementPersistent = false;

    // VITAL FOR CIPHER SECURITY: increase value of restart/reboot counter before first use after (re)boot.
    // Security improvement: if initialising and persistent/restart part is all zeros
    // then force it to an entropy-laden non-zero value that still leaves most of its lifetime.
    // Else simply increment it as per the expected restart counter behaviour.
    // NOTE: AS A MINIMUM the restart counter must be incremented here on initialisation.
    if(doInitialisation)
        {
        if(!get3BytePersistentTXRestartCounter(buf)) { return(false); }
        if((0 == buf[0]) && (0 == buf[1]) && (0 == buf[2]))
            { if(!resetRaw3BytePersistentTXRestartCounterInEEPROM(false)) { return(false); } }
        else
            { incrementPersistent = true; }
        }

    // Ephemeral (non-persisted) least-significant bytes of message count.
    static uint8_t ephemeral[3];

    // Temporary area for initialising ephemeral[] where needed.
    uint8_t tmpE[sizeof(ephemeral)];
    if(doInitialisation)
        {
        for(uint8_t i = sizeof(tmpE); i-- > 0; )
            { tmpE[i] = OTV0P2BASE::getSecureRandomByte(); } // Doesn't like being called with interrupts off.
        // Mask off top bits of top (most significant byte) to preserve most of the remaining counter life
        // but allow ~20 bits ie a decent chunk of 1 million messages
        // (maybe several years at a message every 4 minutes)
        // before likely IV reuse even with absence/failure of the restart counter.
        tmpE[0] = 0xf & (tmpE[0] ^ (tmpE[0] >> 4));
        }

    // Disable interrupts while adjusting counter and copying back to the caller.
    // Though since it is slow, incrementing the persistent counter (when done) is outside this block.
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
        if(doInitialisation)
            {
            // Fill lsbs of ephemeral part with entropy so as not to reduce lifetime significantly.
            memcpy(ephemeral+max(0,sizeof(ephemeral)-sizeof(tmpE)), tmpE, min(sizeof(tmpE), sizeof(ephemeral)));
            }

        // Increment the counter including the persistent part where necessary.
        for(uint8_t i = sizeof(ephemeral); i-- > 0; )
            {
            if(0 != ++ephemeral[i]) { break; }
            if(0 == i)
                {
                // Prepare to increment the persistent part below.
                incrementPersistent = true;
                }
            }

        // Copy in the ephemeral part.
        memcpy(buf + 3, ephemeral, 3);
        }

    // Increment persistent part if necessary.
    // Done outside atomic block as potentially slow (worst-case 8 EEPROM full writes).
    if(incrementPersistent)
        {
        // Increment the persistent part; fail entirely if not usable/incrementable (eg at max value).
        if(!increment3BytePersistentTXRestartCounter()) { return(false); }
        }

    // Copy in the persistent part; fail entirely if it is not usable.
    if(!get3BytePersistentTXRestartCounter(buf)) { return(false); }

    return(true);
    }

// Read RX message count from specified EEPROM location; fails if CRC fails.
// First 6 bytes are counter MSB first, followed by CRC.
//  * eepromLOC  pointer into Flash for this counter instance; never NULL
//  * counter  buffer to load counter to; never NULL
// Pays no attention to the unary counter.
static bool getLastRXMessageCounterFromTable(const uint8_t * const eepromLoc, uint8_t * const counter)
    {
//    if((NULL == eepromLoc) || (NULL == counter)) { return(false); } // FAIL
    // First get the 6 bytes (inverted) from the start of the given region.
    // The values are inverted so as:
    //   * to be all zeros from fresh/erased EEPROM
    //   * to reduce wear on normal increment
    //     (lsbit goes from 1 to 0 and and nothing else changes
    //     allowing a write without erase on half the increments)
    eeprom_read_block(counter, eepromLoc, SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes);
    for(uint8_t i = 0; i < SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes; ++i) { counter[i] ^= 0xff; }
    // Now check the CRC byte (immediately following the counter):
    //  1) Fail if the top bit was clear indicating an update in progress...
    //  2) Fail if the CRC itself does not match.
    // The two operations can be performed at once since the CRC msb should be 0, ie 1 when inverted.
    const uint8_t crcRAW = eeprom_read_byte(eepromLoc + SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes);
    // Compute/validate the 7-bit CRC.
    uint8_t crc = 0;
    for(int i = 0; i < SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes; ++i) { crc = OTV0P2BASE::crc7_5B_update(crc, counter[i]); }
    if(crc != (uint8_t)~crcRAW) { /* OTV0P2BASE::serialPrintlnAndFlush(F("!RXmc")); */ return(false); } // FAIL
    return(true); // Done!
    }

// If true, use unary counter in final bytes of primary and secondary counter
// to reduce EEPROM wear by a factor of ~17,
// nominally extending life to over 20Y at 15 messages per hour (>10Y at 30msg/h),
// where equipment lifetime is expected to be around 10Y max.
static const bool use_unary_counter = true;

// Read current (last-authenticated) RX message count for specified node, or return false if failed.
// Deals with any redundancy/corruption etc.
// Will fail for invalid node ID and for unrecoverable memory corruption.
// Uses unary count across 2 bytes (primary and secondary) to give up to 17 RXes before needing to update main counters.
// Both args must be non-NULL, with counter pointing to enough space to copy the message counter value to.
bool SimpleSecureFrame32or0BodyRXV0p2::getLastRXMessageCounter(const uint8_t * const ID, uint8_t * const counter) const
    {
    // Rely on getNextMatchingNodeID() to reject a NULL ID with a non-zero length.
    if(NULL == counter) { return(false); } // FAIL
    // First look up the node association; fail if not present.
    const int8_t index = OTV0P2BASE::getNextMatchingNodeID(0, ID, OTV0P2BASE::OpenTRV_Node_ID_Bytes, NULL);
    if(index < 0) { return(false); } // FAIL
    // Note: nominal risk of race if associations table can be altered concurrently.
    // Compute base location in EEPROM of association table entry/row.
    uint8_t * const rawPtr = (uint8_t *)(OTV0P2BASE::V0P2BASE_EE_START_NODE_ASSOCIATIONS + index*(uint16_t)OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE);
    // Read low-wear unary increment value from trailing bytes.
    // Use primary 'spare' byte as most significant.
    // In case of error in the increment value treat it as the largest-possible value
    // which is safe (prevents replay) but may cause up to 16 messages to be ignored.
    // Assume that the high redundancy in the increment value will catch much possible random corruption,
    // though failing to complete clearing a bit may allow a replay of the last message.
    const uint8_t incr = (!use_unary_counter) ? 0 :
        OTV0P2BASE::eeprom_unary_2byte_decode(eeprom_read_byte(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_0_OFFSET + 7),
                                              eeprom_read_byte(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_1_OFFSET + 7));
    const uint8_t appliedIncr = (incr >= 0) ? incr : (OTV0P2BASE::EEPROM_UNARY_2BYTE_MAX_VALUE);
    // Try primary then secondary (both will be written to each time).
    if(!getLastRXMessageCounterFromTable(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_0_OFFSET, counter) &&
       !getLastRXMessageCounterFromTable(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_1_OFFSET, counter))
       { return(false); } // FAIL: both counters borked.
    return(use_unary_counter ? SimpleSecureFrame32or0BodyBase::msgcounteradd(counter, appliedIncr) : true);
    }

// Carefully update specified counter (primary or secondary) and CRCs as appropriate; returns false on failure.
// Sets write-in-progress flag before starting and clears it (sets it to 1) with the CRC afterwards.
// Reads back each byte written before proceeding.
// Pays no attention to the unary counter.
static bool updateRXMessageCount(uint8_t * const eepromLoc, const uint8_t * const newCounterValue)
    {
    // First set the write-in-progress flag (clear to 0), msbit of the CRC byte...
    uint8_t * const CRCptr = eepromLoc + SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes;
    OTV0P2BASE::eeprom_smart_clear_bits(CRCptr, 0x7f);
    // Compute 7-bit CRC to use at the end, with the write-in-progress flag off (1).
    uint8_t crc = 0;
    for(int i = 0; i < SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes; ++i) { crc = OTV0P2BASE::crc7_5B_update(crc, newCounterValue[i]); }
    const uint8_t rawCRC = ~crc; // The CRC's high-bit should be 0, so 1 when inverted.
    // Byte-by-byte careful minimal update of EEPROM, checking after each byte, ie for gross immediate failure.
    uint8_t *p = eepromLoc;
    for(int i = 0; i < SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes; ++i, ++p)
        {
        const uint8_t asWritten = ~newCounterValue[i];
        OTV0P2BASE::eeprom_smart_update_byte(p, asWritten);
        if(asWritten != eeprom_read_byte(p)) { return(false); } // FAIL
        }
    // Write CRC byte, clearing write-in-progress flag to 1...
    OTV0P2BASE::eeprom_smart_update_byte(CRCptr, rawCRC);
    if(rawCRC != eeprom_read_byte(CRCptr)) { return(false); } // FAIL
    // Done.
    return(true);
    }

// Update persistent message counter for received frame AFTER successful authentication.
// ID is full (8-byte) node ID; counter is full (6-byte) counter.
// Returns false on failure, eg if message counter is not higher than the previous value for this node.
// The implementation should allow several years of life typical message rates (see above).
// The implementation should be robust in the face of power failures / reboots, accidental or malicious,
// not allowing replays nor other cryptographic attacks, nor forcing node dissociation.
// Must only be called once the RXed message has passed authentication.
// Use a unary count as proxy for LSBs to reduce wear; clear unary value after main count increment so as to never have too low a total value.
bool SimpleSecureFrame32or0BodyRXV0p2::updateRXMessageCountAfterAuthentication(const uint8_t *ID, const uint8_t *newCounterValue)
    {
    // Validate node ID and new count.
    if(!validateRXMessageCount(ID, newCounterValue)) { return(false); } // Putative new counter value not valid; reject.
    // Look up the node association; fail if not present.
    const int8_t index = OTV0P2BASE::getNextMatchingNodeID(0, ID, OTV0P2BASE::OpenTRV_Node_ID_Bytes, NULL);
    if(index < 0) { return(false); } // FAIL (shouldn't be possible after previous validation).
    // Note: nominal risk of race if associations table can be altered concurrently.
    // Compute base location in EEPROM of association table entry/row.
    uint8_t * const rawPtr = (uint8_t *)(OTV0P2BASE::V0P2BASE_EE_START_NODE_ASSOCIATIONS + index*(uint16_t)OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE);
    if(!use_unary_counter)
        {
        // Update primary AND secondary counter copies directly; don't use unary counter.
        if(!updateRXMessageCount(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_0_OFFSET, newCounterValue)) { return(false); } // FAIL
        if(!updateRXMessageCount(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_1_OFFSET, newCounterValue)) { return(false); } // FAIL
        return(true);
        }

//OTV0P2BASE::serialPrintlnAndFlush(F("updateRXc"));
    // If the counter can be updated using just the unary part then do so to reduce EEPROM wear.
    // Else update the primary/secondary counters to the new value and reset the unary value.
    //
    // Get the raw counter value ignoring the unary part.
    // Fall back to the secondary value if there is something wrong with the primary,
    // and fail entirely if the secondary is also broken.
    uint8_t baseCount[fullMessageCounterBytes];
    if(!getLastRXMessageCounterFromTable(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_0_OFFSET, baseCount) &&
       !getLastRXMessageCounterFromTable(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_1_OFFSET, baseCount))
        { return(false); } // FAIL: both copies borked.
    // Compute the maximum value that the base value could be extended to with the unary part.
    uint8_t maxWithUnary[fullMessageCounterBytes];
    memcpy(maxWithUnary, baseCount, sizeof(maxWithUnary));
    if(!SimpleSecureFrame32or0BodyBase::msgcounteradd(maxWithUnary, OTV0P2BASE::EEPROM_UNARY_2BYTE_MAX_VALUE)) { return(false); } // FAIL: counter too near maximum; might roll.
    // If that is at least as large as the requested new counter value
    // (AND there was not a problem reading the unary part)
    if(SimpleSecureFrame32or0BodyBase::msgcountercmp(maxWithUnary, newCounterValue) >= 0)
    // then just update the unary value as needed ...
        {
        // Get the current unary counter part...
        const uint8_t currentIncr = (!use_unary_counter) ? 0 :
            OTV0P2BASE::eeprom_unary_2byte_decode(eeprom_read_byte(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_0_OFFSET + 7),
                                                  eeprom_read_byte(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_1_OFFSET + 7));
        // If impossible to read back the existing value then use 0 for a slightly longer search below.
        const uint8_t startIncr = (currentIncr >= 0) ? currentIncr : 0;
//        const uint8_t startIncr = 0;
        // Try successively larger increments with the unary counter
        // until the total of the base and unary counts is the requested new counter value,
        // then set the unary counter to that value and return a success value.
        // In most cases this will take a single increment
        // as messages will arrive with successive message counter values, barring comms loss.
        for(uint8_t newIncr = startIncr; newIncr <= OTV0P2BASE::EEPROM_UNARY_2BYTE_MAX_VALUE; ++newIncr)
            {
            uint8_t putativeTotal[fullMessageCounterBytes];
            memcpy(putativeTotal, baseCount, sizeof(putativeTotal));
            if(!SimpleSecureFrame32or0BodyBase::msgcounteradd(putativeTotal, newIncr)) { return(false); } // FAIL: counter too near maximum; might roll.
            if(SimpleSecureFrame32or0BodyBase::msgcountercmp(putativeTotal, newCounterValue) == 0)
                {
                // Got it!
                const uint16_t newU16 = OTV0P2BASE::eeprom_unary_2byte_encode(newIncr);
                const uint8_t vm = (uint8_t)(newU16 >> 8);
                const uint8_t vl = (uint8_t)(newU16);
                // Update in a way easy to detect if interrupted, eg by power failure, so lsbyte first,
                // though usually only one bit will actually be changing...
                OTV0P2BASE::eeprom_smart_update_byte(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_1_OFFSET + 7, vl);
                OTV0P2BASE::eeprom_smart_update_byte(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_0_OFFSET + 7, vm);
//OTV0P2BASE::serialPrintlnAndFlush(F("updateRXcUnaryOnly"));
                return(true); // DONE
                }
            }
        return(false); // FAIL: should not really be possible.
        }
    // else update the underlying main counters, primary AND secondary copies ...
//OTV0P2BASE::serialPrintlnAndFlush(F("updateRXcFull"));
    if(!updateRXMessageCount(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_0_OFFSET, newCounterValue)) { return(false); } // FAIL
    if(!updateRXMessageCount(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_1_OFFSET, newCounterValue)) { return(false); } // FAIL
    // ... and reset the unary counter,
    // in a way easy to detect if interrupted, eg by power failure, so lsbyte first.
    OTV0P2BASE::eeprom_smart_erase_byte(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_1_OFFSET + 7);
    OTV0P2BASE::eeprom_smart_erase_byte(rawPtr + OTV0P2BASE::V0P2BASE_EE_NODE_ASSOCIATIONS_MSG_CNT_0_OFFSET + 7);
    return(true);
    }

// As for decodeSecureSmallFrameRaw() but passed a candidate node/counterparty ID
// derived from the frame ID in the incoming header,
// plus possible other adjustments such has forcing bit values for reverse flows.
// This routine constructs an IV from this expanded ID
// (which must be at least length 6 for 'O' / 0x80 style enc/auth)
// and other information in the header
// and then returns the result of calling decodeSecureSmallFrameRaw().
// Returns the total number of bytes read for the frame
// (including, and with a value one higher than the first 'fl' bytes).
// Returns zero in case of error, eg because authentication failed.
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
uint8_t SimpleSecureFrame32or0BodyRXV0p2::_decodeSecureSmallFrameFromID(const SecurableFrameHeader *const sfh,
                                const uint8_t *const buf, const uint8_t buflen,
                                const fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t d,
                                const uint8_t *const adjID, const uint8_t adjIDLen,
                                void *const state, const uint8_t *const key,
                                uint8_t *const decryptedBodyOut, const uint8_t decryptedBodyOutBuflen, uint8_t &decryptedBodyOutSize)
    {
    // Rely on decodeSecureSmallFrameRaw() for validation of items not directly needed here.
    if((NULL == sfh) || (NULL == buf) || (NULL == adjID)) { return(0); } // ERROR
    if(adjIDLen < 6) { return(0); } // ERROR
    // Abort if header was not decoded properly.
    if(sfh->isInvalid()) { return(0); } // ERROR
    // Abort if expected constraints for simple fixed-size secure frame are not met.
    if(23 != sfh->getTl()) { return(0); } // ERROR
//    const uint8_t fl = sfh->fl;
//    if(0x80 != buf[fl]) { return(0); } // ERROR
    if(sfh->getTrailerOffset() + 6 > buflen) { return(0); } // ERROR
    // Construct IV from supplied (possibly adjusted) ID + counters from (start of) trailer.
    uint8_t iv[12];
    memcpy(iv, adjID, 6);
    memcpy(iv + 6, buf + sfh->getTrailerOffset(), SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes);
    // Now do actual decrypt/auth.
    return(decodeSecureSmallFrameRaw(sfh,
                                buf, buflen,
                                d,
                                state, key, iv,
                                decryptedBodyOut, decryptedBodyOutBuflen, decryptedBodyOutSize));
    }

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
uint8_t SimpleSecureFrame32or0BodyRXV0p2::decodeSecureSmallFrameSafely(const SecurableFrameHeader *const sfh,
                                const uint8_t *const buf, const uint8_t buflen,
                                const fixed32BTextSize12BNonce16BTagSimpleDec_ptr_t d,
                                void *const state, const uint8_t *const key,
                                uint8_t *const decryptedBodyOut, const uint8_t decryptedBodyOutBuflen, uint8_t &decryptedBodyOutSize,
                                uint8_t *const ID,
                                bool /*firstIDMatchOnly*/)
    {
    // Rely on _decodeSecureSmallFrameFromID() for validation of items not directly needed here.
    if((NULL == sfh) || (NULL == buf)) { return(0); } // ERROR
    // Abort if header was not decoded properly.
    if(sfh->isInvalid()) { return(0); } // ERROR
//    // Abort if frame is not secure.
//    if(sfh->isSecure()) { return(0); } // ERROR
    // Abort if trailer not large enough to extract message counter from safely (and not expected size/flavour).
    if(23 != sfh->getTl()) { return(0); } // ERROR
    // Look up the full node ID of the sender in the associations table.
    // NOTE: this only tries the first match, ignoring firstIDMatchOnly.
    uint8_t senderNodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes];
    const int8_t index = OTV0P2BASE::getNextMatchingNodeID(0, sfh->id, sfh->getIl(), senderNodeID);
    if(index < 0) { return(0); } // ERROR
    // Extract the message counter and validate it (that it is higher than previously seen)...
    uint8_t messageCounter[SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes];
    // Assume counter positioning as for 0x80 type trailer, ie 6 bytes at start of trailer.
    memcpy(messageCounter, buf + sfh->getTrailerOffset(), SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes);
    if(!validateRXMessageCount(senderNodeID, messageCounter)) { return(0); } // ERROR
    // Now attempt to decrypt.
    // Assumed no need to 'adjust' ID for this form of RX.
    const uint8_t decodeResult =_decodeSecureSmallFrameFromID(sfh,
                                                        buf, buflen,
                                                        d,
                                                        senderNodeID, OTV0P2BASE::OpenTRV_Node_ID_Bytes,
                                                        state, key,
                                                        decryptedBodyOut, decryptedBodyOutBuflen, decryptedBodyOutSize);
    if(0 == decodeResult) { return(0); } // ERROR
    // Successfully decoded: update the RX message counter to avoid duplicates/replays.
    if(!updateRXMessageCountAfterAuthentication(senderNodeID, messageCounter)) { return(0); } // ERROR
    // Success: copy sender ID to output buffer (if non-NULL) as last action.
    if(ID != NULL) { memcpy(ID, senderNodeID, OTV0P2BASE::OpenTRV_Node_ID_Bytes); }
    return(decodeResult);
    }

// Get TX ID that will be used for transmission; returns false on failure.
// Argument must be buffer of (at least) OTV0P2BASE::OpenTRV_Node_ID_Bytes bytes.
bool SimpleSecureFrame32or0BodyTXV0p2::getTXID(uint8_t *const idOut)
    {
    if(NULL == idOut) { return(false); }
    // Copy ID from EEPROM.
    eeprom_read_block(idOut, (uint8_t *)V0P2BASE_EE_START_ID, OTV0P2BASE::OpenTRV_Node_ID_Bytes);
    return(true);
    }

// Fill in 12-byte IV for 'O'-style (0x80) AESGCM security for a frame to TX.
// This dynamically fetches the built-in TX ID (eg from EEPROM or as supplied) for the first 6 bytes.
// This uses and increments the primary message counter for the last 6 bytes.
// Returns true on success, false on failure eg due to message counter generation failure.
bool SimpleSecureFrame32or0BodyTXV0p2::compute12ByteIDAndCounterIVForTX(uint8_t *const ivBuf)
    {
    if(NULL == ivBuf) { return(false); }
    // Fetch entire ID directly to ivBuf for simplicity; lsbytes will be overwritten with message counter.
    if(!getTXID(ivBuf)) { return(false); } // ID fetch failed.
    // Generate and fill in new message count at end of IV.
    return(incrementAndGetPrimarySecure6BytePersistentTXMessageCounter(ivBuf + (12-SimpleSecureFrame32or0BodyBase::fullMessageCounterBytes)));
    }


// Get TX ID that will be used for transmission; returns false on failure.
// Argument must be buffer of (at least) OTV0P2BASE::OpenTRV_Node_ID_Bytes bytes.
bool SimpleSecureFrame32or0BodyTXV0p2SuppliedID::getTXID(uint8_t *const idOut)
    {
    if(NULL == idOut) { return(false); }
    // Without the fetch function, this copies from the internal ID buffer.
    if(NULL == getID) { memcpy(idOut, id, OTV0P2BASE::OpenTRV_Node_ID_Bytes); return(true); }
    // Dynamically fetch/compute ID.
    return(getID(idOut));
    }


    }
