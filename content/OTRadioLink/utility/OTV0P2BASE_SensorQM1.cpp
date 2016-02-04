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

Author(s) / Copyright (s): Damon Hart-Davis 2015
                           Deniz Erbilgin   2016
*/


#include <stdint.h>
#include <util/atomic.h>

#include <Arduino.h>

#include "OTV0P2BASE_SensorQM1.h"


namespace OTV0P2BASE
{
// If count meets or exceeds this threshold in one poll period then
// the room is deemed to be occupied.
// Strictly positive.
// DHD20151119: even now it seems a threshold of >= 2 is needed to avoid false positives.
// DE20160101:  Lowered detection threshold as new boards have lower sensitivity
static const uint8_t voiceDetectionThreshold = 4;

// Force a read/poll of the voice level and return the value sensed.
// Thread-safe and ISR-safe.
uint8_t VoiceDetectionQM1::read()
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    isDetected = ((value = count) >= voiceDetectionThreshold);
    // clear count and detection flag
    // isTriggered = false;
    count = 0;
  }
  return(value);
}

// Handle simple interrupt.
// Fast and ISR (Interrupt Service Routines) safe.
// Returns true if interrupt was successfully handled and cleared
// else another interrupt handler in the chain may be called
// to attempt to clear the interrupt.
bool VoiceDetectionQM1::handleInterruptSimple()
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    // Count of voice activations since last poll, avoiding overflow.
    if ((count < 255) && (++count >= voiceDetectionThreshold))
    {
      // Act as soon as voice is detected.
      isDetected = true;
      // Don't regard this as a very strong indication,
      // as it could be a TV or radio on in the room.
//      Occupancy.markAsPossiblyOccupied();
    }
  }

  //    // Flag that interrupt has occurred
  //    endOfLocking = OTV0P2BASE::getMinutesSinceMidnightLT() + lockingPeriod;
  //    isTriggered = true;
  // No further work to be done to 'clear' interrupt.
  return (true);
}

}
