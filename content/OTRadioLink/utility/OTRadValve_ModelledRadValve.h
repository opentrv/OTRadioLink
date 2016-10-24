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
#include "OTV0P2BASE_Util.h"
#include "OTRadValve_Parameters.h"


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


// All input state for computing valve movement.
// Exposed to allow easier unit testing.
//
// This uses int_fast16_t for C16 temperatures (ie Celsius * 16)
// to be able to efficiently process signed values with sufficient range for room temperatures.
struct ModelledRadValveInputState
  {
  // Offset from raw temperature to get reference temperature in C/16.
  static const int_fast8_t refTempOffsetC16 = 8;

  // All initial values set by the constructor are sane, but should not be relied on.
  ModelledRadValveInputState(const int_fast16_t realTempC16);

  // Calculate and store reference temperature(s) from real temperature supplied.
  // Proportional temperature regulation is in a 1C band.
  // By default, for a given target XC the rad is off at (X+1)C so temperature oscillates around that point.
  // This routine shifts the reference point at which the rad is off to (X+0.5C)
  // ie to the middle of the specified degree, which is more intuitive,
  // and which may save a little energy if users target the specified temperatures.
  // Suggestion c/o GG ~2014/10 code, and generally less misleading anyway!
  void setReferenceTemperatures(const int_fast16_t currentTempC16)
    {
    const int_fast16_t referenceTempC16 = currentTempC16 + refTempOffsetC16; // TODO-386: push targeted temperature down by 0.5C to middle of degree.
    refTempC16 = referenceTempC16;
    }

  // Current target room temperature in C in range [MIN_TARGET_C,MAX_TARGET_C].
  uint8_t targetTempC;
  // Min % valve at which is considered to be actually open (allow the room to heat) [1,100].
  uint8_t minPCOpen;
  // Max % valve is allowed to be open [1,100].
  uint8_t maxPCOpen;

  // If true then allow a wider deadband (more temperature drift) to save energy and valve noise.
  // This is a strong hint that the system can work less strenuously to reach or stay on, target,
  // and/or that the user has not manually requested an adjustment recently
  // so this need not be ultra responsive.
  bool widenDeadband;
  // True if in glacial mode.
  bool glacial;
  // True if an eco bias is to be applied.
  bool hasEcoBias;
  // True if in BAKE mode.
  bool inBakeMode;
  // User just adjusted controls or other fast response needed.
  // (Should not be true at same time as widenDeadband.)
  // Indicates manual operation/override, so speedy response required (TODO-593).
  bool fastResponseRequired;

  // Reference (room) temperature in C/16; must be set before each valve position recalc.
  // Proportional control is in the region where (refTempC16>>4) == targetTempC.
  // This is signed and at least 16 bits.
  int_fast16_t refTempC16;
  };


// All retained state for computing valve movement, eg containing time-based state.
// Exposed to allow easier unit testing.
// All initial values set by the constructor are sane.
//
// This uses int_fast16_t for C16 temperatures (ie Celsius * 16)
// to be able to efficiently process signed values with sufficient range for room temperatures.
struct ModelledRadValveState
  {
  // Construct an instance, with sensible defaults, but no (room) temperature.
  // Defers its initialisation with room temperature until first tick().
  ModelledRadValveState() :
    initialised(false),
    isFiltering(false),
    valveMoved(false),
    cumulativeMovementPC(0),
    valveTurndownCountdownM(0), valveTurnupCountdownM(0)
    { }

  // Construct an instance, with sensible defaults, and current (room) temperature from the input state.
  // Does its initialisation with room temperature immediately.
  ModelledRadValveState(const ModelledRadValveInputState &inputState);

  // Perform per-minute tasks such as counter and filter updates then recompute valve position.
  // The input state must be complete including target and reference temperatures
  // before calling this including the first time whereupon some further lazy initialisation is done.
  //   * valvePCOpenRef  current valve position UPDATED BY THIS ROUTINE, in range [0,100]
  void tick(volatile uint8_t &valvePCOpenRef, const ModelledRadValveInputState &inputState);

  // True once all deferred initialisation done during the first tick().
  // This takes care of setting state that depends on run-time data
  // such as real temperatures to propagate into all the filters.
  bool initialised;

  // If true then filtering is being applied to temperatures since they are fast-changing.
  bool isFiltering;

  // True if the computed valve position was changed by tick().
  bool valveMoved;

  // Testable/reportable events.
  // Cleared at the start of each tick().
  // Set as appropriate by computeRequiredTRVPercentOpen() to indicate
  // particular activity and paths taken.
  // May only be reported and accessible in debug mode; primarily to facilitate unit testing.
  typedef enum
    {
    MRVE_NONE,      // No event.
    MRVE_OPENFAST,  // Fast open as per TODO-593.
    MRVE_DRAUGHT    // Cold draught detected.
    } event_t;
#if 1 || defined(DEBUG)
  mutable event_t lastEvent = MRVE_NONE;
  // Clear the last event, ie event state becomes MRVE_NONE.
  void clearEvent() const { lastEvent = MRVE_NONE; }
  // Set the event to be as passed.
  void setEvent(event_t event) const { lastEvent = event; }
#else
  // Dummy placeholders where event state not held.
  void clearEvent() const { }
  // Set the event to be as passed.
  void setEvent(event_t e) const { }
#endif

  // Cumulative valve movement count, as unsigned cumulative percent with rollover [0,8191].
  // This is a useful as a measure of battery consumption (slewing the valve)
  // and noise generated (and thus disturbance to humans) and of appropriate control damping.
  //
  // Keep as an unsigned 12-bit field (uint16_t x : 12) to ensure that
  // the value doesn't wrap round to -ve value
  // and can safely be sent/received in JSON by hosts with 16-bit signed ints,
  // and the maximum number of decimal digits used in its representation is limited to 4
  // and used efficiently (~80% use of the top digit).
  //
  // Daily allowance (in terms of battery/energy use) is assumed to be about 400% (DHD20141230),
  // so this should hold many times that value to avoid ambiguity from missed/infrequent readings,
  // especially given full slew (+100%) in nominally as little as 1 minute.
  uint16_t cumulativeMovementPC : 12;

  // Set non-zero when valve flow is constricted, and then counts down to zero.
  // Some or all attempts to open the valve are deferred while this is non-zero
  // to reduce valve hunting if there is string turbulence from the radiator
  // or maybe draughts from open windows/doors
  // causing measured temperatures to veer up and down.
  // This attempts to reduce excessive valve noise and energy use
  // and help to avoid boiler short-cycling.
  uint8_t valveTurndownCountdownM;
  // Mark flow as having been reduced.
  // TODO: possibly decrease reopen delay in comfort mode and increase in filtering/wide-deadband/eco mode.
  void valveTurndown() { valveTurndownCountdownM = DEFAULT_ANTISEEK_VALVE_REOPEN_DELAY_M; }
  // If true then avoid turning up the heat yet.
  bool dontTurnup() const { return(0 != valveTurndownCountdownM); }

  // Set non-zero when valve flow is increased, and then counts down to zero.
  // Some or all attempts to close the valve are deferred while this is non-zero
  // to reduce valve hunting if there is string turbulence from the radiator
  // or maybe draughts from open windows/doors
  // causing measured temperatures to veer up and down.
  // This attempts to reduce excessive valve noise and energy use
  // and help to avoid boiler short-cycling.
  uint8_t valveTurnupCountdownM;
  // Mark flow as having been increased.
  // TODO: possibly increase reclose delay in filtering/wide-deadband mode.
  void valveTurnup() { valveTurnupCountdownM = DEFAULT_ANTISEEK_VALVE_RECLOSE_DELAY_M; }
  // If true then avoid turning down the heat yet.
  bool dontTurndown() const { return(0 != valveTurnupCountdownM); }

  // Length of filter memory in ticks; strictly positive.
  // Must be at least 4, and may be more efficient at a power of 2.
  static const size_t filterLength = 16;

  // Previous unadjusted temperatures, 0 being the newest, and following ones successively older.
  // These values have any target bias removed.
  // Half the filter size times the tick() interval gives an approximate time constant.
  // Note that full response time of a typical mechanical wax-based TRV is ~20mins.
  int_fast16_t prevRawTempC16[filterLength];

  // Get smoothed raw/unadjusted temperature from the most recent samples.
  int_fast16_t getSmoothedRecent() const;

  // Get last change in temperature (C*16, signed); +ve means rising.
  int_fast16_t getRawDelta() const { return(prevRawTempC16[0] - prevRawTempC16[1]); }

  // Get last change in temperature (C*16, signed) from n ticks ago capped to filter length; +ve means rising.
  int_fast16_t getRawDelta(uint8_t n) const { return(prevRawTempC16[0] - prevRawTempC16[OTV0P2BASE::fnmin((size_t)n, filterLength-1)]); }

//  // Compute an estimate of rate/velocity of temperature change in C/16 per minute/tick.
//  // A positive value indicates that temperature is rising.
//  // Based on comparing the most recent smoothed value with an older smoothed value.
//  int getVelocityC16PerTick();

  // Computes a new valve position given supplied input state including the current valve position; [0,100].
  // Uses no state other than that passed as the arguments (thus unit testable).
  // Does not alter any of the input state.
  // Uses hysteresis and a proportional control and some other cleverness.
  // Is always willing to turn off quickly, but on slowly (AKA "slow start" algorithm),
  // and tries to eliminate unnecessary 'hunting' which makes noise and uses actuator energy.
  // Nominally called at a regular rate, once per minute.
  // All inputState values should be set to sensible values before starting.
  // Usually called by tick() which does required state updates afterwards.
  uint8_t computeRequiredTRVPercentOpen(const uint8_t currentValvePCOpen, const ModelledRadValveInputState &inputState) const;
  };


    }

#endif
