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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2015
*/

/*
 Real-time clock support.
 */


#include <util/atomic.h>

#include <Arduino.h>

#include "OTV0P2BASE_Security.h"

#include "OTV0P2BASE_EEPROM.h"


namespace OTV0P2BASE
{


// Get the current basic ßstats transmission level (for data outbound from this node).
// May not exactly match enumerated levels; use inequalities.
// Not thread-/ISR- safe.
stats_TX_level getStatsTXLevel() { return((stats_TX_level)eeprom_read_byte((uint8_t *)V0P2BASE_EE_START_STATS_TX_ENABLE)); }



}
