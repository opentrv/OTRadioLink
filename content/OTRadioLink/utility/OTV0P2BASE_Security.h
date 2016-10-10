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

#include <stdint.h>

#include "OTV0P2BASE_EEPROM.h"


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
  stTXmostUnsec = 0x80, // Allow TX of all but most security-sensitive stats in plaintext, eg. occupancy status.
  stTXsecOnly   = 0xfe, // Only transmit if the stats TX can be kept secure/encrypted.
  stTXnever     = 0xff, // DEFAULT: never transmit status info beyond the minimum necessary.
  };

// Get the current basic stats transmission level (for data outbound from this node).
// May not exactly match enumerated levels; use inequalities.
// Not thread-/ISR- safe.
stats_TX_level getStatsTXLevel();


// Size of OpenTRV node ID in bytes.
// Note that 0xff is never a valid OpenTRV node ID byte.
// Note that most OpenTRV node ID bytes should have the top bit (0x80) set.
static const uint8_t OpenTRV_Node_ID_Bytes = 8;

// Returns true if definitely valid OpenTRV node ID byte: must have the top bit set and not be 0xff.
inline bool validIDByte(const uint8_t v) { return((0 != (0x80 & v)) && (0xff != v)); }

// Coerce any EEPROM-based node OpenTRV ID bytes to valid values if unset (0xff) or if forced,
// by filling with valid values (0x80--0xfe) from decent entropy.
// Will moan about invalid values and return false but not attempt to reset,
// eg in case underlying EEPROM cell is worn/failing.
// Returns true if all values good.
bool ensureIDCreated(const bool force = false);

// Functions for setting a 16 byte primary building secret key which must not be all-1s.
/**
 * @brief   Sets the primary building 16 byte secret key in EEPROM.
 * @param   newKey    A pointer to the first byte of a 16 byte array containing the new key.
 *                    On passing a NULL pointer, the stored key will be cleared.
 *                    NOTE: The key pointed to by newKey must be stored as binary, NOT as text.
 * @retval  true if key is cleared successfully or new key is set, else false.
 */
bool setPrimaryBuilding16ByteSecretKey(const uint8_t *key);

/**
 * @brief   Fills an array with the 16 byte primary building key.
 * @param   key  pointer to a 16 byte buffer to write the key too.
 * @retval  true if key appears to be set and is retrieved
 */
bool getPrimaryBuilding16ByteSecretKey(uint8_t *key);

// Verify that the stored key is that passed in.
// Avoids leaking information about the key,
// eg by printing any of it, or terminating early on mismatch.
bool checkPrimaryBuilding16ByteSecretKey(const uint8_t *key);

// Maximum number of node associations that can be maintained for secure traffic.
// This puts an upper bound on the number of nodes which a hub can listen to securely.
#ifdef V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS
static const uint8_t MAX_NODE_ASSOCIATIONS = V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS;
#endif

/**
 * @brief   Clears all existing node ID associations.
 */
void clearAllNodeAssociations();

/**Return current number of node ID associations.
 * Will be zero immediately after clearAllNodeAssociations().
 */
uint8_t countNodeAssociations();

/**Get node ID of association at specified index.
 * Returns true if successful.
 *   * index  association index of required node ID
 *   * nodeID  8-byte buffer to receive ID; never NULL
 */
bool getNodeAssociation(uint8_t index, uint8_t *nodeID);

/**
 * @brief   Checks through stored node IDs and adds a new one if there is space.
 * @param   pointer to new 8 byte node ID
 * @retval  index of this new association, or -1 if no space
 */
int8_t addNodeAssociation(const uint8_t *nodeID);

/**
 * @brief   Returns first matching node ID after the index provided. If no
 *          matching ID found, it will return -1.
 * @param   index   Index to start searching from.
 *          prefix  Prefix to match; can be NULL iff prefixLen == 0.
 *          prefixLen  Length of prefix, [0,8] bytes.
 *          nodeID  Buffer to write nodeID to; can be NULL if only the index return value is required. THIS IS NOT PRESERVED WHEN FUNCTION RETURNS -1!
 * @retval  returns index or -1 if no matching node ID found
 */
int8_t getNextMatchingNodeID(uint8_t _index, const uint8_t *prefix, uint8_t prefixLen, uint8_t *nodeID);

//#if 0 // Pairing API outline.
//struct pairInfo { bool successfullyPaired; };
//bool startPairing(bool primary, &pairInfo);
//bool continuePairing(bool primary, &pairInfo); // Incremental work.
//void clearPairing(bool primary, &pairInfo);
//#endif


}
#endif
