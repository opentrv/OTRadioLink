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
*/

/*
 * OpenTRV model and smart control of (thermostatic) radiator valve.
 *
 * Also includes some common supporting base/interface classes.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_MODELLEDRADVALVE_H
#define ARDUINO_LIB_OTRADVALVE_MODELLEDRADVALVE_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


// Simple mean filter.
// Find mean of group of ints where sum can be computed in an int without loss.
// TODO: needs a unit test or three.
template<size_t N> int smallIntMean(const int data[N])
  {
  // Extract mean.
  // Assume values and sum will be nowhere near the limits.
  int sum = 0;
  for(int8_t i = N; --i >= 0; ) { sum += data[i]; }
  // Compute rounded-up mean.
  return((sum + (int)(N/2)) / (int)N); // Avoid accidental computation as unsigned...
  }


// Delay in minutes after increasing flow before re-closing is allowed.
// This is to avoid excessive seeking/noise in the presence of strong draughts for example.
// Too large a value may cause significant temperature overshoots and possible energy wastage.
static const uint8_t ANTISEEK_VALVE_RECLOSE_DELAY_M = 5;
// Delay in minutes after restricting flow before re-opening is allowed.
// This is to avoid excessive seeking/noise in the presence of strong draughts for example.
// Too large a value may cause significant temperature undershoots and discomfort/annoyance.
static const uint8_t ANTISEEK_VALVE_REOPEN_DELAY_M = (ANTISEEK_VALVE_RECLOSE_DELAY_M*2);

// Typical heat turn-down response time; in minutes, strictly positive.
static const uint8_t TURN_DOWN_RESPONSE_TIME_M = (ANTISEEK_VALVE_RECLOSE_DELAY_M + 3);

// Assumed daily budget in cumulative (%) valve movement for battery-powered devices.
static const uint16_t DEFAULT_MAX_CUMULATIVE_PC_DAILY_VALVE_MOVEMENT = 400;


// All input state for computing valve movement.
// Exposed to allow easier unit testing.
// FIXME: add flag to indicate manual operation/override, so speedy response required (TODO-593).
struct ModelledRadValveInputState
  {
  // All initial values set by the constructor are sane, but should not be relied on.
  ModelledRadValveInputState(const int realTempC16);

  // Calculate reference temperature from real temperature.
  void setReferenceTemperatures(const int currentTempC16);

  // Current target room temperature in C in range [MIN_TARGET_C,MAX_TARGET_C].
  uint8_t targetTempC;
  // Min % valve at which is considered to be actually open (allow the room to heat) [1,100].
  uint8_t minPCOpen;
  // Max % valve is allowed to be open [1,100].
  uint8_t maxPCOpen;

  // If true then allow a wider deadband (more temperature drift) to save energy and valve noise.
  // This is a strong hint that the system can work less strenuously to hit, and stay on, target.
  bool widenDeadband;
  // True if in glacial mode.
  bool glacial;
  // True if an eco bias is to be applied.
  bool hasEcoBias;
  // True if in BAKE mode.
  bool inBakeMode;

  // Reference (room) temperature in C/16; must be set before each valve position recalc.
  // Proportional control is in the region where (refTempC16>>4) == targetTempC.
  int refTempC16;
  };


    }

#endif
