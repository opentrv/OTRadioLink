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
// #include <iostream>

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
enum stats_TX_level : uint8_t
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
static constexpr uint8_t OpenTRV_Node_ID_Bytes = 8;

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
typedef bool (GetPrimary16ByteSecretKey_t)(uint8_t *key);
GetPrimary16ByteSecretKey_t getPrimaryBuilding16ByteSecretKey;

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
 * @brief   Base class for node association tables.
 */
class NodeAssociationTableBase {
public:
    virtual bool set(uint8_t index, const uint8_t* src) = 0;
    virtual void get(uint8_t index, uint8_t* dest) const = 0;
};

/**
 * @brief   Extends NodeAssociationTableBase for unit testing.
 */
class NodeAssociationTableMock : public NodeAssociationTableBase {
public:
    static constexpr uint8_t maxSets {V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS};
    static constexpr uint8_t idLength {V0P2BASE_EE_NODE_ASSOCIATIONS_8B_ID_LENGTH};

    NodeAssociationTableMock();

    // Set an ID
    bool set(uint8_t index, const uint8_t* src) override;
    // Get an ID
    void get(uint8_t index, uint8_t* dest) const override;
    // Exposed for unit testing. Clears all values to default.
    void _reset() { for(auto& x: buf) { x = 255; } }

private:
    // This is the size of a node association entry. Usually
    // V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE bytes in length.
    static constexpr uint8_t setSize {V0P2BASE_EE_NODE_ASSOCIATIONS_8B_ID_LENGTH};
    // This buffer pretends to be the EEPROM.
    uint8_t buf[maxSets * setSize];
};

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
 *          prefix  Prefix to match; can be NULL if prefixLen == 0.  TODO: Make this an OTBuf_t?
 *          prefixLen  Length of prefix, [0,8] bytes.
 *          nodeID  Buffer to write nodeID to; can be NULL if only the index return value is required. Not currently true >> THIS IS NOT PRESERVED WHEN FUNCTION RETURNS -1!
 * @retval  returns index or -1 if no matching node ID found
 */
template<class NodeAssocTable_T, const NodeAssocTable_T& nodes>
int8_t getNextMatchingNodeIDGeneric(uint8_t _index, const uint8_t *prefix, uint8_t prefixLen, uint8_t *nodeID)
{
    // // Validate inputs.
    if(_index >= V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS) { return(-1); }
    if(prefixLen > V0P2BASE_EE_NODE_ASSOCIATIONS_8B_ID_LENGTH) { return(-1); }
    if((NULL == prefix) && (0 != prefixLen)) { return(-1); }

    // Loop through node IDs until match or last entry tested.
    //   - if a match is found, return index and fill nodeID
    //   - if no match, exit loop.
    for (uint8_t index = _index; index != nodes.maxSets; ++index) {
        uint8_t temp[nodes.idLength] = {};
        nodes.get(index, temp);

        if (0xff == temp[0]) { return (-1); } 

        // If no prefix is passed in, we match automatically and let the caller
        // deal with scanning values.
        const bool isMatch = (prefixLen == 0) || (memcmp(temp, prefix, prefixLen) == 0);

        if (isMatch) {
            if (nullptr != nodeID) { memcpy(nodeID, temp, nodes.idLength); }

            return (index);
        }
    }

    // No match has been found.
    return(-1);
}

#ifdef ARDUINO_ARCH_AVR
#define OTV0P2BASE_NODE_ASSOCIATION_TABLE_V0P2
/**
 * @brief   Implementation of NodeAssociationTableBase for Arduino with EEPROM.
 */
class NodeAssociationTableV0p2 : public NodeAssociationTableBase {
private:
    static constexpr uint8_t setSize {V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE};
    static constexpr intptr_t startAddr {V0P2BASE_EE_START_NODE_ASSOCIATIONS};

public:
    static constexpr uint8_t maxSets {V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS};
    static constexpr uint8_t idLength {V0P2BASE_EE_NODE_ASSOCIATIONS_8B_ID_LENGTH};

    /**
     * @brief   Sets an 8-byte ID in EEPROM.
     * @param   index: Index the ID is located at. must be in range [0, maxSets[
     * @param   src: Pointer to a buffer to copy the ID from.
     */
    bool set(uint8_t index, const uint8_t* src) override;
    /**
     * @brief   Gets an 8-byte ID from EEPROM.
     * @param   index: Index the ID is located at. must be in range [0, maxSets[
     * @param   dest: Pointer to a buffer to copy the ID to.
     */
    void get(uint8_t index, uint8_t* dest) const override;
};

// Static instance of V0p2_Nodes for backwards compatibility.
static constexpr NodeAssociationTableV0p2 V0p2_Nodes;

int8_t getNextMatchingNodeID(
    uint8_t _index, 
    const uint8_t *prefix, 
    uint8_t prefixLen, 
    uint8_t *nodeID)
{
    // return (getNextMatchingNodeIDGeneric<decltype(V0p2_Nodes), V0p2_Nodes>(index, prefix, prefixLen, nodeID));
        // // Validate inputs.
    if(_index >= V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS) { return(-1); }
    if(prefixLen > V0P2BASE_EE_NODE_ASSOCIATIONS_8B_ID_LENGTH) { return(-1); }
    if((NULL == prefix) && (0 != prefixLen)) { return(-1); }

    // Loop through node IDs until match or last entry tested.
    //   - if a match is found, return index and fill nodeID
    //   - if no match, exit loop.
    for (uint8_t index = _index; index != V0p2_Nodes.maxSets; ++index) {
        uint8_t temp[V0p2_Nodes.idLength] = {};
        V0p2_Nodes.get(index, temp);

        if (0xff == temp[0]) { return (-1); } 

        // If no prefix is passed in, we match automatically and let the caller
        // deal with scanning values.
        const bool isMatch = (prefixLen == 0) || (memcmp(temp, prefix, prefixLen) == 0);

        if (isMatch) {
            if (nullptr != nodeID) { memcpy(nodeID, temp, V0p2_Nodes.idLength); }

            return (index);
        }
    }

    // No match has been found.
    return(-1);
}
#endif // ARDUINO_ARCH_AVR

//#if 0 // Pairing API outline.
//struct pairInfo { bool successfullyPaired; };
//bool startPairing(bool primary, &pairInfo);
//bool continuePairing(bool primary, &pairInfo); // Incremental work.
//void clearPairing(bool primary, &pairInfo);
//#endif


}
#endif
