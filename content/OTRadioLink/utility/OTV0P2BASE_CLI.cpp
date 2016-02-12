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

Author(s) / Copyright (s): Deniz Erbilgin 2016
                           Damon Hart-Davis 2016
*/

#include "OTV0P2BASE_CLI.h"

#include "OTV0P2BASE_EEPROM.h"
#include "OTV0P2BASE_Security.h"
#include "OTV0P2BASE_Sleep.h"
#include "OTV0P2BASE_Util.h"


namespace OTV0P2BASE {
namespace CLI {

// Show / reset node ID ('I').
bool NodeID::doCommand(char *const buf, const uint8_t buflen)
    {
    if((3 == buflen) && ('*' == buf[2]))
      { OTV0P2BASE::ensureIDCreated(true); } // Force ID change.
    Serial.print(F("ID:"));
    for(uint8_t i = 0; i < V0P2BASE_EE_LEN_ID; ++i)
      {
      Serial.print(' ');
      Serial.print(eeprom_read_byte((uint8_t *)(V0P2BASE_EE_START_ID + i)), HEX);
      }
    Serial.println();
    return(true);
    }

// Set TX privacy level ("X NN").
// Lower means less privacy: 0 means send everything, 255 means send as little as possible.
bool SetTXPrivacy::doCommand(char *const buf, const uint8_t buflen)
    {
    char *last; // Used by strtok_r().
    char *tok1;
    // Minimum 3 character sequence makes sense and is safe to tokenise, eg "X 0".
    if((buflen >= 3) && (NULL != (tok1 = strtok_r(buf+2, " ", &last))))
      {
      const uint8_t nn = (uint8_t) atoi(tok1);
      OTV0P2BASE::eeprom_smart_update_byte((uint8_t *)V0P2BASE_EE_START_STATS_TX_ENABLE, nn);
      }
    return(true);
    }

// Zap/erase learned statistics ('Z').
// Avoid showing status afterwards as may already be rather a lot of output.
bool ZapStats::doCommand(char *const buf, const uint8_t buflen)
    {
    // Try to avoid causing an overrun if near the end of the minor cycle (even allowing for the warning message if unfinished!).
    if(OTV0P2BASE::zapStats((uint16_t) OTV0P2BASE::fnmax(1, ((int)OTV0P2BASE::msRemainingThisBasicCycle()/2) - 20)))
      { Serial.println(F("Zapped.")); }
    else
      { Serial.println(F("Not finished.")); }
    return(false); // May be slow; avoid showing stats line which will in any case be unchanged.
    }


} }
