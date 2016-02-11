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
                           Deniz Erbilgin   2016
*/

/*
 Basic security support.
 */

#ifndef OTV0P2BASE_SECURITY_H
#define OTV0P2BASE_SECURITY_H


namespace OTV0P2BASE
{


// Leaf node privacy level: how much to transmit about stats such as temperature and occupancy.
// The greater numerically the value, the less data is sent, especially over an insecure channel.
// Excess unencrypted stats may, for example, allow a clever burglar to work out when no one is home.
// Note that even in the 'always' setting,
// some TXes may be selectively skipped or censored for energy saving and security reasons
// eg an additional 'never transmit occupancy' flag may be set locally.
// The values correspond to levels and intermediate values not explicitly enumerated are allowed.
// Lower values mean less that security is required.
enum stats_TX_level
  {
  stTXalwaysAll = 0,    // Always be prepared to transmit all stats (zero privacy).
  stTXmostUnsec = 0x80, // Allow TX of all but most security-sensitive stats in plaintext, eg occupancy status.
  stTXsecOnly   = 0xfe, // Only transmit if the stats TX can be kept secure/encrypted.
  stTXnever     = 0xff, // DEFAULT: never transmit status info beyond the minimum necessary.
  };

// Get the current basic stats transmission level (for data outbound from this node).
// May not exactly match enumerated levels; use inequalities.
// Not thread-/ISR- safe.
stats_TX_level getStatsTXLevel();


// Returns true iff valid OpenTRV node ID byte: must have the top bit set and not be 0xff.
inline bool validIDByte(const uint8_t v) { return((0 != (0x80 & v)) && (0xff != v)); }

// Coerce any EEPROM-based node OpenTRV ID bytes to valid values if unset (0xff) or if forced,
// by filling with valid values (0x80--0xfe) from decent entropy.
// Will moan about invalid values and return false but not attempt to reset,
// eg in case underlying EEPROM cell is worn/failing.
// Returns true if all values good.
bool ensureIDCreated(const bool force = false);

// Functions for setting a 16 byte primary building secret key
bool setPrimaryBuilding16ByteSecretKey(const uint8_t *key);
bool getPrimaryBuilding16ByteSecretKey(uint8_t *key);

/**
 * @brief   Clears all existing node IDs by writing 0xff to first byte.
 * @todo    Should this return something useful, such as the number of bytes cleared?
 */
void clearAllNodeIDs();

/**
 * @brief   Checks through stored node IDs and adds a new one if there is space.
 * @param   pointer to new 8 byte node ID
 * @retval  Number of stored node IDs, or 0xFF if storage full
 */
uint8_t addNodeID(const uint8_t *nodeID);

//#if 0 // Pairing API outline.
//struct pairInfo { bool successfullyPaired; };
//bool startPairing(bool primary, &pairInfo);
//bool continuePairing(bool primary, &pairInfo); // Incremental work.
//void clearPairing(bool primary, &pairInfo);
//#endif


}
#endif
