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
 Occupancy pseudo-sensor that combines inputs from other sensors.
 */

#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#endif

#include "OTV0P2BASE_SensorOccupancy.h"


namespace OTV0P2BASE
{


//#if (OCCUPATION_TIMEOUT_M < 25) || (OCCUPATION_TIMEOUT_M > 100)
//#error needs support for different occupancy timeout
//#elif OCCUPATION_TIMEOUT_M <= 25
//#define OCCCP_SHIFT 2
//#elif OCCUPATION_TIMEOUT_M <= 50
//#define OCCCP_SHIFT 1
//#elif OCCUPATION_TIMEOUT_M <= 100
//#define OCCCP_SHIFT 0
//#endif

#ifdef ARDUINO

// Shift from minutes remaining to confidence.
// Will not work correctly with timeout > 100.
static const uint8_t OCCCP_SHIFT =
  ((PseudoSensorOccupancyTracker::OCCUPATION_TIMEOUT_M <= 3) ? 5 :
  ((PseudoSensorOccupancyTracker::OCCUPATION_TIMEOUT_M <= 6) ? 4 :
  ((PseudoSensorOccupancyTracker::OCCUPATION_TIMEOUT_M <= 12) ? 3 :
  ((PseudoSensorOccupancyTracker::OCCUPATION_TIMEOUT_M <= 25) ? 2 :
  ((PseudoSensorOccupancyTracker::OCCUPATION_TIMEOUT_M <= 50) ? 1 :
  ((PseudoSensorOccupancyTracker::OCCUPATION_TIMEOUT_M <= 100) ? 0 :
  ((PseudoSensorOccupancyTracker::OCCUPATION_TIMEOUT_M <= 200) ? -1 : -2)))))));

// Update notion of occupancy confidence.
uint8_t PseudoSensorOccupancyTracker::read()
  {
  ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
    {
    // Compute as percentage.
    const uint8_t newValue = (0 == occupationCountdownM) ? 0 :
        OTV0P2BASE::fnmin((uint8_t)((uint8_t)100 - (uint8_t)((((uint8_t)OCCUPATION_TIMEOUT_M) - occupationCountdownM) << OCCCP_SHIFT)), (uint8_t)100);
    value = newValue;
    // Update the various metrics in a thread-/ISR- safe way (needs lock, since read-modify-write).
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
      {
      // Run down occupation timer (or run up vacancy time) if need be.
      if(occupationCountdownM > 0) { --occupationCountdownM; vacancyM = 0; vacancyH = 0; }
      else if(vacancyH < 0xffU) { if(++vacancyM >= 60) { vacancyM = 0; ++vacancyH; } }
      // Run down 'recent activity' timer.
      if(activityCountdownM > 0) { --activityCountdownM; }
      }
    return(newValue);
    }
  }
#endif

#ifdef ARDUINO
// Call when decent but not very strong evidence of active room occupation, such as a light being turned on, or voice heard.
// Do not call based on internal/synthetic events.
// Doesn't force the room to appear recently occupied.
// If the hardware allows this may immediately turn on the main GUI LED until normal GUI reverts it,
// at least periodically.
// Preferably do not call for manual control operation to avoid interfering with UI operation.
// Thread-safe.
void PseudoSensorOccupancyTracker::markAsPossiblyOccupied()
  {
  // TODO: set a user-clearable 'activity' flag, probably at first sign only of occupancy.
//  if(0 == occupationCountdownM) // Flash at first sign only of occupancy after vacancy to limit annoyance.
//    { LED_HEATCALL_ON_ISR_SAFE(); }
  // Update primary occupation metric in thread-safe way (needs lock, since read-modify-write).
  ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
    {
    occupationCountdownM = OTV0P2BASE::fnmax((uint8_t)occupationCountdownM, (uint8_t)(OCCUPATION_TIMEOUT_LIKELY_M));
    activityCountdownM = 2; // Probably thread-/ISR- safe anyway, as atomic byte write.
    }
  }
#endif

#ifdef ARDUINO
// Call when weak evidence of active room occupation, such rising RH% or CO2 or mobile phone RF levels while not dark.
// Do not call based on internal/synthetic events.
// Doesn't force the room to appear recently occupied.
// If the hardware allows this may immediately turn on the main GUI LED until normal GUI reverts it,
// at least periodically.
// Preferably do not call for manual control operation to avoid interfering with UI operation.
// Thread-safe.
void PseudoSensorOccupancyTracker::markAsJustPossiblyOccupied()
  {
  // TODO: set a user-clearable 'activity' flag, probably at first sign only of occupancy.
//  if(0 == occupationCountdownM) // Flash at first sign only of occupancy after vacancy to limit annoyance.
//    { LED_HEATCALL_ON_ISR_SAFE(); }
  // Update primary occupation metric in thread-safe way (needs lock, since read-modify-write).
  ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
    {
    occupationCountdownM = OTV0P2BASE::fnmax((uint8_t)occupationCountdownM, (uint8_t)(OCCUPATION_TIMEOUT_MAYBE_M));
    activityCountdownM = 2; // Probably thread-/ISR- safe anyway, as atomic byte write.
    }
  }
#endif


}
