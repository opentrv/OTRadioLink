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


// Dummy implementations of setPrimaryBuilding16ByteSecretKey & getPrimaryBuilding16ByteSecretKey
// Notes copied from TODO-788:
// Clearing a key should 'smart' erase the appropriate EEPROM bytes back to their original 0xff values.
// bool getPrimaryBuilding16ByteSecretKey(uint8_t *key) fills in the 16-byte key buffer passed to it and returns true,
// or on failure (eg because the key is unset and all 0xffs) returns false.
bool setPrimaryBuilding16ByteSecretKey(const uint8_t *newKey)
  {
  if(newKey == NULL) return false;
  // convert hex string to binary array
  //   - if any bytes are invalid, return false (this may also function as a free length check)
  OTV0P2BASE::serialPrintAndFlush((const char *)newKey);
  OTV0P2BASE::serialPrintlnAndFlush(); // echo
  return(false); // FIXME
  }
bool getPrimaryBuilding16ByteSecretKey(const uint8_t *key)
  {
  return(false); // FIXME
  }



}
