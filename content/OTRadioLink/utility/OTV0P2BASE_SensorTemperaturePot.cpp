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
 Temperature potentiometer (pot) dial sensor with UI / occupancy outputs.

 V0p2/AVR specific for now.
 */



#include "OTV0P2BASE_SensorTemperaturePot.h"

#include <stddef.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "OTV0P2BASE_ADC.h"
#include "OTV0P2BASE_BasicPinAssignments.h"
#include "OTV0P2BASE_Entropy.h"
#include "OTV0P2BASE_PowerManagement.h"


namespace OTV0P2BASE
{


#ifdef SensorTemperaturePot_DEFINED

// Force a read/poll of the temperature pot and return the value sensed [0,255] (cold to hot).
// Potentially expensive/slow.
// This value has some hysteresis applied to reduce noise.
// Not thread-safe nor usable within ISRs (Interrupt Service Routines).
uint8_t SensorTemperaturePot::read()
  {
  // Capture the old raw value early.
  const uint16_t oldRaw = raw;

  // No need to wait for voltage to stabilise as pot top end directly driven by IO_POWER_UP.
  OTV0P2BASE::power_intermittent_peripherals_enable(false);
  const uint16_t tpRaw = OTV0P2BASE::analogueNoiseReducedRead(V0p2_PIN_TEMP_POT_AIN, DEFAULT); // Vcc reference.
  OTV0P2BASE::power_intermittent_peripherals_disable();

  const bool reverse = isReversed();
  const uint16_t newRaw = reverse ? (TEMP_POT_RAW_MAX - tpRaw) : tpRaw;

  // Capture entropy from changed LS bits.
  if((uint8_t)newRaw != (uint8_t)oldRaw) { ::OTV0P2BASE::addEntropyToPool((uint8_t)newRaw, 0); } // Claim zero entropy as may be forced by Eve.

  // Capture reduced-noise value with a little hysteresis.
  // Only update the value if changed significantly so as to reduce noise.
  // Too much hysteresis may make the dial difficult to use,
  // especially if the rotation is physically constrained.
  const uint8_t oldValue = value;
  const uint8_t potentialNewValue = newRaw >> 2;
  if(((potentialNewValue > oldValue) && (newRaw - oldRaw >= (RN_HYST<<2))) ||
     ((potentialNewValue < oldValue) && (oldRaw - newRaw >= (RN_HYST<<2))))
    {
    // Use this potential new value as a reduced-noise new value.
    const uint8_t rn = (uint8_t) potentialNewValue;
    // Atomically store the reduced-noise normalised value.
    value = rn;

    // Smart responses to adjustment/movement of temperature pot.
    // Possible to get reasonable functionality without using MODE button.
    //
    // Ignore first reading which might otherwise cause spurious mode change, etc.
    if((uint16_t)~0U != (uint16_t)raw) // Ignore if raw not yet set for the first time.
      {
      // Force FROST mode when dial turned right down to bottom.
      if(rn < loEndStop) { if(NULL != warmModeCallback) { warmModeCallback(false); } }
      // Start BAKE mode when dial turned right up to top.
      else if(rn > hiEndStop) { if(NULL != bakeStartCallback) { bakeStartCallback(true); } }
      // Cancel BAKE mode when dial/temperature turned down.
      else if(rn < oldValue) { if(NULL != bakeStartCallback) { bakeStartCallback(false); } }
      // Force WARM mode when dial/temperature turned up.
      else if(rn > oldValue) { if(NULL != warmModeCallback) { warmModeCallback(true); } }

      // Report that the user operated the pot, ie part of the manual UI.
      // Do this regardless of whether a specific mode change was invoked.
      if(NULL != occCallback) { occCallback(); }
      }
    }

#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("Temp pot: ");
  DEBUG_SERIAL_PRINT(tp);
  DEBUG_SERIAL_PRINT_FLASHSTRING(", rn: ");
  DEBUG_SERIAL_PRINT(tempPotReducedNoise);
  DEBUG_SERIAL_PRINTLN();
#endif

  // Store new raw value last.
  raw = newRaw;
  // Return noise-reduced value.
  return(value);
  }

#endif // SensorTemperaturePot_DEFINED


}
