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
#include "OTRadValve_AbstractRadValve.h"
#include "OTRadValve_ValveMode.h"
#include "OTRadValve_TempControl.h"
#include "OTRadValve_ActuatorPhysicalUI.h"


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


// All input state for computing valve movement.
// Exposed to allow easier unit testing.
//
// This uses int_fast16_t for C16 temperatures (ie Celsius * 16)
// to be able to efficiently process signed values with sufficient range for room temperatures.
//
// All initial values set by the constructors are sane, but should not be relied on.
struct ModelledRadValveInputState final
  {
  // Offset from raw temperature to get reference temperature in C/16.
  static constexpr int_fast8_t refTempOffsetC16 = 8;

  // All initial values set by the constructor are sane, but should not be relied on.
  ModelledRadValveInputState(const int_fast16_t realTempC16 = 0) { setReferenceTemperatures(realTempC16); }

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
  uint8_t targetTempC = DEFAULT_ValveControlParameters::FROST; // Start with a safe/sensible value.
  // Min % valve at which is considered to be actually open (allow the room to heat) [1,100].
  uint8_t minPCOpen = OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN;
  // Max % valve is allowed to be open [1,100].
  uint8_t maxPCOpen = 100;

  // If true then allow a wider deadband (more temperature drift) to save energy and valve noise.
  // This is a strong hint that the system can work less strenuously to reach or stay on, target,
  // and/or that the user has not manually requested an adjustment recently
  // so this need not be ultra responsive.
  bool widenDeadband = false;
  // True if in glacial mode.
  bool glacial = false;
  // True if an eco bias is to be applied.
  bool hasEcoBias = false;
  // True if in BAKE mode.
  bool inBakeMode = false;
  // User just adjusted controls or other fast response needed.
  // (Should not be true at same time as widenDeadband.)
  // Indicates manual operation/override, so speedy response required (TODO-593).
  bool fastResponseRequired = false;

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
struct ModelledRadValveState final
  {
  // Construct an instance, with sensible defaults, but no (room) temperature.
  // Defers its initialisation with room temperature until first tick().
  ModelledRadValveState(bool _alwaysGlacial = false) :
    alwaysGlacial(_alwaysGlacial),
    initialised(false),
    isFiltering(false),
    valveMoved(false),
    cumulativeMovementPC(0),
    valveTurndownCountdownM(0), valveTurnupCountdownM(0)
    { }

  // Construct an instance, with sensible defaults, and current (room) temperature from the input state.
  // Does its initialisation with room temperature immediately.
  ModelledRadValveState(const ModelledRadValveInputState &inputState, bool _defaultGlacial = false);

  // Perform per-minute tasks such as counter and filter updates then recompute valve position.
  // The input state must be complete including target and reference temperatures
  // before calling this including the first time whereupon some further lazy initialisation is done.
  //   * valvePCOpenRef  current valve position UPDATED BY THIS ROUTINE, in range [0,100]
  void tick(volatile uint8_t &valvePCOpenRef, const ModelledRadValveInputState &inputState);

  // True if by default/always in glacial mode, eg to minimise flow and overshoot.
  const bool alwaysGlacial;

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
  uint8_t computeRequiredTRVPercentOpen(uint8_t currentValvePCOpen, const ModelledRadValveInputState &inputState) const;
  };

// Sensor, control and stats inputs for computations.
// Read access to all necessary underlying devices.
struct ModelledRadValveSensorCtrlStats final
    {
    // Read-only access to user-selected valve mode; must not be NULL.
    const ValveMode *const valveMode;

    // Read-only access to ambient (eg room) temperature in C<<4; must not be NULL.
    const OTV0P2BASE::TemperatureC16Base *const temperatureC16;

    // Read-only access to temperature control for set-points; must not be NULL.
    const TempControlBase *const tempControl;

    // Read-only access to occupancy tracker; must not be NULL.
    const OTV0P2BASE::PseudoSensorOccupancyTracker *const occupancy;

    // Read-only access to ambient light sensor; must not be NULL.
    const OTV0P2BASE::SensorAmbientLightBase *const ambLight;

    // Read-only access to physical UI; must not be NULL.
    const ActuatorPhysicalUIBase *const physicalUI;

    // Read-only access to simple schedule; must not be NULL.
    const OTV0P2BASE::SimpleValveScheduleBase *const schedule;

    // Read-only access to by-hour stats; must not be NULL.
    const OTV0P2BASE::NVByHourByteStatsBase *const byHourStats;

    // Construct an instance wrapping read-only access to all input devices.
    // All arguments must be non-NULL.
    ModelledRadValveSensorCtrlStats(
        const ValveMode *const _valveMode,
        const OTV0P2BASE::TemperatureC16Base *const _temperatureC16,
        const TempControlBase *const _tempControl,
        const OTV0P2BASE::PseudoSensorOccupancyTracker *const _occupancy,
        const OTV0P2BASE::SensorAmbientLightBase *const _ambLight,
        const ActuatorPhysicalUIBase *const _physicalUI,
        const OTV0P2BASE::SimpleValveScheduleBase *const _schedule,
        const OTV0P2BASE::NVByHourByteStatsBase *const _byHourStats)
       : valveMode(_valveMode),
         temperatureC16(_temperatureC16),
         tempControl(_tempControl),
         occupancy(_occupancy),
         ambLight(_ambLight),
         physicalUI(_physicalUI),
         schedule(_schedule),
         byHourStats(_byHourStats)
         { }
    };

// Base class for stateless computation of target temperature.
// Implementations will capture parameters and sensor references, etc.
class ModelledRadValveComputeTargetTempBase
  {
  public:
    // Compute and return target (usually room) temperature (stateless).
    // Computes the target temperature based on various sensors, controls and stats.
    // Can be called as often as required though may be slow/expensive.
    // Will be called by computeTargetTemperature().
    // One aim is to allow reasonable energy savings (10--30%+)
    // even if the device is left in WARM mode all the time,
    // using occupancy/light/etc to determine when temperature can be set back
    // without annoying users.
    //
    // Attempts in WARM mode to make the deepest reasonable cuts to maximise savings
    // when the room is vacant and not likely to become occupied again soon,
    // ie this looks ahead to give the room time to recover to target before occupancy.
    //
    // Stateless directly-testable version behind computeTargetTemperature().
    virtual uint8_t computeTargetTemp() const = 0;

    // Set all fields of inputState from the target temperature and other args, and the sensor/control inputs.
    // The target temperature will usually have just been computed by computeTargetTemp().
    virtual void setupInputState(ModelledRadValveInputState &inputState,
        const bool isFiltering,
        const uint8_t newTarget, const uint8_t minPCOpen, const uint8_t maxPCOpen, const bool glacial) const = 0;
  };

// Basic stateless implementation of computation of target temperature.
// Templated with all the input instances for maximum speed and minimum code size.
template<
  class valveControlParameters,
  const ValveMode *const valveMode,
  class TemperatureC16Base,             const TemperatureC16Base *const temperatureC16,
  class TempControlBase,                const TempControlBase *const tempControl,
  class PseudoSensorOccupancyTracker,   const PseudoSensorOccupancyTracker *const occupancy,
  class SensorAmbientLightBase,         const SensorAmbientLightBase *const ambLight,
  class ActuatorPhysicalUIBase,         const ActuatorPhysicalUIBase *const physicalUI,
  class SimpleValveScheduleBase,        const SimpleValveScheduleBase *const schedule,
  class NVByHourByteStatsBase,          const NVByHourByteStatsBase *const byHourStats,
  bool (*const setbackLockout)() = ((bool(*)())NULL)
  >
class ModelledRadValveComputeTargetTempBasic final : public ModelledRadValveComputeTargetTempBase
  {
  public:
//    ModelledRadValveComputeTargetTempBasic()
//      {
//      // TODO validate arg types and that things aren't NULL.  static_assert()?
//      }
    virtual uint8_t computeTargetTemp() const override
        {
        // In FROST mode.
        if(!valveMode->inWarmMode())
          {
          const uint8_t frostC = tempControl->getFROSTTargetC();

          // If scheduled WARM is due soon then ensure that room is at least at setback temperature
          // to give room a chance to hit the target, and for furniture and surfaces to be warm, etc, on time.
          // Don't do this if the room has been vacant for a long time (eg so as to avoid pre-warm being higher than WARM ever).
          // Don't do this if there has been recent manual intervention, eg to allow manual 'cancellation' of pre-heat (TODO-464).
          // Only do this if the target WARM temperature is NOT an 'eco' temperature (ie very near the bottom of the scale).
          // If well into the 'eco' zone go for a larger-than-usual setback, else go for usual small setback.
          // Note: when pre-warm and warm time for schedule is ~1.5h, and default setback 1C,
          // this is assuming that the room temperature can be raised by ~1C/h.
          // See the effect of going from 2C to 1C setback: http://www.earth.org.uk/img/20160110-vat-b.png
          // (A very long pre-warm time may confuse or distress users, eg waking them in the morning.)
          if(!occupancy->longVacant() && schedule->isAnyScheduleOnWARMSoon() && !physicalUI->recentUIControlUse())
            {
            const uint8_t warmTarget = tempControl->getWARMTargetC();
            // Compute putative pre-warm temperature, usually only just below WARM target.
            const uint8_t preWarmTempC = OTV0P2BASE::fnmax((uint8_t)(warmTarget - (tempControl->isEcoTemperature(warmTarget) ? valveControlParameters::SETBACK_ECO : valveControlParameters::SETBACK_DEFAULT)), frostC);
            if(frostC < preWarmTempC) // && (!isEcoTemperature(warmTarget)))
              { return(preWarmTempC); }
            }

          // Apply FROST safety target temperature by default in FROST mode.
          return(frostC);
          }

        else if(valveMode->inBakeMode()) // If in BAKE mode then use elevated target.
          {
          return(OTV0P2BASE::fnmin((uint8_t)(tempControl->getWARMTargetC() + valveControlParameters::BAKE_UPLIFT), OTRadValve::MAX_TARGET_C)); // No setbacks apply in BAKE mode.
          }

        else // In 'WARM' mode with possible setback.
          {
          const uint8_t wt = tempControl->getWARMTargetC();

          // If smart setbacks are locked out then return WARM temperature as-is.  (TODO-786, TODO-906)
          if((NULL != setbackLockout) && (setbackLockout)())
            { return(wt); }

          // Set back target the temperature a little if the room seems to have been vacant for a long time (TODO-107)
          // or it is too dark for anyone to be active or the room is not likely occupied at this time
          // or the room was apparently not occupied at thus time yesterday (and is not now).
          //   AND no WARM schedule is active now (TODO-111)
          //   AND no recent manual interaction with the unit's local UI (TODO-464) indicating local settings override.
          // The notion of "not likely occupied" is "not now"
          // AND less likely than not at this hour of the day AND an hour ahead (TODO-758).
          // Note that this mainly has to work in domestic settings in winter (with ~8h of daylight)
          // but should ideally also work in artificially-lit offices (maybe ~12h continuous lighting).
          // No 'lights-on' signal for a whole day is a fairly strong indication that the heat can be turned down.
          // TODO-451: TODO-453: ignore a short lights-off, eg from someone briefly leaving room or a transient shadow.
          // TODO: consider bottom quartile of ambient light as alternative setback trigger for near-continuously-lit spaces (aiming to spot daylight signature).
          // Look ahead to next time period (as well as current) to determine notLikelyOccupiedSoon
          // but suppress lookahead of occupancy when its been dark for many hours (eg overnight) to avoid disturbing/waking.  (TODO-792)
          // Note that deeper setbacks likely offer more savings than faster (but shallower) setbacks.
          const bool longLongVacant = occupancy->longLongVacant();
          const bool longVacant = longLongVacant || occupancy->longVacant();
          const bool likelyVacantNow = longVacant || occupancy->isLikelyUnoccupied();
          const bool ecoBias = tempControl->hasEcoBias();
          // True if the room has been dark long enough to indicate night. (TODO-792)
          const uint8_t dm = ambLight->getDarkMinutes();
          const bool darkForHours = dm > 245; // A little over 4h, not quite max 255.
          // Be more ready to decide room not likely occupied soon if eco-biased.
          // Note that this value is likely to be used +/- 1 so must be in range [1,23].
          const uint8_t thisHourNLOThreshold = ecoBias ? 15 : 12;
          const uint8_t hoursLessOccupiedThanThis = byHourStats->countStatSamplesBelow(V0P2BASE_EE_STATS_SET_OCCPC_BY_HOUR_SMOOTHED, byHourStats->getByHourStat(V0P2BASE_EE_STATS_SET_OCCPC_BY_HOUR_SMOOTHED, OTV0P2BASE::STATS_SPECIAL_HOUR_CURRENT_HOUR));
          const uint8_t hoursLessOccupiedThanNext = byHourStats->countStatSamplesBelow(V0P2BASE_EE_STATS_SET_OCCPC_BY_HOUR_SMOOTHED, byHourStats->getByHourStat(V0P2BASE_EE_STATS_SET_OCCPC_BY_HOUR_SMOOTHED, OTV0P2BASE::STATS_SPECIAL_HOUR_NEXT_HOUR));
          const bool notLikelyOccupiedSoon = longLongVacant ||
              (likelyVacantNow &&
              // No more than about half the hours to be less occupied than this hour to be considered unlikely to be occupied.
              (hoursLessOccupiedThanThis < thisHourNLOThreshold) &&
              // Allow to be a little bit more occupied for the next hour than the current hour.
              // Suppress occupancy lookahead if room has been dark for several hours, eg overnight.  (TODO-792)
              (darkForHours || (hoursLessOccupiedThanNext < (thisHourNLOThreshold+1))));
          const uint8_t minLightsOffForSetbackMins = ecoBias ? 10 : 20;
          if(longVacant ||
             ((notLikelyOccupiedSoon || (dm > minLightsOffForSetbackMins) || (ecoBias && (occupancy->getVacancyH() > 0) && (0 == byHourStats->getByHourStat(V0P2BASE_EE_STATS_SET_OCCPC_BY_HOUR, OTV0P2BASE::STATS_SPECIAL_HOUR_CURRENT_HOUR)))) &&
                 !schedule->isAnyScheduleOnWARMNow() && !physicalUI->recentUIControlUse()))
            {
            // Use a default minimal non-annoying setback if:
            //   in upper part of comfort range
            //   or if the room is likely occupied now
            //   or if the room is not known to be dark and hasn't been vacant for a long time ie ~1d and not in the very bottom range occupancy (TODO-107, TODO-758)
            //      TODO POSSIBLY: limit to (say) 3--4h light time for when someone out but room daylit, but note that detecting occupancy will be harder too in daylight.
            //      TODO POSSIBLY: after ~3h vacancy AND apparent smoothed occupancy non-zero (so some can be detected) AND ambient light in top quartile or in middle of typical bright part of cycle (assume peak of daylight) then being lit is not enough to prevent a deeper setback.
            //   or is fairly likely to be occupied in the next hour (to pre-warm) and the room hasn't been dark for hours and vacant for a long time
            //   or if a scheduled WARM period is due soon and the room hasn't been vacant for a long time,
            // else usually use a somewhat bigger 'eco' setback
            // else use an even bigger 'full' setback for maximum savings if in the eco region and
            //   the room has been vacant for a very long time
            //   or is unlikely to be unoccupied at this time of day and
            //     has been vacant and dark for a while or is in the lower part of the 'eco' range.
            // This final dark/vacant timeout to enter FULL fallback while in mild eco mode
            // should probably be longer than required to watch a typical movie or go to sleep (~2h) for example,
            // but short enough to take effect overnight and to be in effect a reasonable fraction of a (~8h) night.
            const uint8_t minVacantAndDarkForFULLSetbackH = 2; // Hours; strictly positive, typically 1--4.
            const uint8_t setback = (tempControl->isComfortTemperature(wt) ||
                                     occupancy->isLikelyOccupied() ||
                                     (!longVacant && !ambLight->isRoomDark() && (hoursLessOccupiedThanThis > 4)) ||
                                     (!longVacant && !darkForHours && (hoursLessOccupiedThanNext >= thisHourNLOThreshold-1)) ||
                                     (!longVacant && schedule->isAnyScheduleOnWARMSoon())) ?
                    valveControlParameters::SETBACK_DEFAULT :
                ((ecoBias && (longLongVacant ||
                    (notLikelyOccupiedSoon && (tempControl->isEcoTemperature(wt) ||
                        ((dm > (uint8_t)OTV0P2BASE::fnmin(254, 60*minVacantAndDarkForFULLSetbackH)) && (occupancy->getVacancyH() >= minVacantAndDarkForFULLSetbackH)))))) ?
                    valveControlParameters::SETBACK_FULL : valveControlParameters::SETBACK_ECO);

            // Target must never be set low enough to create a frost/freeze hazard.
            const uint8_t newTarget = OTV0P2BASE::fnmax((uint8_t)(wt - setback), tempControl->getFROSTTargetC());

            return(newTarget);
            }
          // Else use WARM target as-is.
          return(wt);
          }
        }

    // Set all fields of inputState from the target temperature and other args, and the sensor/control inputs.
    // The target temperature will usually have just been computed by computeTargetTemp().
    virtual void setupInputState(ModelledRadValveInputState &inputState,
        const bool isFiltering,
        const uint8_t newTarget, const uint8_t minPCOpen, const uint8_t maxPCOpen, const bool glacial) const override
        {
        // Set up state for computeRequiredTRVPercentOpen().
        inputState.targetTempC = newTarget;
        inputState.minPCOpen = minPCOpen;
        inputState.maxPCOpen = maxPCOpen;
        inputState.glacial = glacial;
        inputState.inBakeMode = valveMode->inBakeMode();
        inputState.hasEcoBias =
                tempControl->hasEcoBias();
        // Request a fast response from the valve if user is manually adjusting controls.
        const bool veryRecentUIUse =
                physicalUI->veryRecentUIControlUse();
        inputState.fastResponseRequired = veryRecentUIUse;
        // Widen the allowed deadband significantly in an unlit/quiet/vacant room (TODO-383, TODO-593, TODO-786, TODO-1037)
        // (or in FROST mode, or if temperature is jittery eg changing fast and filtering has been engaged)
        // to attempt to reduce the total number and size of adjustments and thus reduce noise/disturbance (and battery drain).
        // The wider deadband (less good temperature regulation) might be noticeable/annoying to sensitive occupants.
        // With a wider deadband may also simply suppress any movement/noise on some/most minutes while close to target temperature.
        // For responsiveness, don't widen the deadband immediately after manual controls have been used (TODO-593).
        //
        // Minimum number of hours vacant to force wider deadband in ECO mode, else a full day ('long vacant') is the threshold.
        // May still have to back this off if only automatic occupancy input is ambient light and day >> 6h, ie other than deep winter.
        constexpr uint8_t minVacancyHoursForWideningECO = 3;
        inputState.widenDeadband = (!veryRecentUIUse)
            && (isFiltering
                    || (!valveMode->inWarmMode())
                    || ambLight->isRoomDark() // Must be false if light sensor not usable.
                    || occupancy->longVacant()
                    || (tempControl->hasEcoBias()
                            && (occupancy->getVacancyH() >= minVacancyHoursForWideningECO)));
        // Capture adjusted reference/room temperatures
        // and set callingForHeat flag also using same outline logic as computeRequiredTRVPercentOpen() will use.
        inputState.setReferenceTemperatures(temperatureC16->get());
        }
  };

// Internal model of radiator valve position, embodying control logic.
#define ModelledRadValve_DEFINED
class ModelledRadValve final : public AbstractRadValve
  {
  private:
    // Target temperature computation.
    const ModelledRadValveComputeTargetTempBase *ctt;

    // All input state for deciding where to set the radiator valve in normal operation.
    struct ModelledRadValveInputState inputState;
    // All retained state for deciding where to set the radiator valve in normal operation.
    struct ModelledRadValveState retainedState;

    // Read-only access to temperature control; never NULL.
    const TempControlBase *const tempControl;

    // True if this node is calling for heat.
    // Marked volatile for thread-safe lock-free access.
    volatile bool callingForHeat = false;

    // True if the room/ambient temperature is below target, enough to likely call for heat.
    // Marked volatile for thread-safe lock-free access.
    volatile bool underTarget = false;

    // The current automated setback (if any) in the direction of energy saving in C; non-negative.
    // Not intended for ISR/threaded access.
    uint8_t setbackC = 0;

    // True if in glacial mode.
    bool glacial;

    // Maximum percentage valve is allowed to be open [0,100].
    // Usually 100, but special circumstances may require otherwise.
    const uint8_t maxPCOpen;

    // Compute target temperature and set heat demand for TRV and boiler; update state.
    // CALL REGULARLY APPROXIMATELY ONCE PER MINUTE TO ALLOW SIMPLE TIME-BASED CONTROLS.
    // Inputs are inWarmMode(), isRoomLit().
    // This routine may take significant CPU time; no I/O is done, only internal state is updated.
    //
    // Will clear any BAKE mode if the newly-computed target temperature is already exceeded.
    void computeCallForHeat();

    // Read/write (non-const) access to valveMode instance; never NULL.
    ValveMode *const valveModeRW;

    // Read/write access the underlying physical device; NULL if none.
    AbstractRadValve *const physicalDeviceOpt;

  public:
    // Create an instance.
    ModelledRadValve(
        const ModelledRadValveComputeTargetTempBase *const _ctt,
        ValveMode *const _valveMode,
        const TempControlBase *const _tempControl,
        AbstractRadValve *const _physicalDeviceOpt,
        const bool _defaultGlacial = false, const uint8_t _maxPCOpen = 100)
      : ctt(_ctt),
        retainedState(_defaultGlacial),
        tempControl(_tempControl),
        glacial(_defaultGlacial),
        maxPCOpen(OTV0P2BASE::fnmin(_maxPCOpen, (uint8_t)100U)),
        valveModeRW(_valveMode),
        physicalDeviceOpt(_physicalDeviceOpt)
      { }

    // Force a read/poll/recomputation of the target position and call for heat.
    // Sets/clears changed flag if computed valve position changed.
    // Pushes target to physical device if configured.
    // Call at a fixed rate (1/60s).
    // Potentially expensive/slow.
    virtual uint8_t read() override { computeCallForHeat(); return(value); }

    // Returns preferred poll interval (in seconds); non-zero.
    // Must be polled at near constant rate, about once per minute.
    virtual uint8_t preferredPollInterval_s() const override { return(60); }

    // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
    // The lifetime of the pointed-to text must be at least that of the Sensor instance.
    virtual const char *tag() const override { return("v|%"); }

    // Returns true iff not in error state and not (re)calibrating/(re)initialising/(re)syncing.
    // By default there is no recalibration step.
    // Passes through to underlying physical valve if configured, else true.
    virtual bool isInNormalRunState() const override { return((NULL == physicalDeviceOpt) ? true : physicalDeviceOpt->isInNormalRunState()); }

    // Returns true if in an error state.
    // May be recoverable by forcing recalibration.
    // Passes through to underlying physical valve if configured, else false.
    virtual bool isInErrorState() const override { return((NULL == physicalDeviceOpt) ? false : physicalDeviceOpt->isInErrorState()); }

    // True if the controlled physical valve is thought to be at least partially open right now.
    // If multiple valves are controlled then is this should be true only if all are at least partially open.
    // Used to help avoid running boiler pump against closed valves.
    // The default is to use the check the current computed position
    // against the minimum open percentage.
    // True iff the valve(s) (if any) controlled by this unit are really open.
    //
    // When driving a remote wireless valve such as the FHT8V,
    // this should wait until at least the command has been sent.
    // This also implies open to OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN or equivalent.
    // Must be exactly one definition/implementation supplied at link time.
    // If more than one valve is being controlled by this unit,
    // then this should return true if all of the valves are (significantly) open.
    virtual bool isControlledValveReallyOpen() const override;

    // Get estimated minimum percentage open for significant flow [1,99] for this device.
    // Return global node value.
    virtual uint8_t getMinPercentOpen() const override { return(getMinValvePcReallyOpen()); }

    // Get maximum allowed percent open [1,100] to limit maximum flow rate.
    // This may be important for systems such as district heat systems that charge by flow,
    // and other systems that prefer return temperatures to be as low as possible,
    // such as condensing boilers.
    uint8_t getMaxPercentageOpenAllowed() const { return(maxPCOpen); }

    // Enable/disable 'glacial' mode (default false/off).
    // For heat-pump, district-heating and similar slow-response and pay-by-volume environments.
    // Also may help with over-powerful or unbalanced radiators
    // with a significant risk of overshoot.
    void setGlacialMode(bool glacialOn) { glacial = glacialOn; }

    // Returns true if this valve control is in glacial mode.
    bool inGlacialMode() const { return(glacial); }

    // True if the computed valve position was changed by read().
    // Can be used to trigger rebuild of messages, force updates to actuators, etc.
    bool isValveMoved() const { return(retainedState.valveMoved); }

    // True if this unit is actively calling for heat.
    // This implies that the temperature is (significantly) under target,
    // the valve is really open,
    // and this needs more heat than can be passively drawn from an already-running boiler.
    // Thread-safe and ISR safe.
    virtual bool isCallingForHeat() const override { return(callingForHeat); }

    // True if the room/ambient temperature is below target, enough to likely call for heat.
    // This implies that the temperature is (significantly) under target,
    // the valve is really open,
    // and this needs more heat than can be passively drawn from an already-running boiler.
    // Thread-safe and ISR safe.
    virtual bool isUnderTarget() const override { return(underTarget); }

    // Get target temperature in C as computed by computeTargetTemperature().
    uint8_t getTargetTempC() const { return(inputState.targetTempC); }

    // Returns a suggested (JSON) tag/field/key name including units of getTargetTempC(); not NULL.
    // The lifetime of the pointed-to text must be at least that of this instance.
    const char *tagTTC() const { return("tT|C"); }

    // Get the current automated setback (if any) in the direction of energy saving in C; non-negative.
    // For heating this is the number of C below the nominal user-set target temperature
    // that getTargetTempC() is; zero if no setback is in effect.
    // Generally will be 0 in FROST or BAKE modes.
    // Not ISR-/thread- safe.
    uint8_t getSetbackC() const { return(setbackC); }

    // Returns a suggested (JSON) tag/field/key name including units of getSetbackC(); not NULL.
    // It would often be appropriate to mark this as low priority since depth of setback matters more than speed.
    // The lifetime of the pointed-to text must be at least that of this instance.
    const char *tagTSC() const { return("tS|C"); }

    // Get cumulative valve movement %; rolls at 8192 in range [0,8191], ie non-negative.
    // It would often be appropriate to mark this as low priority since it can be computed from valve positions.
    uint16_t getCumulativeMovementPC() { return(retainedState.cumulativeMovementPC); }

    // Returns a suggested (JSON) tag/field/key name including units of getCumulativeMovementPC(); not NULL.
    // The lifetime of the pointed-to text must be at least that of this instance.
    const char *tagCMPC() const { return("vC|%"); }

    // Return minimum valve percentage open to be considered actually/significantly open; [1,100].
    // This is a value that has to mean all controlled valves are at least partially open if more than one valve.
    // At the boiler hub this is also the threshold percentage-open on eavesdropped requests that will call for heat.
    // If no override is set then OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN is used.
    uint8_t getMinValvePcReallyOpen() const;

    // Set and cache minimum valve percentage open to be considered really open.
    // Applies to local valve and, at hub, to calls for remote calls for heat.
    // Any out-of-range value (eg >100) clears the override and OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN will be used.
    void setMinValvePcReallyOpen(uint8_t percent);

    // Compute/update target temperature immediately.
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
    void computeTargetTemperature();

    // Pass through a wiggle request to the underlying device if specified.
    virtual void wiggle() const override { if(NULL != physicalDeviceOpt) { physicalDeviceOpt->wiggle(); } }
  };


    }

#endif
