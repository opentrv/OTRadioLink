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

//#include <OTV0p2Base.h>

#include "OTRadValve_ModelledRadValve.h"

#include "utility/OTRadValve_AbstractRadValve.h"

namespace OTRadValve
    {


// Offset from raw temperature to get reference temperature in C/16.
static const int8_t refTempOffsetC16 = 8;

ModelledRadValveInputState::ModelledRadValveInputState(const int realTempC16) :
    targetTempC(12 /* FROST */),
    minPCOpen(OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN), maxPCOpen(100),
    widenDeadband(false), glacial(false), hasEcoBias(false), inBakeMode(false)
    { setReferenceTemperatures(realTempC16); }

// Calculate reference temperature from real temperature.
// Proportional temperature regulation is in a 1C band.
// By default, for a given target XC the rad is off at (X+1)C so temperature oscillates around that point.
// This routine shifts the reference point at which the rad is off to (X+0.5C)
// ie to the middle of the specified degree, which is more intuitive,
// and which may save a little energy if users target the specified temperatures.
// Suggestion c/o GG ~2014/10 code, and generally less misleading anyway!
void ModelledRadValveInputState::setReferenceTemperatures(const int currentTempC16)
  {
  const int referenceTempC16 = currentTempC16 + refTempOffsetC16; // TODO-386: push targeted temperature down by 0.5C to middle of degree.
  refTempC16 = referenceTempC16;
  }



// Minimum slew/error % distance in central range; should be larger than smallest temperature-sensor-driven step (6) to be effective; [1,100].
// Note: keeping TRV_MIN_SLEW_PC sufficiently high largely avoids spurious hunting back and forth from single-ulp noise.
#ifndef TRV_MIN_SLEW_PC
#define TRV_MIN_SLEW_PC 7
#endif
// Set maximum valve slew rate (percent/minute) when close to target temperature.
// Note: keeping TRV_MAX_SLEW_PC_PER_MIN small reduces noise and overshoot and surges of water
// (eg for when additionally charged by the m^3 of flow in district heating systems)
// and will likely work better with high-thermal-mass / slow-response systems such as UFH.
// Should be << 100%/min, and probably << 30%/min, given that 30% may be the effective control range of many rad valves.
#ifndef TRV_MAX_SLEW_PC_PER_MIN
#define TRV_MIN_SLEW_PC_PER_MIN 1 // Minimal slew rate (%/min) to keep flow rates as low as possible.
#ifndef TRV_SLEW_GLACIAL
#define TRV_MAX_SLEW_PC_PER_MIN 5 // Maximum normal slew rate (%/min), eg to fully open from off when well under target; [1,100].
#else
#define TRV_MAX_SLEW_PC_PER_MIN TRV_MIN_SLEW_PC_PER_MIN
#endif
#endif

// Derived from basic slew values.
#ifndef TRV_SLEW_GLACIAL
#define TRV_SLEW_PC_PER_MIN_VFAST (min(34,(4*TRV_MAX_SLEW_PC_PER_MIN))) // Takes >= 3 minutes for full travel.
#define TRV_SLEW_PC_PER_MIN_FAST (min(20,(2*TRV_MAX_SLEW_PC_PER_MIN))) // Takes >= 5 minutes for full travel.
#else
#define TRV_SLEW_PC_PER_MIN_FAST TRV_MAX_SLEW_PC_PER_MIN
#define TRV_SLEW_PC_PER_MIN_VFAST TRV_MAX_SLEW_PC_PER_MIN
#endif

// TODO-467: if defined then slow to glacial on when wide deadband has been specified implying reduced heating effort.
#define GLACIAL_ON_WITH_WIDE_DEADBAND

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

// Get smoothed raw/unadjusted temperature from the most recent samples.
int ModelledRadValveState::getSmoothedRecent() const
  { return(smallIntMean<filterLength>(prevRawTempC16)); }

//// Compute an estimate of rate/velocity of temperature change in C/16 per minute/tick.
//// A positive value indicates that temperature is rising.
//// Based on comparing the most recent smoothed value with an older smoothed value.
//int ModelledRadValveState::getVelocityC16PerTick()
//  {
//  const int oldSmoothed = smallIntMean<filterLength/2>(prevRawTempC16 + (filterLength/2));
//  const int newSmoothed = getSmoothedRecent();
//  const int velocity = (newSmoothed - oldSmoothed + (int)(filterLength/4)) / (int)(filterLength/2); // Avoid going unsigned by accident.
////DEBUG_SERIAL_PRINT_FLASHSTRING("old&new sm, velocity: ");
////DEBUG_SERIAL_PRINT(oldSmoothed);
////DEBUG_SERIAL_PRINT('&');
////DEBUG_SERIAL_PRINT(newSmoothed);
////DEBUG_SERIAL_PRINT(',');
////DEBUG_SERIAL_PRINT(velocity);
////DEBUG_SERIAL_PRINTLN();
//  return(velocity);
//  }

// Maximum jump between adjacent readings before forcing filtering.
// Too small a value may in some circumstances cap room rate rise to this per minute.
// Too large a value may fail to sufficiently help damp oscillations and overshoot.
// As to be at least as large as the minimum temperature sensor precision to avoid false triggering of the filter.
// Typical values range from 2 (for 1/8C precision temperature sensor) up to 4.
static const int MAX_TEMP_JUMP_C16 = 3; // 3/16C.

// Perform per-minute tasks such as counter and filter updates then recompute valve position.
// The input state must be complete including target and reference temperatures
// before calling this including the first time whereupon some further lazy initialisation is done.
//   * valvePCOpenRef  current valve position UPDATED BY THIS ROUTINE, in range [0,100]
void ModelledRadValveState::tick(volatile uint8_t &valvePCOpenRef, const ModelledRadValveInputState &inputState)
  {
  const int rawTempC16 = inputState.refTempC16 - refTempOffsetC16; // Remove adjustment for target centre.
  if(!initialised)
    {
    // Fill the filter memory with the current room temperature.
    for(int i = filterLength; --i >= 0; ) { prevRawTempC16[i] = rawTempC16; }
    initialised = true;
    }

  // Shift in the latest (raw) temperature.
  for(int i = filterLength; --i > 0; ) { prevRawTempC16[i] = prevRawTempC16[i-1]; }
  prevRawTempC16[0] = rawTempC16;

  // Disable/enable filtering.
  // Allow possible exit from filtering for next time
  // if the raw value is close enough to the current filtered value
  // so that reverting to unfiltered will not of itself cause a big jump.
  if(isFiltering)
    {
    if(abs(getSmoothedRecent() - rawTempC16) <= MAX_TEMP_JUMP_C16) { isFiltering = false; }
    }
  // Force filtering (back) on if any adjacent past readings are wildly different.
  if(!isFiltering)
    {
    for(int i = 1; i < filterLength; ++i) { if(abs(prevRawTempC16[i] - prevRawTempC16[i-1]) > MAX_TEMP_JUMP_C16) { isFiltering = true; break; } }
    }

  // Tick count down timers.
  if(valveTurndownCountdownM > 0) { --valveTurndownCountdownM; }
  if(valveTurnupCountdownM > 0) { --valveTurnupCountdownM; }

  // Update the modelled state including the valve position passed by reference.
  const uint8_t newValvePC = computeRequiredTRVPercentOpen(valvePCOpenRef, inputState);
  const bool changed = (newValvePC != valvePCOpenRef);
  if(changed)
    {
    if(newValvePC > valvePCOpenRef)
      {
      // Defer reclosing valve to avoid excessive hunting.
      valveTurnup();
      cumulativeMovementPC += (newValvePC - valvePCOpenRef);
      }
    else
      {
      // Defer opening valve to avoid excessive hunting.
      valveTurndown();
      cumulativeMovementPC += (valvePCOpenRef - newValvePC);
      }
    valvePCOpenRef = newValvePC;
    }
  valveMoved = changed;
  }

// Computes a new valve position given supplied input state including the current valve position; [0,100].
// Uses no state other than that passed as the arguments (thus unit testable).
// FIXME: This supplied 'retained' state may be updated, nothing else is changed.
// Uses hysteresis and a proportional control and some other cleverness.
// Is always willing to turn off quickly, but on slowly (AKA "slow start" algorithm),
// and tries to eliminate unnecessary 'hunting' which makes noise and uses actuator energy.
// Nominally called at a regular rate, once per minute.
// All inputState values should be set to sensible values before starting.
// Usually called by tick() which does required state updates afterwards.
uint8_t ModelledRadValveState::computeRequiredTRVPercentOpen(const uint8_t valvePCOpen, const ModelledRadValveInputState &inputState) const
  {
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINT_FLASHSTRING("targ=");
DEBUG_SERIAL_PRINT(inputState.targetTempC);
DEBUG_SERIAL_PRINT_FLASHSTRING(" room=");
DEBUG_SERIAL_PRINT(inputState.refTempC);
DEBUG_SERIAL_PRINTLN();
#endif

  // Possibly-adjusted and.or smoothed temperature to use for targeting.
  const int adjustedTempC16 = isFiltering ? (getSmoothedRecent() + refTempOffsetC16) : inputState.refTempC16;
  const int8_t adjustedTempC = (adjustedTempC16 >> 4);

  // (Well) under temp target: open valve up.
  if(adjustedTempC < inputState.targetTempC)
    {
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("under temp");
    // Limit valve open slew to help minimise overshoot and actuator noise.
    // This should also reduce nugatory setting changes when occupancy (etc) is fluctuating.
    // Thus it may take several minutes to turn the radiator fully on,
    // though probably opening the first third or so will allow near-maximum heat output in practice.
    if(valvePCOpen < inputState.maxPCOpen)
      {
      // If room is well below target and in BAKE mode then immediately open to maximum.
      // Need debounced bake mode value to avoid spurious slamming open of the valve as the user cycles through modes.
      if(inputState.inBakeMode) { return(inputState.maxPCOpen); }

      // Reduce valve hunting: defer re-opening if recently closed.
      if(dontTurnup()) { return(valvePCOpen); }

      // True if a long way below target (more than 1C below target).
      const bool vBelowTarget = (adjustedTempC < inputState.targetTempC-1);

      // Open glacially if explicitly requested or if temperature overshoot has happened or is a danger,
      // or if there's likely no one going to care about getting on target particularly quickly (or would prefer reduced noise).
      //
      // If already at least the expected minimum % open for significant flow,
      //   and wide deadband (eg in FROST mode or dark) to avoid over-eager pre-warm / anticipation for example (TODO-467)
      // OR
      //   filtering is on indicating rapid recent changes or jitter,
      //   and the last raw change was upwards,
      // then force glacial mode to try to damp oscillations and avoid overshoot and excessive valve movement (TODO-453).
      const bool beGlacial = inputState.glacial ||
          ((valvePCOpen >= inputState.minPCOpen) &&
              (
#if defined(GLACIAL_ON_WITH_WIDE_DEADBAND)
               // Don't work so hard to reach and hold target temp with wide deadband
               // (widened eg because room is dark, or this is a pre-warm in FROST mode, or temperature is gyrating)
               // and not comfort mode nor massively below (possibly already setback) target temp.
               (inputState.widenDeadband && inputState.hasEcoBias && !vBelowTarget) ||
#endif
               (isFiltering && (getRawDelta() > 0)))); // FIXME: maybe redundant w/ GLACIAL_ON_WITH_WIDE_DEADBAND and widenDeadband set when isFiltering is true
      if(beGlacial) { return(valvePCOpen + 1); }

      // If well below target and without a wide deadband,
      // go straight to 'moderately open' if less open currently (TODO-593),
      // which should allow flow and turn the boiler on ASAP,
      // a little like a mini-BAKE.
      const uint8_t cappedModeratelyOpen = min(inputState.maxPCOpen, OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN);
      if(vBelowTarget && (valvePCOpen < cappedModeratelyOpen) &&
          !inputState.widenDeadband)
          { return(cappedModeratelyOpen); }

      // Ensure that the valve opens quickly from cold for acceptable response (TODO-593).
      // Less fast if already moderately open or with a wide deadband.
      const uint8_t slewRate =
          ((valvePCOpen >= OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN) || !inputState.widenDeadband) ?
              TRV_MAX_SLEW_PC_PER_MIN :
              // Open VFAST until almost all valves would be significantly open.
              ((valvePCOpen >= OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN) ?
                  TRV_SLEW_PC_PER_MIN_FAST : TRV_SLEW_PC_PER_MIN_VFAST);
      const uint8_t minOpenFromCold = max(slewRate, inputState.minPCOpen);
      // Open to 'minimum' likely open state immediately if less open currently.
      if(valvePCOpen < minOpenFromCold) { return(minOpenFromCold); }
      // Slew open relatively gently...
      return(min((uint8_t)(valvePCOpen + slewRate), inputState.maxPCOpen)); // Capped at maximum.
      }
    // Keep open at maximum allowed.
    return(inputState.maxPCOpen);
    }

  // (Well) over temp target: close valve down.
  if(adjustedTempC > inputState.targetTempC)
    {
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("over temp");

    if(0 != valvePCOpen)
      {
      // Reduce valve hunting: defer re-closing if recently opened.
      if(dontTurndown()) { return(valvePCOpen); }

      // True if just above the the proportional range.
      const bool justOverTemp = (adjustedTempC == inputState.targetTempC+1);

      // TODO-453: avoid closing the valve at all when the temperature error is small and falling, and there is a widened deadband.
      if(justOverTemp && inputState.widenDeadband && (getRawDelta() < 0)) { return(valvePCOpen); }

      // TODO-482: glacial close if temperature is jittery and not too far above target.
      if(justOverTemp && isFiltering) { return(valvePCOpen - 1); }

      // Continue shutting valve slowly as not yet fully closed.
      // TODO-117: allow very slow final turn off to help systems with poor bypass, ~1% per minute.
      // Special slow-turn-off rules for final part of travel at/below "min % really open" floor.
      const uint8_t minReallyOpen = inputState.minPCOpen;
      const uint8_t lingerThreshold = (minReallyOpen > 0) ? (minReallyOpen-1) : 0;
      if(valvePCOpen < minReallyOpen)
        {
        // If lingered long enough then do final chunk in one burst to help avoid valve hiss and temperature overshoot.
        if((DEFAULT_MAX_RUN_ON_TIME_M < minReallyOpen) && (valvePCOpen < minReallyOpen - DEFAULT_MAX_RUN_ON_TIME_M))
          { return(0); } // Shut valve completely.
        return(valvePCOpen - 1); // Turn down as slowly as reasonably possible to help boiler cool.
        }

      // TODO-109: with comfort bias close relatively slowly to reduce wasted effort from minor overshoots.
      // TODO-453: close relatively slowly when temperature error is small (<1C) to reduce wasted effort from minor overshoots.
      if(((!inputState.hasEcoBias) || justOverTemp || isFiltering) &&
         (valvePCOpen > constrain(((int)lingerThreshold) + TRV_SLEW_PC_PER_MIN_FAST, TRV_SLEW_PC_PER_MIN_FAST, inputState.maxPCOpen)))
        { return(valvePCOpen - TRV_SLEW_PC_PER_MIN_FAST); }

      // Else (by default) force to (nearly) off immediately when requested, ie eagerly stop heating to conserve energy.
      // In any case percentage open should now be low enough to stop calling for heat immediately.
      return(lingerThreshold);
      }

    // Ensure that the valve is/remains fully shut.
    return(0);
    }

  // Close to (or at) temp target: set valve partly open to try to tightly regulate.
  //
  // Use currentTempC16 lsbits to set valve percentage for proportional feedback
  // to provide more efficient and quieter TRV drive and probably more stable room temperature.
  const uint8_t lsbits = (uint8_t) (adjustedTempC16 & 0xf); // LSbits of temperature above base of proportional adjustment range.
//    uint8_t tmp = (uint8_t) (refTempC16 & 0xf); // Only interested in lsbits.
  const uint8_t tmp = 16 - lsbits; // Now in range 1 (at warmest end of 'correct' temperature) to 16 (coolest).
  const uint8_t ulpStep = 6;
  // Get to nominal range 6 to 96, eg valve nearly shut just below top of 'correct' temperature window.
  const uint8_t targetPORaw = tmp * ulpStep;
  // Constrain from below to likely minimum-open value, in part to deal with TODO-117 'linger open' in lieu of boiler bypass.
  // Constrain from above by maximum percentage open allowed, eg for pay-by-volume systems.
  const uint8_t targetPO = constrain(targetPORaw, inputState.minPCOpen, inputState.maxPCOpen);

  // Reduce spurious valve/boiler adjustment by avoiding movement at all unless current temperature error is significant.
  if(targetPO != valvePCOpen)
    {
    // True iff valve needs to be closed somewhat.
    const bool tooOpen = (targetPO < valvePCOpen);
    // Compute the minimum/epsilon slew adjustment allowed (the deadband).
    // Also increase effective deadband if temperature resolution is lower than 1/16th, eg 8ths => 1+2*ulpStep minimum.
// FIXME //    const uint8_t realMinUlp = 1 + (inputState.isLowPrecision ? 2*ulpStep : ulpStep); // Assume precision no coarser than 1/8C.
    const uint8_t realMinUlp = 1 + ulpStep;
    const uint8_t _minAbsSlew = (uint8_t)(inputState.widenDeadband ? max(min(OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN/2,max(TRV_MAX_SLEW_PC_PER_MIN,2*TRV_MIN_SLEW_PC)), 2+TRV_MIN_SLEW_PC) : TRV_MIN_SLEW_PC);
    const uint8_t minAbsSlew = max(realMinUlp, _minAbsSlew);
    if(tooOpen) // Currently open more than required.  Still below target at top of proportional range.
      {
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("slightly too open");
      const uint8_t slew = valvePCOpen - targetPO;
      // Ensure no hunting for ~1ulp temperature wobble.
      if(slew < minAbsSlew) { return(valvePCOpen); }

      // Reduce valve hunting: defer re-closing if recently opened.
      if(dontTurndown()) { return(valvePCOpen); }

      // TODO-453: avoid closing the valve at all when the (raw) temperature is not rising, so as to minimise valve movement.
      // Since the target is the top of the proportional range than nothing within it requires the temperature to be *forced* down.
      // Possibly don't apply this rule at the very top of the range in case filtering is on and the filtered value moves differently to the raw.
      if(getRawDelta() <= 0) { return(valvePCOpen); }

      // Close glacially if explicitly requested or if temperature undershoot has happened or is a danger.
      // Also be glacial if in soft setback which aims to allow temperatures to drift passively down a little.
      //   (TODO-451, TODO-467: have darkness only immediately trigger a 'soft setback' using wide deadband)
      // This assumes that most valves more than about 1/3rd open can deliver significant power, esp if not statically balanced.
      // TODO-482: try to deal better with jittery temperature readings.
      const bool beGlacial = inputState.glacial ||
#if defined(GLACIAL_ON_WITH_WIDE_DEADBAND)
          ((inputState.widenDeadband || isFiltering) && (valvePCOpen <= OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN)) ||
#endif
          (lsbits < 8);
      if(beGlacial) { return(valvePCOpen - 1); }

      if(slew > TRV_SLEW_PC_PER_MIN_FAST)
          { return(valvePCOpen - TRV_SLEW_PC_PER_MIN_FAST); } // Cap slew rate.
      // Adjust directly to target.
      return(targetPO);
      }

    // if(targetPO > TRVPercentOpen) // Currently open less than required.  Still below target at top of proportional range.
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("slightly too closed");
    // If room is well below target and in BAKE mode then immediately open to maximum.
    // Needs debounced bake mode value to avoid spuriously slamming open the valve as the user cycles through modes.
    if(inputState.inBakeMode) { return(inputState.maxPCOpen); }

    const uint8_t slew = targetPO - valvePCOpen;
    // To to avoid hunting around boundaries of a ~1ulp temperature step.
    if(slew < minAbsSlew) { return(valvePCOpen); }

    // Reduce valve hunting: defer re-opening if recently closed.
    if(dontTurnup()) { return(valvePCOpen); }

    // TODO-453: minimise valve movement (and thus noise and battery use).
    // Keeping the temperature steady anywhere in the target proportional range
    // while minimising valve moment/noise/etc is a good goal,
    // so if raw temperatures are rising at the moment then leave the valve as-is.
    // If fairly near the final target then also leave the valve as-is (TODO-453 & TODO-451).
    const int rise = getRawDelta();
    if(rise > 0) { return(valvePCOpen); }
    if( /* (0 == rise) && */ (lsbits >= (inputState.widenDeadband ? 8 : 12))) { return(valvePCOpen); }

    // Open glacially if explicitly requested or if temperature overshoot has happened or is a danger.
    // Also be glacial if in soft setback which aims to allow temperatures to drift passively down a little.
    //   (TODO-451, TODO-467: have darkness only immediately trigger a 'soft setback' using wide deadband)
    // This assumes that most valves more than about 1/3rd open can deliver significant power, esp if not statically balanced.
    const bool beGlacial = inputState.glacial ||
#if defined(GLACIAL_ON_WITH_WIDE_DEADBAND)
        inputState.widenDeadband ||
#endif
        (lsbits >= 8) || ((lsbits >= 4) && (valvePCOpen >= OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN));
    if(beGlacial) { return(valvePCOpen + 1); }

    // Slew open faster with comfort bias.
    const uint8_t maxSlew = (!inputState.hasEcoBias) ? TRV_SLEW_PC_PER_MIN_FAST : TRV_MAX_SLEW_PC_PER_MIN;
    if(slew > maxSlew)
        { return(valvePCOpen + maxSlew); } // Cap slew rate open.
    // Adjust directly to target.
    return(targetPO);
    }

  // Leave value position as was...
  return(valvePCOpen);
  }


    }
