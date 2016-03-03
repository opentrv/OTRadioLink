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
#include <string.h>

#include "OTRadioLink_SecureableFrameType.h"
#include "OTRadioLink_SecureableFrameType_V0p2Impl.h"

#include "OTV0P2BASE_EEPROM.h"


namespace OTRadioLink
    {


// Load the raw form of the persistent reboot/restart message counter from EEPROM into the supplied array.
// Deals with inversion, but does not interpret the data or check CRCs etc.
// Separates the EEPROM access from the data interpretation to simplify unit testing.
// Buffer must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
// Not ISR-safe.
void SimpleSecureFrame32or0BodyV0p2::loadRaw3BytePersistentTXRestartCounterFromEEPROM(uint8_t *const loadBuf)
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
// Not ISR-safe.
static bool saveRaw3BytePersistentTXRestartCounterToEEPROM(const uint8_t *const loadBuf)
    {
    if(NULL == loadBuf) { return(false); }
    // Invert all the bytes and write them back carefully testing each OK before starting the next.
    for(uint8_t i = 0; i < OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR; ++i)
        {
        const uint8_t b = loadBuf[i] ^ 0xff;
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
bool SimpleSecureFrame32or0BodyV0p2::resetRaw3BytePersistentTXRestartCounterInEEPROM(const bool allZeros)
    {
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
        if((0 == noise[primaryPeristentTXMessageRestartCounterBytes-1]) && (0 == noise[1]) && (0 == noise[0])) { noise[primaryPeristentTXMessageRestartCounterBytes-1] = 1; }
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
    // FIXME: for now use the primary copy only: should be able to salvage from secondary, else take higher+1.
    // Fail if the CRC is not valid.
    uint8_t crc = 0;
    for(int i = 0; i < primaryPeristentTXMessageRestartCounterBytes; ++i) { crc = _crc8_ccitt_update(crc, base[i]); }
#if 0
OTV0P2BASE::serialPrintAndFlush(F("CRC expected vs actual "));
OTV0P2BASE::serialPrintAndFlush(crc, HEX);
OTV0P2BASE::serialPrintAndFlush(' ');
OTV0P2BASE::serialPrintAndFlush(base[primaryPeristentTXMessageRestartCounterBytes], HEX);
OTV0P2BASE::serialPrintlnAndFlush();
#endif
    if(crc != base[primaryPeristentTXMessageRestartCounterBytes]) { /* OTV0P2BASE::serialPrintlnAndFlush(F("CRC failed")); */ return(false); } // CRC failed.
    // Check for all 0xff (maximum) value and fail if found.
    if((0xff == base[0]) && (0xff == base[1]) && (0xff == base[2])) { /* OTV0P2BASE::serialPrintlnAndFlush(F("counter at max")); */ return(false); } // Counter at max.
    // Copy (primary) counter to output.
    for(int i = 0; i < primaryPeristentTXMessageRestartCounterBytes; ++i) { buf[i] = base[i]; }
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
bool SimpleSecureFrame32or0BodyV0p2::read3BytePersistentTXRestartCounter(const uint8_t *const loadBuf, uint8_t *const buf)
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
bool SimpleSecureFrame32or0BodyV0p2::get3BytePersistentTXRestartCounter(uint8_t *const buf) const
    {
    uint8_t loadBuf[OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR];
    loadRaw3BytePersistentTXRestartCounterFromEEPROM(loadBuf);
    return(read3BytePersistentTXRestartCounter(loadBuf, buf));
    }

// Increment RAM copy of persistent reboot/restart message counter; returns false on failure.
// Will refuse to increment such that the top byte overflows, ie when already at 0xff.
// Updates the CRC.
// Input/output buffer (loadBuf) must be VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR bytes long.
bool SimpleSecureFrame32or0BodyV0p2::increment3BytePersistentTXRestartCounter(uint8_t *const loadBuf)
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
bool SimpleSecureFrame32or0BodyV0p2::increment3BytePersistentTXRestartCounter()
    {
    // Increment the persistent part; fail entirely if not usable/incrementable (eg all 0xff).
    uint8_t loadBuf[OTV0P2BASE::VOP2BASE_EE_LEN_PERSISTENT_MSG_RESTART_CTR];
    loadRaw3BytePersistentTXRestartCounterFromEEPROM(loadBuf);
    if(!increment3BytePersistentTXRestartCounter(loadBuf)) { return(false); }
    if(!saveRaw3BytePersistentTXRestartCounterToEEPROM(loadBuf)) { return(false); }
    return(true);
    }


// Fill in 12-byte IV for 'O'-style (0x80) AESGCM security for a frame to TX.
// This uses the local node ID as-is for the first 6 bytes.
// This uses and increments the primary message counter for the last 6 bytes.
// Returns true on success, false on failure eg due to message counter generation failure.
bool SimpleSecureFrame32or0BodyV0p2::compute12ByteIDAndCounterIVForTX(uint8_t *const ivBuf)
    {
    if(NULL == ivBuf) { return(false); }
    // Fill in first 6 bytes of this node's ID.
    eeprom_read_block(ivBuf, (uint8_t *)V0P2BASE_EE_START_ID, 6);
    // Generate and fill in new message count at end of IV.
    return(getPrimarySecure6BytePersistentTXMessageCounter(ivBuf + 6));
    }


    }
