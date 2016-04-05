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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2016
*/

/*
 Basic security support.
 */


#include <util/atomic.h>

#include <Arduino.h>

#include "OTV0P2BASE_Security.h"

#include "OTV0P2BASE_ADC.h"
#include "OTV0P2BASE_EEPROM.h"
#include "OTV0P2BASE_Serial_IO.h"


namespace OTV0P2BASE
{


// Get the current basic stats transmission level (for data outbound from this node).
// May not exactly match enumerated levels; use inequalities.
// Not thread-/ISR- safe.
stats_TX_level getStatsTXLevel() { return((stats_TX_level)eeprom_read_byte((uint8_t *)V0P2BASE_EE_START_STATS_TX_ENABLE)); }


// Coerce any EEPROM-based node OpenTRV ID bytes to valid values if unset (0xff) or if forced,
// by filling with valid values (0x80--0xfe) from decent entropy gathered on the fly.
// Will moan about invalid values and return false but not attempt to reset,
// eg in case underlying EEPROM cell is worn/failing.
// Returns true iff all values good.
bool ensureIDCreated(const bool force)
  {
  bool allGood = true;
  for(uint8_t i = 0; i < V0P2BASE_EE_LEN_ID; ++i)
    {
    uint8_t * const loc = i + (uint8_t *)V0P2BASE_EE_START_ID;
    if(force || (0xff == eeprom_read_byte(loc))) // Byte is unset or change is being forced.
        {
//        OTV0P2BASE::serialPrintAndFlush(F("Setting ID byte "));
//        OTV0P2BASE::serialPrintAndFlush(i);
//        OTV0P2BASE::serialPrintAndFlush(' ');
        for( ; ; )
          {
          // Try to make decently-randomised 'unique-ish' ID.
          // The ID is not confidential, and will be transmitted in the clear.
          // The system will typically not have been running long when this is invoked.
          const uint8_t newValue = 0x80 | OTV0P2BASE::getSecureRandomByte();
          if(0xff == newValue) { continue; } // Reject unusable value.
          OTV0P2BASE::eeprom_smart_update_byte(loc, newValue);
//          OTV0P2BASE::serialPrintAndFlush(newValue, HEX);
          break;
          }
//        OTV0P2BASE::serialPrintlnAndFlush();
        }
    // Validate.
    const uint8_t v2 = eeprom_read_byte(loc);
    if(!validIDByte(v2))
        {
        allGood = false;
//        OTV0P2BASE::serialPrintAndFlush(F("Invalid ID byte "));
//        OTV0P2BASE::serialPrintAndFlush(i);
//        OTV0P2BASE::serialPrintAndFlush(F(" ... "));
//        OTV0P2BASE::serialPrintAndFlush(v2, HEX);
//        OTV0P2BASE::serialPrintlnAndFlush();
        break;
        }
     }
//  if(!allGood) { OTV0P2BASE::serialPrintlnAndFlush(F("Invalid ID")); }
  return(allGood);
  }



/**
 * @brief   Sets the primary building 16 byte secret key in EEPROM.
 * @param   newKey    A pointer to the first byte of a 16 byte array containing the new key.
 *                    On passing a NULL pointer, the stored key will be cleared.
 *                    NOTE: The key pointed to by newKey must be stored as binary, NOT as text.
 * @retval  true if key is cleared successfully or new key is set, else false.
 */
// Functions for setting a 16 byte primary building secret key, which must not be all-1s.
bool setPrimaryBuilding16ByteSecretKey(const uint8_t *const newKey) // <-- this should be 16-byte binary, NOT text!
{
    // If newKey is a null pointer then clear existing key.
    if(newKey == NULL) {
        // Clear key.
        for(uint8_t i = 0; i < VOP2BASE_EE_LEN_16BYTE_PRIMARY_BUILDING_KEY; i++) {
            eeprom_smart_erase_byte(((uint8_t *)VOP2BASE_EE_START_16BYTE_PRIMARY_BUILDING_KEY)+i);
        }
        return(true);
    } else {
        // Set new key.
        const uint8_t *key = newKey;
        for(uint8_t i = 0; i < VOP2BASE_EE_LEN_16BYTE_PRIMARY_BUILDING_KEY; i++) {
            eeprom_smart_update_byte(((uint8_t *)VOP2BASE_EE_START_16BYTE_PRIMARY_BUILDING_KEY)+i, *key++);
        }
        // Verify the key just written.
        // Historically, esp on a big batch of REV7s, keys did not 'stick' the first time.  (TODO-863)
        return(checkPrimaryBuilding16ByteSecretKey(newKey));
    }
}

// Verify that the stored key is that passed in.
// Avoids leaking information about the key,
// eg by printing any of it, or terminating early on mismatch.
bool checkPrimaryBuilding16ByteSecretKey(const uint8_t *key)
    {
    bool isSame = true;
    for(uint8_t i = 0; i < VOP2BASE_EE_LEN_16BYTE_PRIMARY_BUILDING_KEY; i++)
        { isSame &= (*key++ == eeprom_read_byte(((uint8_t *)VOP2BASE_EE_START_16BYTE_PRIMARY_BUILDING_KEY)+i)); }
    return(isSame);
    }

/**
 * @brief   Fills an array with the 16 byte primary building key.
 * @param   key  pointer to a 16 byte buffer to write the key too.
 * @retval  true if key appears to be set and is retrieved
 */
bool getPrimaryBuilding16ByteSecretKey(uint8_t *key)
  {
  if(key == NULL) { return(false); }
  eeprom_read_block(key,
                    (uint8_t *)OTV0P2BASE::VOP2BASE_EE_START_16BYTE_PRIMARY_BUILDING_KEY,
                    OTV0P2BASE::VOP2BASE_EE_LEN_16BYTE_PRIMARY_BUILDING_KEY);
  bool isOK = false;
  for(uint8_t i = 0; i < OTV0P2BASE::VOP2BASE_EE_LEN_16BYTE_PRIMARY_BUILDING_KEY; ++i)
    { if(0xff != key[i]) { isOK = true; } } // Keep execution time relatively constant; no 'break'.
  return(isOK);
  }

/**
 * @brief Clears all existing node IDs.
 */
void clearAllNodeAssociations()
{
    uint8_t *nodeIDPtr = (uint8_t *)V0P2BASE_EE_START_NODE_ASSOCIATIONS;
////     It would be sufficient to ensure that the first byte of the first entry is erased (0xff)
////     IF we erase the first byte of the following entry each time we add any except the last.
//    eeprom_smart_erase_byte(nodeIDPtr);
    // Loop through EEPROM node ID locations, erasing the first byte of each.
    // (0xff is never a valid OpenTRV ID byte.)
    for(uint8_t i = 0; i < V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS; ++i) {
        eeprom_smart_erase_byte(nodeIDPtr);
        nodeIDPtr += V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE; // increment ptr
    }
}

/**Return current number of node ID associations.
 * Will be zero immediately after clearAllNodeAssociations().
 */
uint8_t countNodeAssociations()
    {
    // The first node ID starting with 0xff indicates that it and subsequent entries are empty.
    uint8_t *eepromPtr = (uint8_t *)V0P2BASE_EE_START_NODE_ASSOCIATIONS;
    // Loop through node ID locations checking for invalid byte (0xff).
    for(uint8_t i = 0; i < V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS; i++, eepromPtr += V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE)
        { if(0xff == eeprom_read_byte(eepromPtr)) { return(i); } }
    return(V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS); // All full!
    }

/**Get node ID of association at specified index.
 * Returns true if successful.
 *   * index  association index of required node ID
 *   * nodeID  8-byte buffer to receive ID; never NULL
 */
bool getNodeAssociation(const uint8_t index, uint8_t *const nodeID)
  {
  if((NULL == nodeID) || (index >= countNodeAssociations())) { return(false); } // FAIL: bad args.
  eeprom_read_block(nodeID,
                    (uint8_t *)(V0P2BASE_EE_START_NODE_ASSOCIATIONS + index*(uint16_t)V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE),
                    OpenTRV_Node_ID_Bytes);
  return(true);
  }

/**
 * @brief   Checks through stored node IDs and adds a new one if there is space.
 * @param   pointer to new 8 byte node ID
 * @retval  index of this new association, or -1 if no space
 */
// TODO: optionally allow setting (persistent) MSBs of counter to current+1 and force counterparty restart to eliminate replay attack.
int8_t addNodeAssociation(const uint8_t *nodeID)
{
    uint8_t *eepromPtr = (uint8_t *)V0P2BASE_EE_START_NODE_ASSOCIATIONS;
    // Loop through node ID locations checking for empty slot marked by invalid byte (0xff).
    for(uint8_t i = 0; i < V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS; i ++) {
        if(eeprom_read_byte(eepromPtr) == 0xff) {
            for(uint8_t j = 0; j < V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE; j++) {
                if(j < V0P2BASE_EE_NODE_ASSOCIATIONS_8B_ID_LENGTH) {
                    eeprom_smart_update_byte(eepromPtr++, *nodeID++);
                } else {
                    // On writing a new association/entry all bytes after the ID must be erased to 0xff.
                    eeprom_smart_erase_byte(eepromPtr++);
                }
            }
            return (i);
        }
        eepromPtr += V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE; // increment ptr
    }
    return(-1); // No space.
}

/**
 * @brief   Returns first matching node ID after the index provided. If no
 *          matching ID found, it will return -1.
 * @param   index   Index to start searching from.
 *          prefix  Prefix to match; can be NULL iff prefixLen == 0.
 *          prefixLen  Length of prefix, [0,8] bytes.
 *          nodeID  Buffer to write nodeID to; can be NULL if only the index return value is required. THIS IS NOT PRESERVED WHEN FUNCTION RETURNS -1!
 * @retval  returns index or -1 if no matching node ID found
 */
int8_t getNextMatchingNodeID(const uint8_t _index, const uint8_t *prefix, const uint8_t prefixLen, uint8_t *nodeID)
{
    // Validate inputs.
    if(_index >= V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS) { return(-1); }
    if(prefixLen > V0P2BASE_EE_NODE_ASSOCIATIONS_8B_ID_LENGTH) { return(-1); }
    if((NULL == prefix) && (0 != prefixLen)) { return(-1); }

    // Loop through node IDs until match or last entry tested.
    //   - if a match is found, return index and fill nodeID
    //   - if no match, exit loop.
    uint8_t *eepromPtr = (uint8_t *)V0P2BASE_EE_START_NODE_ASSOCIATIONS + (_index *  (int)V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE);
    for(uint8_t index = _index; index < V0P2BASE_EE_NODE_ASSOCIATIONS_MAX_SETS; index++) {
        uint8_t temp = eeprom_read_byte(eepromPtr); // temp variable for byte read
        if(temp == 0xff) { return(-1); } // last entry reached. exit w/ error.
        else if((0 == prefixLen) || (temp == *prefix)) { // this is the case where it matches
            // loop through first prefixLen bytes of nodeID, comparing output
            uint8_t i; // persistent loop counter
            uint8_t *tempPtr = eepromPtr;    // temp pointer so that eepromPtr is preserved if not a match
            if(NULL != nodeID) { nodeID[0] = temp; }
            for(i = 1; i < prefixLen; i++) {
                // if bytes match, copy and check next byte?
                temp = eeprom_read_byte(tempPtr++);
                if(prefix[i] == temp) {
                    if(NULL != nodeID) { nodeID[i] = temp; }
                } else break; // exit inner loop.
            }
            if(NULL != nodeID) {
                // Since prefix matches, copy rest of node ID.
                for (; i < (V0P2BASE_EE_NODE_ASSOCIATIONS_8B_ID_LENGTH); i++) {
                    nodeID[i] = eeprom_read_byte(tempPtr++);
                }
            }
            return index;
        }
        eepromPtr += V0P2BASE_EE_NODE_ASSOCIATIONS_SET_SIZE; // Increment ptr to next node ID field.
    }

    // No match has been found.
    return(-1);
}


}
