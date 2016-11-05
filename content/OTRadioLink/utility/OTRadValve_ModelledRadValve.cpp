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

#include <stdlib.h>
#include "utility/OTRadValve_AbstractRadValve.h"

#include "OTRadValve_ModelledRadValve.h"


namespace OTRadValve
    {


// Simple mean filter.
// Find mean of group of ints where sum can be computed in an int without loss.
// TODO: needs a unit test or three.
template<size_t N> int_fast16_t smallIntMean(const int_fast16_t data[N])
  {
  // Extract mean.
  // Assume values and sum will be nowhere near the limits.
  int sum = 0;
  for(int8_t i = N; --i >= 0; ) { sum += data[i]; }
  // Compute rounded-up mean.
  return((sum + (int)(N/2)) / (int)N); // Avoid accidental computation as unsigned...
  }


// Get smoothed raw/unadjusted temperature from the most recent samples.
int_fast16_t ModelledRadValveState::getSmoothedRecent() const
  { return(smallIntMean<filterLength>(prevRawTempC16)); }

//// Compute an estimate of rate/velocity of temperature change in C/16 per minute/tick.
//// A positive value indicates that temperature is rising.
//// Based on comparing the most recent smoothed value with an older smoothed value.
//int ModelledRadValveState::getVelocityC16PerTick()
//  {
//  const int oldSmoothed = smallIntMean<filterLength/2>(prevRawTempC16 + (filterLength/2));
//  const int newSmoothed = getSmoothedRecent();
//  const int velocity = (newSmoothed - oldSmoothed + (int)(filterLength/4)) / (int)(filterLength/2); // Avoid going unsigned by accident.
////V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("old&new sm, velocity: ");
////V0P2BASE_DEBUG_SERIAL_PRINT(oldSmoothed);
////V0P2BASE_DEBUG_SERIAL_PRINT('&');
////V0P2BASE_DEBUG_SERIAL_PRINT(newSmoothed);
////V0P2BASE_DEBUG_SERIAL_PRINT(',');
////V0P2BASE_DEBUG_SERIAL_PRINT(velocity);
////V0P2BASE_DEBUG_SERIAL_PRINTLN();
//  return(velocity);
//  }

// Maximum jump between adjacent readings before forcing filtering; strictly +ve.
// Too small a value may in some circumstances cap room rate rise to this per minute.
// Too large a value may fail to sufficiently help damp oscillations and overshoot.
// As to be at least as large as the minimum temperature sensor precision to avoid false triggering of the filter.
// Typical values range from 2 (for better-than 1/8C-precision temperature sensor) up to 4.
static const uint8_t MAX_TEMP_JUMP_C16 = 3; // 3/16C.

// Minimum drop in temperature over recent time to trigger 'window open' response; strictly +ve.
// Nominally target up 0.25C--1C drop over a few minutes (limited by the filter length).
// TODO-621: in case of very sharp drop in temperature,
// assume that a window or door has been opened,
// by accident or to ventilate the room,
// so suppress heating to reduce waste.
//
// See one sample 'airing' data set:
//     http://www.earth.org.uk/img/20160930-16WWmultisensortempL.README.txt
//     http://www.earth.org.uk/img/20160930-16WWmultisensortempL.png
//     http://www.earth.org.uk/img/20160930-16WWmultisensortempL.json.xz
//
// 7h (hall, A9B2F7C089EECD89) saw a sharp fall and recovery, possibly from an external door being opened:
// 1C over 10 minutes then recovery by nearly 0.5C over next half hour.
// Note that there is a potential 'sensitising' occupancy signal available,
// ie sudden occupancy may allow triggering with a lower temperature drop.
//[ "2016-09-30T06:45:18Z", "", {"@":"A9B2F7C089EECD89","+":15,"T|C16":319,"H|%":65,"O":1} ]
//[ "2016-09-30T06:57:10Z", "", {"@":"A9B2F7C089EECD89","+":2,"L":101,"T|C16":302,"H|%":60} ]
//[ "2016-09-30T07:05:10Z", "", {"@":"A9B2F7C089EECD89","+":4,"T|C16":303,"v|%":0} ]
//[ "2016-09-30T07:09:08Z", "", {"@":"A9B2F7C089EECD89","+":5,"tT|C":16,"T|C16":305} ]
//[ "2016-09-30T07:21:08Z", "", {"@":"A9B2F7C089EECD89","+":8,"O":2,"T|C16":308,"H|%":64} ]
//[ "2016-09-30T07:33:12Z", "", {"@":"A9B2F7C089EECD89","+":11,"tS|C":0,"T|C16":310} ]
//
// 1g (bedroom, FEDA88A08188E083) saw a slower fall, assumed from airing:
// initially of .25C in 12m, 0.75C over 1h, bottoming out ~2h later down ~2C.
// Note that there is a potential 'sensitising' occupancy signal available,
// ie sudden occupancy may allow triggering with a lower temperature drop.
//[ "2016-09-30T06:27:30Z", "", {"@":"FEDA88A08188E083","+":8,"tT|C":17,"tS|C":0} ]
//[ "2016-09-30T06:31:38Z", "", {"@":"FEDA88A08188E083","+":9,"gE":0,"T|C16":331,"H|%":67} ]
//[ "2016-09-30T06:35:30Z", "", {"@":"FEDA88A08188E083","+":10,"T|C16":330,"O":2,"L":2} ]
//[ "2016-09-30T06:43:30Z", "", {"@":"FEDA88A08188E083","+":12,"H|%":65,"T|C16":327,"O":2} ]
//[ "2016-09-30T06:59:34Z", "", {"@":"FEDA88A08188E083","+":0,"T|C16":325,"H|%":64,"O":1} ]
//[ "2016-09-30T07:07:34Z", "", {"@":"FEDA88A08188E083","+":2,"H|%":63,"T|C16":324,"O":1} ]
//[ "2016-09-30T07:15:36Z", "", {"@":"FEDA88A08188E083","+":4,"L":95,"tT|C":13,"tS|C":4} ]
//[ "2016-09-30T07:19:30Z", "", {"@":"FEDA88A08188E083","+":5,"vC|%":0,"gE":0,"T|C16":321} ]
//[ "2016-09-30T07:23:29Z", "", {"@":"FEDA88A08188E083","+":6,"T|C16":320,"H|%":63,"O":1} ]
//[ "2016-09-30T07:31:27Z", "", {"@":"FEDA88A08188E083","+":8,"L":102,"T|C16":319,"H|%":63} ]
// ...
//[ "2016-09-30T08:15:27Z", "", {"@":"FEDA88A08188E083","+":4,"T|C16":309,"H|%":61,"O":1} ]
//[ "2016-09-30T08:27:41Z", "", {"@":"FEDA88A08188E083","+":7,"vC|%":0,"T|C16":307} ]
//[ "2016-09-30T08:39:33Z", "", {"@":"FEDA88A08188E083","+":10,"T|C16":305,"H|%":61,"O":1} ]
//[ "2016-09-30T08:55:29Z", "", {"@":"FEDA88A08188E083","+":14,"T|C16":303,"H|%":61,"O":1} ]
//[ "2016-09-30T09:07:37Z", "", {"@":"FEDA88A08188E083","+":1,"gE":0,"T|C16":302,"H|%":61} ]
//[ "2016-09-30T09:11:29Z", "", {"@":"FEDA88A08188E083","+":2,"T|C16":301,"O":1,"L":175} ]
//[ "2016-09-30T09:19:41Z", "", {"@":"FEDA88A08188E083","+":4,"T|C16":301,"H|%":61,"O":1} ]
//
// Should probably be significantly larger than MAX_TEMP_JUMP_C16 to avoid triggering alongside any filtering.
// Needs to be be a fast enough fall NOT to be triggered by normal temperature gyrations close to a radiator.
static const uint8_t MIN_WINDOW_OPEN_TEMP_FALL_C16 = OTV0P2BASE::fnmax(MAX_TEMP_JUMP_C16+2, 5); // Just over 1/4C.
// Minutes over which temperature should be falling to trigger 'window open' response; strictly +ve.
// TODO-621.
// Needs to be be a fast enough fall NOT to be triggered by normal temperature gyrations close to a radiator.
// Is capped in practice at the filter length.
static const uint8_t MIN_WINDOW_OPEN_TEMP_FALL_M = 13;

// Construct an instance, with sensible defaults, and current (room) temperature from the input state.
// Does its initialisation with room temperature immediately.
ModelledRadValveState::ModelledRadValveState(const ModelledRadValveInputState &inputState, const bool _alwaysGlacial) :
  alwaysGlacial(_alwaysGlacial),
  initialised(true),
  isFiltering(false),
  valveMoved(false),
  cumulativeMovementPC(0),
  valveTurndownCountdownM(0), valveTurnupCountdownM(0)
  {
  // Fills array exactly as tick() would when !initialised.
  const int_fast16_t rawTempC16 = inputState.refTempC16 - ModelledRadValveInputState::refTempOffsetC16; // Remove adjustment for target centre.
  for(int i = filterLength; --i >= 0; ) { prevRawTempC16[i] = rawTempC16; }
  }

// Perform per-minute tasks such as counter and filter updates then recompute valve position.
// The input state must be complete including target and reference temperatures
// before calling this including the first time whereupon some further lazy initialisation is done.
//   * valvePCOpenRef  current valve position UPDATED BY THIS ROUTINE, in range [0,100]
void ModelledRadValveState::tick(volatile uint8_t &valvePCOpenRef, const ModelledRadValveInputState &inputState)
  {
  // Forget last event if any.
  clearEvent();

  const int_fast16_t rawTempC16 = inputState.refTempC16 - ModelledRadValveInputState::refTempOffsetC16; // Remove adjustment for target centre.
  // Do some one-off work on first tick in new instance.
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
    { if(abs(getSmoothedRecent() - rawTempC16) <= MAX_TEMP_JUMP_C16) { isFiltering = false; } }
  // Force filtering (back) on if any adjacent past readings are wildly different.
  else
    {
    for(unsigned int i = 1; i < filterLength; ++i) { if(abs(prevRawTempC16[i] - prevRawTempC16[i-1]) > MAX_TEMP_JUMP_C16) { isFiltering = true; break; } }
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
// Uses no state other than that passed as the arguments (thus is unit testable).
// Does not alter any of the input state.
// Uses hysteresis and a proportional control and some other cleverness.
// Is always willing to turn off quickly, but on slowly (AKA "slow start" algorithm),
// and tries to eliminate unnecessary 'hunting' which makes noise and uses actuator energy.
// Nominally called at a regular rate, once per minute.
// All inputState values should be set to sensible values before starting.
// Usually called by tick() which does required state updates afterwards.
uint8_t ModelledRadValveState::computeRequiredTRVPercentOpen(const uint8_t valvePCOpen, const ModelledRadValveInputState &inputState) const
  {
  // Minimum slew/error % distance in central range; should be larger than smallest temperature-sensor-driven step (6) to be effective; [1,100].
  // Note: keeping TRV_MIN_SLEW_PC sufficiently high largely avoids spurious hunting back and forth from single-ulp noise.
  static constexpr uint8_t TRV_MIN_SLEW_PC = 7;
  // Set maximum valve slew rate (percent/minute) when close to target temperature.
  // Note: keeping TRV_MAX_SLEW_PC_PER_MIN small reduces noise and overshoot and surges of water
  // (eg for when additionally charged by the m^3 of flow in district heating systems)
  // and will likely work better with high-thermal-mass / slow-response systems such as UFH.
  // Should be << 100%/min, and probably << 30%/min, given that 30% may be the effective control range of many rad valves.
  static constexpr uint8_t TRV_MIN_SLEW_PC_PER_MIN = 1; // Minimal slew rate (%/min) to keep flow rates as low as possible.
  static const uint8_t TRV_MAX_SLEW_PC_PER_MIN = alwaysGlacial ? TRV_MIN_SLEW_PC_PER_MIN : 5;
  // Derived from basic slew values.
  static const uint8_t TRV_SLEW_PC_PER_MIN_FAST = alwaysGlacial ? TRV_MAX_SLEW_PC_PER_MIN : (OTV0P2BASE::fnmin(20,(2*TRV_MAX_SLEW_PC_PER_MIN))); // Takes >= 5 minutes for full travel.
  static const uint8_t TRV_SLEW_PC_PER_MIN_VFAST = alwaysGlacial ? TRV_MAX_SLEW_PC_PER_MIN : (OTV0P2BASE::fnmin(34,(4*TRV_MAX_SLEW_PC_PER_MIN))); // Takes >= 3 minutes for full travel.

  // Possibly-adjusted and/or smoothed temperature to use for targeting.
  const int_fast16_t adjustedTempC16 = isFiltering ? (getSmoothedRecent() + ModelledRadValveInputState::refTempOffsetC16) : inputState.refTempC16;
  // When reduced to whole Celsius then fewer bits are needed to cover expected temperatures.
  const int_fast8_t adjustedTempC = (int_fast8_t) (adjustedTempC16 >> 4);

  // (Well) under temp target: open valve up.
  if(adjustedTempC < inputState.targetTempC)
    {
//V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("under temp");
    // Force to fully open in BAKE mode.
    // Need debounced bake mode value to avoid spurious slamming open of the valve as the user cycles through modes.
    if(inputState.inBakeMode) { return(inputState.maxPCOpen); }

    // Avoid trying to heat the outside world when a window or door is opened (TODO-621).
    // This is a short-term tactical response to a persistent cold draught,
    // eg from a window being opened to ventilate a room manually,
    // or an exterior door being left open.
    //
    // BECAUSE not currently very close to target
    // (possibly because of sudden temperature drop already from near target)
    // AND IF system has 'eco' bias (so tries harder to save energy)
    // and no fast response has been requested (eg by recent user operation of the controls)
    // and the temperature is at/above a minimum frost safety threshold
    // and the temperature is currently falling (over the last measurement period)
    // and the temperature fall over the last few minutes is large
    // THEN attempt to stop calling for heat immediately and continue to turn down
    // (though if inhibited from turning down yet*, then instead avoid opening any further).
    // Turning the valve down should also inhibit reopening it for a little while,
    // even once the temperature has stopped falling.
    //
    // *This may be because the valve has just recently been closed
    // in which case some sort of a temperature drop is not astonishing.
    //
    // It seems sensible to stop calling for heat immediately if one of these events seems to be happening,
    // though that (a) may not stop the boiler and heat delivery if other rooms are still calling for heat
    // and (b) may prevent the boiler being started again for a while even if this was a false alarm,
    // so may annoy users and make heating control seem erratic,
    // so only do this in 'eco' mode where permission has been given to try harder to save energy.
    //
    // TODO: could restrict this to when (valvePCOpen >= inputState.minPCOpen) to save some thrashing and allow lingering.
    // TODO: could explicitly avoid applying this when valve has recently been closed to avoid unwanted feedback loop.
    if(inputState.hasEcoBias &&
       (!inputState.fastResponseRequired) && // Avoid subverting recent manual call for heat.
       (adjustedTempC >= MIN_TARGET_C) &&
       (getRawDelta() < 0) &&
       (getRawDelta(MIN_WINDOW_OPEN_TEMP_FALL_M) <= -(int)MIN_WINDOW_OPEN_TEMP_FALL_C16))
        {
        setEvent(MRVE_DRAUGHT); // Report draught detected.
        if(!dontTurndown())
          {
          // Try to turn down far enough to stop calling for heat immediately.
          if(valvePCOpen >= OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN)
            { return(OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN - 1); }
          // Else continue to close at a reasonable pace.
          if(valvePCOpen > TRV_MAX_SLEW_PC_PER_MIN)
            { return(valvePCOpen - TRV_MAX_SLEW_PC_PER_MIN); }
          // Else close it.
          return(0);
          }
        // Else at least avoid opening the valve.
        return(valvePCOpen);
        }

    // Limit valve open slew to help minimise overshoot and actuator noise.
    // This should also reduce nugatory setting changes when occupancy (etc) is fluctuating.
    // Thus it may take several minutes to turn the radiator fully on,
    // though probably opening the first third or so will allow near-maximum heat output in practice.
    if(valvePCOpen < inputState.maxPCOpen)
      {
      // Reduce valve hunting: defer re-opening if recently closed.
      if(dontTurnup()) { return(valvePCOpen); }

      // True if a long way below target (more than 1C below target).
      const bool vBelowTarget = (adjustedTempC < inputState.targetTempC-1);

      // Open glacially if explicitly requested or if temperature overshoot has happened or is a danger,
      // or if there's likely no one going to care about getting on target particularly quickly (or would prefer reduced noise).
      //
      // If already at least at the expected minimum % open for significant flow,
      // AND a wide deadband has been allowed by the caller (eg room dark or filtering is on or doing pre-warm)
      //   if not way below target to avoid over-eager pre-warm / anticipation for example (TODO-467)
      //     OR
      //   if filtering is on indicating rapid recent changes or jitter, and the last raw change was upwards,
      // THEN force glacial mode to try to damp oscillations and avoid overshoot and excessive valve movement (TODO-453).
      const bool beGlacial = inputState.glacial ||
          ((valvePCOpen >= inputState.minPCOpen) && inputState.widenDeadband && !inputState.fastResponseRequired &&
              (
//      #if defined(GLACIAL_ON_WITH_WIDE_DEADBAND)
               // Don't rush to open the valve (GLACIAL_ON_WITH_WIDE_DEADBAND: TODO-467)
               // if neither in comfort mode nor massively below (possibly already setback) target temp.
               (inputState.hasEcoBias && !vBelowTarget) ||
//      #endif
               // Don't rush to open the valve
               // if temperature is jittery but is moving in the right direction.
               (isFiltering && (getRawDelta() > 0)))); // FIXME: maybe redundant w/ GLACIAL_ON_WITH_WIDE_DEADBAND and widenDeadband set when isFiltering is true
      if(beGlacial) { return(valvePCOpen + 1); }

      // If well below target (and without a wide deadband),
      // or needing a fast response to manual input to be responsive (TODO-593),
      // then jump straight to (just over*) 'moderately open' if less open currently,
      // which should allow flow and turn the boiler on ASAP,
      // a little like a mini-BAKE.
      // For this to work, don't set a wide deadband when, eg, user has just touched the controls.
      // *Jump to just over moderately-open threshold to defeat any small rounding errors in the data path, etc,
      // since boiler is likely to regard this threshold as a trigger to immediate action.
      const uint8_t cappedModeratelyOpen = OTV0P2BASE::fnmin(inputState.maxPCOpen, OTV0P2BASE::fnmin((uint8_t)99, (uint8_t)(OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN+TRV_SLEW_PC_PER_MIN_FAST)));
      if((valvePCOpen < cappedModeratelyOpen) &&
         (inputState.fastResponseRequired || (vBelowTarget && !inputState.widenDeadband)))
          {
          setEvent(MRVE_OPENFAST);
          return(cappedModeratelyOpen);
          }

      // Ensure that the valve opens quickly from cold for acceptable response (TODO-593)
      // both locally in terms of valve position and also in terms of the boiler responding.
      // Less fast if already moderately open or with a wide deadband.
      const uint8_t slewRate =
          ((valvePCOpen > OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN) || !inputState.widenDeadband) ?
              TRV_MAX_SLEW_PC_PER_MIN : TRV_SLEW_PC_PER_MIN_VFAST;
      const uint8_t minOpenFromCold = OTV0P2BASE::fnmax(slewRate, inputState.minPCOpen);
      // Open to 'minimum' likely open state immediately if less open currently.
      if(valvePCOpen < minOpenFromCold) { return(minOpenFromCold); }
      // Slew open relatively gently...
      return(OTV0P2BASE::fnmin((uint8_t)(valvePCOpen + slewRate), inputState.maxPCOpen)); // Capped at maximum.
      }
    // Keep open at maximum allowed.
    return(inputState.maxPCOpen);
    }

  // (Well) over temp target: close valve down.
  if(adjustedTempC > inputState.targetTempC)
    {
//V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("over temp");

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
      // TODO-593: if user is manually adjusting device then attempt to respond quickly.
      if(((!inputState.hasEcoBias) || justOverTemp || isFiltering) &&
         (!inputState.fastResponseRequired) &&
         (valvePCOpen > OTV0P2BASE::fnconstrain((uint8_t)(lingerThreshold + TRV_SLEW_PC_PER_MIN_FAST), (uint8_t)TRV_SLEW_PC_PER_MIN_FAST, inputState.maxPCOpen)))
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
  // Bigger lsbits value means closer to target from below, so closer to valve off.
  const uint8_t lsbits = (uint8_t) (adjustedTempC16 & 0xf); // LSbits of temperature above base of proportional adjustment range.
//    uint8_t tmp = (uint8_t) (refTempC16 & 0xf); // Only interested in lsbits.
  const uint8_t tmp = 16 - lsbits; // Now in range 1 (at warmest end of 'correct' temperature) to 16 (coolest).
  const uint8_t ulpStep = 6;
  // Get to nominal range 6 to 96, eg valve nearly shut just below top of 'correct' temperature window.
  const uint8_t targetPORaw = tmp * ulpStep;
  // Constrain from below to likely minimum-open value, in part to deal with TODO-117 'linger open' in lieu of boiler bypass.
  // Constrain from above by maximum percentage open allowed, eg for pay-by-volume systems.
  const uint8_t targetPO = OTV0P2BASE::fnconstrain(targetPORaw, inputState.minPCOpen, inputState.maxPCOpen);

  // Reduce spurious valve/boiler adjustment by avoiding movement at all unless current temperature error is significant.
  if(targetPO != valvePCOpen)
    {
    // True iff valve needs to be closed somewhat.
    const bool tooOpen = (targetPO < valvePCOpen);
    // Compute the minimum/epsilon slew adjustment allowed (the deadband).
    // Also increase effective deadband if temperature resolution is lower than 1/16th, eg 8ths => 1+2*ulpStep minimum.
// FIXME //    const uint8_t realMinUlp = 1 + (inputState.isLowPrecision ? 2*ulpStep : ulpStep); // Assume precision no coarser than 1/8C.
    // Compute real minimum unit in last place.
    constexpr uint8_t realMinUlp = 1 + ulpStep;
    // Compute minimum slew to use with a wider deadband.
    const uint8_t ls = OTV0P2BASE::fnmax(TRV_MAX_SLEW_PC_PER_MIN, (uint8_t) (2 * TRV_MIN_SLEW_PC));
    const uint8_t ls2 = OTV0P2BASE::fnmin(ls, (uint8_t) (OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN / 2));
    const uint8_t ls3 = OTV0P2BASE::fnmax(ls2, (uint8_t) (2 + TRV_MIN_SLEW_PC));
    const uint8_t _minAbsSlew = (uint8_t)(inputState.widenDeadband ? ls3 : TRV_MIN_SLEW_PC);
    const uint8_t minAbsSlew = OTV0P2BASE::fnmax(realMinUlp, _minAbsSlew);
    if(tooOpen) // Currently open more than required.  Still below target at top of proportional range.
      {
//V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("slightly too open");
      const uint8_t slew = valvePCOpen - targetPO;
      // Ensure no hunting for ~1ulp temperature wobble.
      if(slew < minAbsSlew) { return(valvePCOpen); }

      // Reduce valve hunting: defer re-closing if recently opened.
      if(dontTurndown()) { return(valvePCOpen); }

      // TODO-453: avoid closing the valve at all when the (raw) temperature is not rising, so as to minimise valve movement.
      // Since the target is the top of the proportional range than nothing within it requires the temperature to be *forced* down.
      // Possibly don't apply this rule at the very top of the range in case filtering is on and the filtered value moves differently to the raw.
      const int rise = getRawDelta();
      if(rise < 0) { return(valvePCOpen); }
      if((0 == rise) && inputState.widenDeadband) { return(valvePCOpen); }

      // TODO-1026: minimise movement in dark to avoid disturbing sleep (darkness indicated with wide deadband).
      if(inputState.widenDeadband && lsbits < 14) { return(valvePCOpen); }

      // Close glacially if explicitly requested or if temperature undershoot has happened or is a danger.
      // Also be glacial if in soft setback which aims to allow temperatures to drift passively down a little.
      //   (TODO-451, TODO-467: have darkness only immediately trigger a 'soft setback' using wide deadband)
      // This assumes that most valves more than about 1/3rd open can deliver significant power, esp if not statically balanced.
      // TODO-482: try to deal better with jittery temperature readings.
      const bool beGlacial = inputState.glacial ||
//      #if defined(GLACIAL_ON_WITH_WIDE_DEADBAND)
          // (GLACIAL_ON_WITH_WIDE_DEADBAND: TODO-467)
          ((inputState.widenDeadband || isFiltering) && (valvePCOpen <= OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN)) ||
//      #endif
          (lsbits < 8);
      if(beGlacial) { return(valvePCOpen - 1); }

      if(slew > TRV_SLEW_PC_PER_MIN_FAST)
          { return(valvePCOpen - TRV_SLEW_PC_PER_MIN_FAST); } // Cap slew rate.
      // Adjust directly to target.
      return(targetPO);
      }

    // if(targetPO > TRVPercentOpen) // Currently open less than required.  Still below target at top of proportional range.
//V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("slightly too closed");
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
    // while minimising valve movement/noise/etc is a good goal,
    // so if raw temperatures are rising (oer steady) at the moment then leave the valve as-is.
    // If fairly near the final target then also leave the valve as-is (TODO-453 & TODO-451).
    // TODO-1026: minimise movement in dark to avoid disturbing sleep (dark indicated with wide deadband).
    // DHD20161020: reduced lower threshold with wide deadband from 8 to 2 (cf 12 without).
    const int rise = getRawDelta();
    if(rise > 0) { return(valvePCOpen); }
    if((0 == rise) && inputState.widenDeadband) { return(valvePCOpen); }
    if((lsbits >= (inputState.widenDeadband ? 2 : 12))) { return(valvePCOpen); }

    // Open glacially if explicitly requested or if temperature overshoot has happened or is a danger.
    // Also be glacial if in soft setback which aims to allow temperatures to drift passively down a little.
    //   (TODO-451, TODO-467: have darkness only immediately trigger a 'soft setback' using wide deadband)
    // This assumes that most valves more than about 1/3rd open can deliver significant power, esp if not statically balanced.
    const bool beGlacial = inputState.glacial ||
//      #if defined(GLACIAL_ON_WITH_WIDE_DEADBAND)
        // (GLACIAL_ON_WITH_WIDE_DEADBAND: TODO-467)
        inputState.widenDeadband ||
//      #endif
        (lsbits >= 8) || ((lsbits >= 4) && (valvePCOpen > OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN));
    if(beGlacial) { return(valvePCOpen + 1); }

    // Slew open faster with comfort bias.  (Or with explicit request? inputState.fastResponseRequired TODO-593)
    const uint8_t maxSlew = (!inputState.hasEcoBias) ? TRV_SLEW_PC_PER_MIN_FAST : TRV_MAX_SLEW_PC_PER_MIN;
    if(slew > maxSlew)
        { return(valvePCOpen + maxSlew); } // Cap slew rate towards open.
    // Adjust directly to target.
    return(targetPO);
    }

  // Leave value position as was...
  return(valvePCOpen);
  }



#ifdef ModelledRadValve_DEFINED

// Return minimum valve percentage open to be considered actually/significantly open; [1,100].
// At the boiler hub this is also the threshold percentage-open on eavesdropped requests that will call for heat.
// If no override is set then OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN is used.
uint8_t ModelledRadValve::getMinValvePcReallyOpen() const
  {
#ifdef ARDUINO_ARCH_AVR
  const uint8_t stored = eeprom_read_byte((uint8_t *)V0P2BASE_EE_START_MIN_VALVE_PC_REALLY_OPEN);
  const uint8_t result = ((stored > 0) && (stored <= 100)) ? stored : OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN;
  return(result);
#else
  return(OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN);
#endif // ARDUINO_ARCH_AVR
  }

// Set and cache minimum valve percentage open to be considered really open.
// Applies to local valve and, at hub, to calls for remote calls for heat.
// Any out-of-range value (eg >100) clears the override and OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN will be used.
void ModelledRadValve::setMinValvePcReallyOpen(const uint8_t percent)
  {
#ifdef ARDUINO_ARCH_AVR
  if((percent > 100) || (percent == 0) || (percent == OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN))
    {
    // Bad / out-of-range / default value so erase stored value if not already erased.
    OTV0P2BASE::eeprom_smart_erase_byte((uint8_t *)V0P2BASE_EE_START_MIN_VALVE_PC_REALLY_OPEN);
    return;
    }
  // Store specified value with as low wear as possible.
  OTV0P2BASE::eeprom_smart_update_byte((uint8_t *)V0P2BASE_EE_START_MIN_VALVE_PC_REALLY_OPEN, percent);
#endif // ARDUINO_ARCH_AVR
  }

// True if the controlled physical valve is thought to be at least partially open right now.
// If multiple valves are controlled then is this true only if all are at least partially open.
// Used to help avoid running boiler pump against closed valves.
// The default is to use the check the current computed position
// against the minimum open percentage.
bool ModelledRadValve::isControlledValveReallyOpen() const
  {
//  if(isRecalibrating()) { return(false); }
  if(NULL != physicalDeviceOpt) { if(!physicalDeviceOpt->isControlledValveReallyOpen()) { return(false); } }
  return(value >= getMinPercentOpen());
  }

// Compute target temperature and set heat demand for TRV and boiler; update state.
// CALL REGULARLY APPROXIMATELY ONCE PER MINUTE TO ALLOW SIMPLE TIME-BASED CONTROLS.
// Inputs are inWarmMode(), isRoomLit().
//
// This routine may take significant CPU time.
//
// Internal state is updated, and the target updated on any attached physical valve.
//
// Will clear any BAKE mode if the newly-computed target temperature is already exceeded.
void ModelledRadValve::computeCallForHeat()
  {
  valveModeRW->read();
  // Compute target temperature and ensure that required input state is set for computeRequiredTRVPercentOpen().
  computeTargetTemperature();
  retainedState.tick(value, inputState);
  if(NULL != physicalDeviceOpt) { physicalDeviceOpt->set(value); }
  }

// Compute/update target temperature and set up state for computeRequiredTRVPercentOpen().
// Can be called as often as required though may be slowish/expensive.
// Can be called after any UI/CLI/etc operation
// that may cause the target temperature to change.
// (Will also be called by computeCallForHeat().)
// One aim is to allow reasonable energy savings (10--30%)
// even if the device is left in WARM mode all the time,
// using occupancy/light/etc to determine when temperature can be set back
// without annoying users.
//
// Will clear any BAKE mode if the newly-computed target temperature is already exceeded.
void ModelledRadValve::computeTargetTemperature()
  {
  // Compute basic target temperature statelessly.
  const uint8_t newTarget = ctt->computeTargetTemp();

  // Make new target available.
  value = newTarget;

  // Set up state for computeRequiredTRVPercentOpen().
  ctt->setupInputState(inputState,
      retainedState.isFiltering,
      newTarget, getMinPercentOpen(), getMaxPercentageOpenAllowed(), glacial);

  // Explicitly compute the actual setback when in WARM mode for monitoring purposes.
  // TODO: also consider showing full setback to FROST when a schedule is set but not on.
  // By default, the setback is regarded as zero/off.
  setbackC = 0;
  if(valveModeRW->inWarmMode())
    {
    const uint8_t wt = tempControl->getWARMTargetC();
    if(newTarget < wt) { setbackC = wt - newTarget; }
    }

  // True if the target temperature has not been met.
  const bool targetNotReached = (newTarget >= (inputState.refTempC16 >> 4));
  underTarget = targetNotReached;
  // If the target temperature is already reached then cancel any BAKE mode in progress (TODO-648).
  if(!targetNotReached) { valveModeRW->cancelBakeDebounced(); }
  // Only report as calling for heat when actively doing so.
  // (Eg opening the valve a little in case the boiler is already running does not count.)
  callingForHeat = targetNotReached &&
    (value >= OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN) &&
    isControlledValveReallyOpen();
  }

#endif // ModelledRadValve_DEFINED


    }


//// Median filter.
//// Find mean of interquartile range of group of ints where sum can be computed in an int without loss.
//// FIXME: needs a unit test or three.
//template<uint8_t N> int smallIntIQMean(const int data[N])
//  {
//  // Copy array content.
//  int copy[N];
//  for(int8_t i = N; --i >= 0; ) { copy[i] = data[i]; }
//  // Sort in place with a bubble sort (yeuck) assuming the array to be small.
//  // FIXME: replace with insertion sort for efficiency.
//  // FIXME: break out sort as separate subroutine.
//  uint8_t n = N;
//  do
//    {
//    uint8_t newn = 0;
//    for(uint8_t i = 0; ++i < n; )
//      {
//      const int c0 = copy[i-1];
//      const int c1 = copy[i];
//      if(c0 > c1)
//         {
//         copy[i] = c0;
//         copy[i-1] = c1;
//         newn = i;
//         }
//      }
//    n = newn;
//    } while(0 != n);
//#if 0 && defined(DEBUG)
//DEBUG_SERIAL_PRINT_FLASHSTRING("sorted: ");
//for(uint8_t i = 0; i < N; ++i) { DEBUG_SERIAL_PRINT(copy[i]); DEBUG_SERIAL_PRINT(' '); }
//DEBUG_SERIAL_PRINTLN();
//#endif
//  // Extract mean of interquartile range.
//  const size_t sampleSize = N/2;
//  const size_t start = N/4;
//  // Assume values will be nowhere near the extremes.
//  int sum = 0;
//  for(uint8_t i = start; i < start + sampleSize; ++i) { sum += copy[i]; }
//  // Compute rounded-up mean.
//  return((sum + sampleSize/2) / sampleSize);
//  }
