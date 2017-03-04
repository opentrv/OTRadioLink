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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2017
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
// to be able to efficiently process signed values with sufficient range
// for room temperatures.
//
// All initial values set by the constructors are sane,
// but cannot be relied on to be sane for all uses.
struct ModelledRadValveInputState final
  {
  // Offset from raw temperature to get reference temperature in C/16.
  static constexpr uint8_t refTempOffsetC16 = 8;

  // All initial values set by the constructor are sane, for some uses.
  explicit ModelledRadValveInputState(const int_fast16_t realTempC16 = 0) { setReferenceTemperatures(realTempC16); }

  // Calculate and store reference temperature(s) from real temperature supplied.
  // Proportional temperature regulation is in a 1C band.
  // By default, for a given target XC the rad is off at (X+1)C
  // so that the controlled temperature oscillates around that point.
  // This routine shifts the reference point at which the rad is off to (X+0.5C)
  // ie to the middle of the specified degree, which is more intuitive,
  // and which may save a little energy if users focus on temperatures.
  // Suggestion c/o GG ~2014/10 code, and generally less misleading anyway!
  void setReferenceTemperatures(const int_fast16_t currentTempC16)
    {
    // Push targeted temperature down so that
    // the target is the middle of nominal set-point degree.  (TODO-386)
    const int_fast16_t referenceTempC16 = currentTempC16 + refTempOffsetC16;
    refTempC16 = referenceTempC16;
    }

  // Current target room temperature in C; in range [MIN_TARGET_C,MAX_TARGET_C].
  // Start with a safe/sensible value.
  uint8_t targetTempC = DEFAULT_ValveControlParameters::FROST;
  // Non-setback target in C; 0 if unused else in range [targetTempC,MAX_TARGET_C].
  // Used to provide a higher ceiling for temporary overshoots,
  // or at least for not needing to close the valve fully
  // if the temperature is not moving in the wrong direction
  // when setbacks have been applied, to reduce movement.  (TODO-1099)
  // If non-zero should not be lower than targetTempC
  // nor higher than targetTempC plus the maximum allowed setback.
  uint8_t maxTargetTempC = 0;

  // Min % valve at which is considered to be actually open (allow the room to heat) [1,100].
  // Placeholder for now.
  static constexpr uint8_t minPCReallyOpen = 1; // OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN;
  // Max % valve is allowed to be open [1,100].
  uint8_t maxPCOpen = 100;

  // If true then allow a wider deadband (more temperature drift)
  // to save energy and valve noise.
  // This is a strong hint that the system can work less strenuously
  // to reach or stay on, target,
  // and/or that the user has not manually requested an adjustment recently
  // so this need not be ultra responsive.
  bool widenDeadband = false;
  // True if in glacial mode.
  bool glacial = false;
  // True if an eco bias is to be applied.
  bool hasEcoBias = false;
  // True if in BAKE mode.
  bool inBakeMode = false;
  // User just adjusted controls or is expecting rapid feedback. (TODO-593)
  // (Should not be true at same time as widenDeadband.)
  bool fastResponseRequired = false;

  // Reference (room) temperature in C/16; must be set before each valve position recalc.
  // Proportional control is in the region where (refTempC16>>4) == targetTempC.
  // This is signed and at least 16 bits.
  int_fast16_t refTempC16;
  };


// All retained state for computing valve movement, eg time-based state.
// Exposed to allow easier unit testing.
// All initial values set by the constructor are sane.
//
// This uses int_fast16_t for C16 temperatures (ie Celsius * 16)
// to be able to efficiently process signed values with sufficient range
// for room temperatures.
struct ModelledRadValveState final
  {
  // If true then support a minimal/binary valve implementation.
  static constexpr bool MINIMAL_BINARY_IMPL = false;

  // FEATURE SUPPORT
  // If true then support proportional response in target 1C range.
  static constexpr bool SUPPORT_PROPORTIONAL = !MINIMAL_BINARY_IMPL;
  // If true then detect draughts from open windows and doors.
  static constexpr bool SUPPORT_MRVE_DRAUGHT = false;
  // If true then do lingering close to help boilers with poor bypass.
  static constexpr bool SUPPORT_LINGER = false;
  // If true then support filter minimum on-time (as isFiltering may be >1).
  static constexpr bool SUPPORT_LONG_FILTER = true;

  // Target minutes/ticks for full valve movement when fast response requested.
  static constexpr uint8_t fastResponseTicksTarget = 5;
  // Target minutes/ticks for full valve movement for very fast response.
  // Gives quick feedback and warming, eg in response to manual control use.
  static constexpr uint8_t vFastResponseTicksTarget = 3;

  // Proportional range wide enough to cope with all-in-one TRV overshoot.
  // Note that with the sensor near the heater an apparent overshoot
  // has to be tolerated to actually deliver heat to the room.
  // Within this range the device is always seeking for zero temperature error;
  // this is not a deadband.
  //
  // Primarily exposed to allow for whitebox unit testing; subject to change.
  // With 1/16C precision, a continuous drift in either direction
  // implies a delta T >= 60/16C ~ 4C per hour.
  static constexpr uint8_t _proportionalRange = 7;

  // Max jump between adjacent readings before forcing filtering; strictly +ve.
  // Too small a value may cap room rate rise to this per minute.
  // Too large a value may fail to sufficiently damp oscillations/overshoot.
  // Has to be at least as large as the minimum temperature sensor precision
  // to avoid false triggering of the filter.
  // Typical values range from 2
  // (for better-than 1/8C-precision temperature sensor) up to 4.
  static constexpr uint8_t MAX_TEMP_JUMP_C16 = 3; // 3/16C.
  // Min ticks for a 1C delta before forcing filtering; strictly +ve.
  // Too small a value may cap room rate rise to this per minute.
  // Too large a value may fail to sufficiently damp oscillations/overshoot.
  // A value of 10 would imply a maximum expected rise of 6C/h for example.
  static constexpr uint8_t MIN_TICKS_1C_DELTA = 10;
  // Min ticks for a 0.5C delta before forcing filtering; strictly +ve.
  // As the rise is well under 1C this may be useful
  // to avoid wandering too far from a target temperature.
  static constexpr uint8_t MIN_TICKS_0p5C_DELTA = (MIN_TICKS_1C_DELTA/2);

  // Construct an instance, with sensible defaults, but no (room) temperature.
  // Defers its initialisation with room temperature until first tick().
  ModelledRadValveState() { }

  // Construct an instance, with sensible defaults, but no (room) temperature.
  // Defers its initialisation with room temperature until first tick().
  ModelledRadValveState(bool _alwaysGlacial) : alwaysGlacial(_alwaysGlacial) { }

  // Construct an instance, with sensible defaults, and current (room) temperature from the input state.
  // Does its initialisation with room temperature immediately.
  ModelledRadValveState(const ModelledRadValveInputState &inputState, bool _alwaysGlacial = false);

  // Perform per-minute tasks such as counter and filter updates then recompute valve position.
  // The input state must be complete including target/reference temperatures
  // before calling this including the first time
  // whereupon some further lazy initialisation is done.
  //   * valvePCOpenRef  current valve position UPDATED BY THIS ROUTINE;
  //         in range [0,100]
  //   * inputState  immutable input state reference
  //   * physicalDeviceOpt  physical device to set with new target if non-NULL
  // If the physical device is provided then its target will be updated
  // and its actual value will be monitored for cumulative movement,
  // else if not provided the movement in valvePCOpenRef will be monitored.
  void tick(volatile uint8_t &valvePCOpenRef,
            const ModelledRadValveInputState &inputState,
            AbstractRadValve *const physicalDeviceOpt);

  // True if by default/always in glacial mode, eg to minimise flow / overshoot.
  const bool alwaysGlacial = false;

  // True once all deferred initialisation done during the first tick().
  // This takes care of setting state that depends on run-time data
  // such as real temperatures to propagate into all the filters.
  bool initialised = false;

  // If !false then filtering is being applied since temperatures fast-changing.
  // Can be used as if a bool, though may be set > 1 to allow a timeout.
//  bool isFiltering = false;
  uint8_t isFiltering = 0;

  // True if the computed modelled valve position was changed by tick().
  // This is not an indication if any underlying valve position has changed.
  bool valveMoved = false;

  // Testable/reportable events.
  // Logically not part of this struct's state, so all ops are const.
  // Cleared at the start of each tick().
  // Set as appropriate by computeRequiredTRVPercentOpen() to indicate
  // particular activity and paths taken.
  // May only be reported and accessible in debug mode;
  // primarily to facilitate unit testing.
  typedef enum event : uint8_t
    {
    MRVE_NONE,      // No event.
    MRVE_OPENFAST,  // Fast open as per TODO-593.
    MRVE_DRAUGHT    // Cold draught detected.
    } event_t;
#if 0
  private:
  mutable event_t lastEvent = MRVE_NONE;
  public:
  static constexpr bool eventsSupported = true;
  // Clear the last event, ie event state becomes MRVE_NONE.
  void clearEvent() const { lastEvent = MRVE_NONE; }
  // Set the event to be as passed.
  void setEvent(event_t event) const { lastEvent = event; }
  // Get the last event; MRVE_NONE if none.
  event_t getLastEvent() const { return(lastEvent); }
#else
  static constexpr bool eventsSupported = false;
  // Dummy placeholders where event state not held.
  void clearEvent() const { }
  // Set the event to be as passed.
  void setEvent(event_t) const { }
  // Get the last event always MRVE_NONE.
  event_t getLastEvent() const { return(MRVE_NONE); }
#endif

  // Set non-zero when valve flow is constricted, and then counts down to zero.
  // Some or all attempts to open the valve are deferred while this is non-zero
  // to reduce valve hunting if there is string turbulence from the radiator
  // or maybe draughts from open windows/doors
  // causing measured temperatures to veer up and down.
  // This attempts to reduce excessive valve noise and energy use
  // and help to avoid boiler short-cycling.
  uint8_t valveTurndownCountdownM = 0;
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
  uint8_t valveTurnupCountdownM = 0;
  // Mark flow as having been increased.
  // TODO: possibly increase reclose delay in filtering/wide-deadband mode.
  void valveTurnup() { valveTurnupCountdownM = DEFAULT_ANTISEEK_VALVE_RECLOSE_DELAY_M; }
  // If true then avoid turning down the heat yet.
  bool dontTurndown() const { return(0 != valveTurnupCountdownM); }

  // Cumulative valve movement count, as unsigned cumulative percent with rollover [0,8191].
  // This is a useful as a measure of battery consumption (slewing the valve)
  // and noise generated (and thus disturbance to humans)
  // and of appropriate control damping.
  //
  // DHD20161109: due to possible g++ 4.9.x bug,
  // NOT kept as an unsigned 10-bit field (uint16_t x : 10),
  // but is coerced to range after each change.
  static constexpr uint16_t MAX_CUMULATIVE_MOVEMENT_VALUE = 0x3ff;
  //
  // Cumulative valve movement %; rolls at 1024 in range [0,1023].
  // Most of the time JSON value is 3 digits or fewer, conserving bandwidth.
  // It would often be appropriate to mark this as low priority
  // since it can be approximated from observed valve positions over time.
  // This is computed from actual underlying valve movements if possible,
  // rather than just the modelled valve movements.
  //
  // The (masked) value doesn't wrap round to a negative value
  // and can safely be sent/received in JSON by hosts with 16-bit signed ints,
  // and the maximum number of decimal digits used in its representation is 4
  // but is almost always 3 (or fewer)
  // and used efficiently (~80% use of the top digit).
  //
  // Daily allowance (in terms of battery/energy use)
  // is assumed to be ~400% (DHD20141230),
  // so this should hold much more than that to avoid ambiguity
  // from missed/infrequent readings,
  // especially given full slew (+100%) can sometimes happen in 1 minute/tick.
  uint16_t cumulativeMovementPC = 0;

  // Previous valve position (%), used to compute cumulativeMovementPC.
  uint8_t prevValvePC = 0;

  // Length of filter memory in ticks; strictly positive.
  // Must be at least 4, and may be more efficient at a power of 2.
  static constexpr size_t filterLength = 16;

  // Previous unadjusted temperatures, 0 being the newest, and following ones successively older.
  // These values have any target bias removed.
  // Half the filter size times the tick() interval gives an approximate time constant.
  // Note that full response time of a typical mechanical wax-based TRV is ~20mins.
  int_fast16_t prevRawTempC16[filterLength];

  // If true, detect jitter between adjacent samples to turn filter on.
  // Whether or not true, other detection mechanisms may be used.
  static constexpr bool FILTER_DETECT_JITTER = false;

  // Get smoothed raw/unadjusted temperature from the most recent samples.
  int_fast16_t getSmoothedRecent() const;

  // Get last change in temperature (C*16, signed); +ve means rising.
  int_fast16_t getRawDelta() const { return(prevRawTempC16[0] - prevRawTempC16[1]); }

  // Get last change in temperature (C*16, signed) from n ticks ago capped to filter length; +ve means rising.
  int_fast16_t getRawDelta(uint8_t n) const { return(prevRawTempC16[0] - prevRawTempC16[OTV0P2BASE::fnmin((size_t)n, filterLength-1)]); }

  // Get previous change in temperature (C*16, signed); +ve means was rising.
  int_fast16_t getPrevRawDelta() const { return(prevRawTempC16[1] - prevRawTempC16[2]); }

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

  // Fill the filter memory with the current room temperature in its internal form, as during initialisation.
  // Not intended for general use.
  // Can be used when testing to avoid filtering being triggered with rapid simulated temperature swings.
  inline void _backfillTemperatures(const int_fast16_t rawTempC16)
    { for(int_fast8_t i = filterLength; --i >= 0; ) { prevRawTempC16[i] = rawTempC16; } }

  // Compute the adjusted temperature as used within the class calculation, filter, etc.
  static int_fast16_t computeRawTemp16(const ModelledRadValveInputState& inputState)
    { return(inputState.refTempC16 - ModelledRadValveInputState::refTempOffsetC16); }

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
    constexpr ModelledRadValveSensorCtrlStats(
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
    // Computes the target temperature based on various sensors,
    // controls and stats.
    // Can be called as often as required though may be slow/expensive.
    // Will be called by computeTargetTemperature().
    // A prime aim is to allow reasonable energy savings (10--30%+)
    // even if the device is left untouched and in WARM mode all the time,
    // using occupancy/light/etc to determine when temperature can be set back
    // without annoying users.
    //
    // Attempts in WARM mode to make the deepest reasonable cuts
    // to maximise savings when the room is vacant
    // and not likely to become occupied again soon,
    // ie this looks ahead to give the room time
    // to get to or close to target before occupancy.
    //
    // Stateless directly-testable version behind computeTargetTemperature().
    virtual uint8_t computeTargetTemp() const = 0;

    // Set all fields of inputState from the target temperature and other args, and the sensor/control inputs.
    // The target temperature will usually have just been computed by
    // computeTargetTemp().
    virtual void setupInputState(ModelledRadValveInputState &inputState,
        const bool isFiltering,
        const uint8_t newTargetC,
        const uint8_t minPCOpen, const uint8_t maxPCOpen,
        const bool glacial) const = 0;
  };

// Basic/simple stateless implementation of computation of target temperature.
// Templated with all input instances for maximum speed and minimum code size.
//
// TODO: incorporate condensation protection by keeping above the dew point:
//
//     Td = T - ((100 - RH)/5.)
//
// taken from: https://iridl.ldeo.columbia.edu/dochelp/QA/Basic/dewpoint.html
//
// where T and RH are the current temperature and relative humidity.
template<
  class valveControlParameters,
  const ValveMode *const valveMode,
  class TemperatureC16Base,                     const TemperatureC16Base *const temperatureC16,
  class TempControlBase,                        const TempControlBase *const tempControl,
  class PseudoSensorOccupancyTracker,           const PseudoSensorOccupancyTracker *const occupancy,
  class SensorAmbientLightBase,                 const SensorAmbientLightBase *const ambLight,
  class ActuatorPhysicalUIBase,                 const ActuatorPhysicalUIBase *const physicalUI,
  class SimpleValveScheduleBase,                const SimpleValveScheduleBase *const schedule,
  class NVByHourByteStatsBase,                  const NVByHourByteStatsBase *const byHourStats,
  class rh_t = OTV0P2BASE::HumiditySensorBase,  const rh_t *const relHumidityOpt = static_cast<const rh_t *>(NULL),
  bool (*const setbackLockout)() = ((bool(*)())NULL)
  >
class ModelledRadValveComputeTargetTempBasic final : public ModelledRadValveComputeTargetTempBase
  {
  public:
    constexpr ModelledRadValveComputeTargetTempBasic()
        {
        // Validate (non-optional) args.  Doesn't work with g++ 4.9.2 in Arduino IDE.
        static_assert(valveMode, "non-optional parameter must not be NULL");
        static_assert(temperatureC16, "non-optional parameter must not be NULL");
        static_assert(tempControl, "non-optional parameter must not be NULL");
        static_assert(occupancy, "non-optional parameter must not be NULL");
        static_assert(ambLight, "non-optional parameter must not be NULL");
        static_assert(physicalUI, "non-optional parameter must not be NULL");
        static_assert(schedule, "non-optional parameter must not be NULL");
        static_assert(byHourStats, "non-optional parameter must not be NULL");
        }
    virtual uint8_t computeTargetTemp() const override
        {
        // In FROST mode.
        if(!valveMode->inWarmMode())
          {
          const uint8_t frostC = tempControl->getFROSTTargetC();

          // If a scheduled WARM is due soon then ensure
          // that room is at least at a smallish setback temperature
          // to give room a chance to hit the WARM target,
          // and for furniture and surfaces to be warm, etc, on time.
          // Don't do this if the room has been vacant for a long time
          // (eg so as to avoid pre-warm ever being higher than WARM).
          // Don't do this if there has been recent manual intervention,
          // eg to allow manual 'cancellation' of pre-heat.  (TODO-464)
          // Only do this if the target WARM temperature is
          // NOT an 'ECO' temperature (ie very near the bottom of the scale).
          // If well into the 'ECO' zone go for a larger-than-usual setback,
          // else set minimal/default setback.
          // Note: when pre-warm and warm time for schedule is ~1.5h,
          // and the default setback is 1C,
          // this is assuming that the room temperature can be raised by ~1C/h.
          // See the effect of going from 2C to 1C setback:
          //     http://www.earth.org.uk/img/20160110-vat-b.png
          // (A very long pre-warm time may confuse or distress users,
          // eg waking them in the morning.)
          if(!occupancy->longVacant() &&
             schedule->isAnyScheduleOnWARMSoon() &&
             !physicalUI->recentUIControlUse())
            {
            const uint8_t warmTarget = tempControl->getWARMTargetC();
            // Compute putative pre-warm temperature, usually just below WARM.
            const uint8_t preWarmTempC = OTV0P2BASE::fnmax(frostC,
                uint8_t(warmTarget -
                    (tempControl->isEcoTemperature(warmTarget) ?
                        valveControlParameters::SETBACK_ECO :
                        valveControlParameters::SETBACK_DEFAULT)));
            return(preWarmTempC);
            }

          // Apply FROST safety target temperature by default in FROST mode.
          return(frostC);
          }

        // If in BAKE mode then use elevated target, with no setbacks.
        else if(valveMode->inBakeMode())
          {
          return(OTV0P2BASE::fnmin((uint8_t)(tempControl->getWARMTargetC() +
              valveControlParameters::BAKE_UPLIFT), OTRadValve::MAX_TARGET_C));
          }

        else // In 'WARM' mode with possible setback.
          {
          const uint8_t wt = tempControl->getWARMTargetC();

          // If smart setbacks are locked out then return WARM temperature as-is.  (TODO-786, TODO-906)
          if((NULL != setbackLockout) && (setbackLockout)())
            { return(wt); }

//          const bool longLongVacant = occupancy->longLongVacant();
          const bool longVacant = /*longLongVacant || */ occupancy->longVacant();
          const bool confidentlyVacant = longVacant || occupancy->confidentlyVacant();
          const bool likelyVacantNow = confidentlyVacant || occupancy->isLikelyUnoccupied();

          // No setback unless apparently vacant and no scheduled WARM.
          // TODO: or dark and weakly occupied in case room only briefly occupied.
          // TODO: or set back on anticipated vacancy.
          const bool allowSetback = likelyVacantNow &&
              (/*long*/longVacant || !schedule->isAnyScheduleOnWARMNow());

          if(allowSetback)
            {
            // Use DEFAULT setback unless confident that more is OK.
            // This default should not be annoying, but saves little energy.
            uint8_t setback = valveControlParameters::SETBACK_DEFAULT;

            // Note when it has been dark for many hours, overnight in winter.
            // This should be long enough to almost never be true
            // in the afternoon or early evening even in long winter days.
            const uint16_t dm = ambLight->getDarkMinutes();
            static constexpr uint16_t longDarkM = 7*60U; // 7h

            // Any imminent scheduled on may inhibit all but minimum setback.
            const bool scheduleOnSoon = schedule->isAnyScheduleOnWARMSoon();
            // High likelihood of occupancy now inhibits ECO setback.
            const uint8_t hoursLessOccupiedThanThis =
                byHourStats->countStatSamplesBelow(
                    OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_OCCPC_BY_HOUR_SMOOTHED,
                    byHourStats->getByHourStatRTC(
                        OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_OCCPC_BY_HOUR_SMOOTHED,
                        OTV0P2BASE::NVByHourByteStatsBase::SPECIAL_HOUR_CURRENT_HOUR));
            static constexpr uint8_t maxThr = 17;
            static constexpr uint8_t minThr = 14;
            static_assert(maxThr >= minThr, "sensitivity must not decrease with temp");
            const uint8_t thisHourNLOThreshold =
                tempControl->hasEcoBias() ? maxThr : minThr;
            const bool relativelyActive =
                (hoursLessOccupiedThanThis > thisHourNLOThreshold);
            // Inhibit ECO (or more) setback
            // for scheduled-on (unless long vacant, eg a day or more)
            // or where this hour is typically relatively busy
            // (unless 'vacant' for the equivalent of a decent night's sleep).
            // Avoid inhibiting warm-up before return from work/school.
            const bool inhibitECOSetback = !longVacant &&
                (scheduleOnSoon ||
                ((dm < longDarkM) && (relativelyActive)));

            // ECO setback is possible: bulk of energy saving opportunities.
            // Go for ECO if dark or likely vacant now,
            // and not usually relatively occupied now or in the next hour.
            if(!inhibitECOSetback &&
               (confidentlyVacant ||
                (likelyVacantNow && (hoursLessOccupiedThanThis <= 1)) ||
                (0 != dm)))
                {
                setback = valveControlParameters::SETBACK_ECO;

                // High likelihood of occupancy soon inhibits FULL setback,
                // (unless dark for hours so as to avoid waking users early)
                // to allow getting warm ready for return from work/school.
                // TODO: other signals such as manual control use and
                // typical temperature at this time could inhibit FULL setback.
                const uint8_t hoursLessOccupiedThanNext =
                    byHourStats->countStatSamplesBelow(
                        OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_OCCPC_BY_HOUR_SMOOTHED,
                        byHourStats->getByHourStatRTC(
                            OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_OCCPC_BY_HOUR_SMOOTHED,
                            OTV0P2BASE::NVByHourByteStatsBase::SPECIAL_HOUR_NEXT_HOUR));
                const bool relativelyActiveSoon =
                    (hoursLessOccupiedThanNext > 2 + thisHourNLOThreshold);

                // Set a lower occupancy threshold to prevent FULL setback.
                // Much lower if not dark for too long.
                static constexpr uint8_t linReduction = 4;
                static_assert(linReduction < minThr, "ensure new threshold is sane/+ve");
                // Note: ignoring nominal overflow of dm>>5 in uint8_t since other factors should make it irrelevant.
                const uint8_t thisHourNLOThresholdF =
                    OTV0P2BASE::fnmin(thisHourNLOThreshold - linReduction,
                                      (thisHourNLOThreshold >> 2) + uint8_t(dm>>5));
                const bool notInactive =
                    (hoursLessOccupiedThanThis > thisHourNLOThresholdF);

                // Inhibit FULL setback if at top end of comfort range.
                const bool comfortTemperature = tempControl->isComfortTemperature(wt);
                const bool inhibitFULLSetback = comfortTemperature ||
                    ((dm < longDarkM) && (notInactive || relativelyActiveSoon));

                // FULL setback possible; saving energy/noise for night/holiday.
                // If long vacant (no sign of activity for around a day)
                // OR dark for a while AND return not strongly anticipated
                // then allow a maximum night setback and minimise noise (TODO-792, TODO-1027)
                // Drop through quicker in darkness when current/next hours
                // are rarely occupied (ie anticipatory turn down);
                // also help avoid revving up heating for brief lights-on
                // in the middle of the night.  (TODO-1092)
                const bool veryQuiet =
                    (hoursLessOccupiedThanThis <= 1) ||
                    (hoursLessOccupiedThanNext <= 1);
                if(!inhibitFULLSetback &&
                   (longVacant || (dm >= (veryQuiet ? 2 : 10))))
                    { setback = valveControlParameters::SETBACK_FULL; }
                }

            // Target must never be set low enough to create a frost/freeze hazard.
            const uint8_t newTarget = OTV0P2BASE::fnmax((uint8_t)(wt - setback), tempControl->getFROSTTargetC());

            return(newTarget);
            }

          // Else use WARM target as-is.
          return(wt);
          }
        }

    // Set all fields of inputState from the target temperature etc.
    // Usually target temp will just have been computed by computeTargetTemp().
    // This should not second-guess computeTargetTemp() in terms of setbacks.
    virtual void setupInputState(ModelledRadValveInputState &inputState,
        const bool /*isFiltering*/,
        const uint8_t newTargetC,
        const uint8_t /*minPCOpen*/, const uint8_t maxPCOpen,
        const bool glacial) const override
        {
        // Set up state for computeRequiredTRVPercentOpen().
        inputState.targetTempC = newTargetC;
        const uint8_t wt = tempControl->getWARMTargetC();
        inputState.maxTargetTempC = wt;
//        inputState.minPCReallyOpen = minPCOpen;
        inputState.maxPCOpen = maxPCOpen;
        // Force glacial if unusually low maxPCOpen
        // that would interact badly with other aspects of the algorithm.
        // Note: may also wish to force glacial if room very dark
        // to minimise noise.  (TODO-1027)
        inputState.glacial = glacial ||
            (maxPCOpen < DEFAULT_VALVE_PC_SAFER_OPEN);
        inputState.inBakeMode = valveMode->inBakeMode();
        inputState.hasEcoBias = tempControl->hasEcoBias();
        // Request a fast response from the valve
        // if the user is currently manually adjusting the controls (TODO-593)
        // or there is a very recent (and reasonably strong) occupancy signal
        // such as lights on.  (TODO-1069)
        // This may provide enough feedback to have the user resist adjusting
        // controls prematurely!
        const bool fastResponseRequired =
            physicalUI->veryRecentUIControlUse() ||
            (occupancy->reportedNewOccupancyRecently());
        inputState.fastResponseRequired = fastResponseRequired;
        // Widen the allowed deadband significantly in a dark room
        // to save (heating/battery) energy and noise (TODO-383, TODO-1037)
        // (OR if temperature is jittery eg changing fast and filtering is on,
        // OR if any setback is in place or in FROST ie anything below WARM)
        // to attempt to reduce the total number and size of adjustments and
        // thus reduce noise/disturbance (and battery drain).
        // The wider deadband (less good temperature regulation) may be
        // noticeable/annoying to sensitive occupants.
        // A wider deadband may also simply suppress any movement/noise on
        // some/most minutes while close to target temperature.
        // For responsiveness, don't widen the deadband immediately
        // after manual controls have been used.  (TODO-593)
        // DHD20170109: filtering effectively forces wide in computation now.
        inputState.widenDeadband = (!fastResponseRequired) &&
            ((newTargetC < wt)
                || ambLight->isRoomDark()); // Must return false if not usable.
        // Capture adjusted reference/room temperature.
        inputState.setReferenceTemperatures(temperatureC16->get());
        }
  };

// Pre-2017 stateless implementation of computation of target temperature.
// Templated with all the input instances for maximum speed and minimum code size.
template<
  class valveControlParameters,
  const ValveMode *const valveMode,
  class TemperatureC16Base,                     const TemperatureC16Base *const temperatureC16,
  class TempControlBase,                        const TempControlBase *const tempControl,
  class PseudoSensorOccupancyTracker,           const PseudoSensorOccupancyTracker *const occupancy,
  class SensorAmbientLightBase,                 const SensorAmbientLightBase *const ambLight,
  class ActuatorPhysicalUIBase,                 const ActuatorPhysicalUIBase *const physicalUI,
  class SimpleValveScheduleBase,                const SimpleValveScheduleBase *const schedule,
  class NVByHourByteStatsBase,                  const NVByHourByteStatsBase *const byHourStats,
  class rh_t = OTV0P2BASE::HumiditySensorBase,  const rh_t *const relHumidityOpt = static_cast<const rh_t *>(NULL),
  bool (*const setbackLockout)() = ((bool(*)())NULL)
  >
class ModelledRadValveComputeTargetTemp2016 final : public ModelledRadValveComputeTargetTempBase
  {
  public:
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
          const bool isDark = ambLight->isRoomDark();
          const uint16_t dm = ambLight->getDarkMinutes();
          const bool darkForHours = dm > 245; // A little over 4h.
          // Be more ready to decide room not likely occupied soon if eco-biased.
          // Note that this value is likely to be used +/- 1 so must be in range [1,23].
          const uint8_t thisHourNLOThreshold = ecoBias ? 15 : 12;
          const uint8_t hoursLessOccupiedThanThis = byHourStats->countStatSamplesBelow(OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_OCCPC_BY_HOUR_SMOOTHED, byHourStats->getByHourStatRTC(OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_OCCPC_BY_HOUR_SMOOTHED, OTV0P2BASE::NVByHourByteStatsBase::SPECIAL_HOUR_CURRENT_HOUR));
          const uint8_t hoursLessOccupiedThanNext = byHourStats->countStatSamplesBelow(OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_OCCPC_BY_HOUR_SMOOTHED, byHourStats->getByHourStatRTC(OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_OCCPC_BY_HOUR_SMOOTHED, OTV0P2BASE::NVByHourByteStatsBase::SPECIAL_HOUR_NEXT_HOUR));
          const bool notLikelyOccupiedSoon = longLongVacant ||
              (likelyVacantNow &&
              // No more than about half the hours to be less occupied than this hour to be considered unlikely to be occupied.
              (hoursLessOccupiedThanThis < thisHourNLOThreshold) &&
              // Allow to be a little bit more occupied for the next hour than the current hour.
              // Suppress occupancy lookahead if room has been dark for several hours, eg overnight.  (TODO-792)
              (darkForHours || (hoursLessOccupiedThanNext < (thisHourNLOThreshold+1))));
          const uint8_t minLightsOffForSetbackMins = ecoBias ? 10 : 20;
          if(longVacant ||
             ((notLikelyOccupiedSoon || (dm > minLightsOffForSetbackMins) || (ecoBias && (occupancy->getVacancyH() > 0) && (0 == byHourStats->getByHourStatRTC(OTV0P2BASE::NVByHourByteStatsBase::STATS_SET_OCCPC_BY_HOUR, OTV0P2BASE::NVByHourByteStatsBase::SPECIAL_HOUR_CURRENT_HOUR)))) &&
                 !schedule->isAnyScheduleOnWARMNow() && !physicalUI->recentUIControlUse()))
            {
            // Restrict to a DEFAULT/minimal non-annoying setback if:
            //   in upper part of comfort range (and the room isn't very dark, eg in the dead of night TODO-1027)
            //   or if the room is likely occupied now
            //   or if not lower part of ECO range and not dark and not vacant for a while and relative humidity is high with hysteresis (TODO-586)
            //   or if the room is not known to be dark and hasn't been vacant for a long time ie ~1d and is not in the very bottom range of occupancy (TODO-107, TODO-758)
            //      TODO POSSIBLY: limit to (say) 3--4h light time for when someone out but room daylit, but note that detecting occupancy will be harder too in daylight.
            //      TODO POSSIBLY: after ~3h vacancy AND apparent smoothed occupancy non-zero (so some can be detected) AND ambient light in top quartile or in middle of typical bright part of cycle (assume peak of daylight) then being lit is not enough to prevent a deeper setback.
            //   or if the room has not been dark for hours and is not in the very bottom range of occupancy (TODO-107, TODO-758)
            //   or if a scheduled WARM period is due soon and the room hasn't been vacant for a long time,
            // else usually use a somewhat bigger ECO setback
            // else use an even bigger FULL setback for maximum savings if in the ECO region and
            //   the room has been vacant for a very long time
            //   or is unlikely to be unoccupied at this time of day and
            //     has been vacant and dark for a while or is in the lower part of the ECO range.
            // This final dark/vacant timeout to enter FULL setback while in mild ECO mode
            // should probably be longer than required to watch a typical movie or go to sleep (~2h) for example,
            // but short enough to take effect overnight and to be in effect a reasonable fraction of a (~8h) night.
            //
            // Note: a FULL setback is prevented when at the the top end
            // of the scale, ie at a comfort temperature,
            // and is usually limited to minimum/default.
            constexpr uint8_t minVacantAndDarkForFULLSetbackH = 2; // Hours; strictly positive, typically 1--4.
            const bool comfortTemperature = tempControl->isComfortTemperature(wt);
            const uint8_t setback = ((comfortTemperature && !ambLight->isRoomVeryDark()) ||
                                     occupancy->isLikelyOccupied() ||
                                     (!longVacant && !isDark && !tempControl->isEcoTemperature(wt) && (NULL != relHumidityOpt) && relHumidityOpt->isAvailable() && relHumidityOpt->isRHHighWithHyst()) ||
                                     (!longVacant && !isDark && (hoursLessOccupiedThanThis > 4)) ||
                                     (!longVacant && !isDark && !darkForHours && (hoursLessOccupiedThanNext >= thisHourNLOThreshold-1)) ||
                                     (!longVacant && schedule->isAnyScheduleOnWARMSoon())) ?
                    valveControlParameters::SETBACK_DEFAULT :
                ((!comfortTemperature && (longLongVacant ||
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
    // This should not second-guess computeTargetTemp() in terms of setbacks, etc.
    virtual void setupInputState(ModelledRadValveInputState &inputState,
        const bool isFiltering,
        const uint8_t newTarget, const uint8_t /*minPCOpen*/, const uint8_t maxPCOpen, const bool glacial) const override
        {
        // Set up state for computeRequiredTRVPercentOpen().
        inputState.targetTempC = newTarget;
//        inputState.minPCReallyOpen = minPCOpen;
        inputState.maxPCOpen = maxPCOpen;
        inputState.glacial = glacial; // Note: may also wish to force glacial if room very dark to minimise noise (TODO-1027).
        inputState.inBakeMode = valveMode->inBakeMode();
        inputState.hasEcoBias = tempControl->hasEcoBias();
        // Request a fast response from the valve if the user is currently manually adjusting the controls (TODO-593)
        // or there is a very recent (and reasonably strong) occupancy signal such as lights on (TODO-1069).
        // This may provide enough feedback to have the user resist adjusting things prematurely!
        const bool fastResponseRequired =
            physicalUI->veryRecentUIControlUse() || (occupancy->reportedRecently() && occupancy->isLikelyOccupied());
        inputState.fastResponseRequired = fastResponseRequired;
        // Widen the allowed deadband significantly in a dark room (TODO-383, TODO-1037)
        // (or if temperature is jittery eg changing fast and filtering has been engaged,
        // or if any setback is in place or is in FROST mode ie anything below the WARM target)
        // to attempt to reduce the total number and size of adjustments and thus reduce noise/disturbance (and battery drain).
        // The wider deadband (less good temperature regulation) might be noticeable/annoying to sensitive occupants.
        // With a wider deadband may also simply suppress any movement/noise on some/most minutes while close to target temperature.
        // For responsiveness, don't widen the deadband immediately after manual controls have been used (TODO-593).
        inputState.widenDeadband = (!fastResponseRequired) &&
            (isFiltering
                || ambLight->isRoomDark() // Must be false if light sensor not usable.
                || (newTarget < tempControl->getWARMTargetC())); // There is a setback in place, or not WARM mode.
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
    // May need to be set true if maxPCOpen unusually low.
    bool glacial;

    // Maximum percentage valve is allowed to be open [0,100].
    // Usually 100, but special circumstances may require otherwise.
    const uint8_t maxPCOpen = 100;

    // Compute target temperature and set heat demand for TRV and boiler; update state.
    // CALL REGULARLY APPROXIMATELY ONCE PER MINUTE TO ALLOW SIMPLE TIME-BASED CONTROLS.
    // Inputs are inWarmMode(), isRoomLit().
    // This routine may take significant CPU time; no I/O is done,
    // only internal state is updated.
    //
    // Will clear any BAKE mode if the newly-computed target temperature
    // is already exceeded.
    void computeCallForHeat();

    // Read/write (non-const) access to valveMode instance; never NULL.
    ValveMode *const valveModeRW;

    // Read/write access to the underlying physical device; NULL if none.
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
        physicalDeviceOpt(_physicalDeviceOpt),
        targetTemperatureSubSensor(inputState.targetTempC, V0p2_SENSOR_TAG_F("tT|C")),
        setbackSubSensor(setbackC, V0p2_SENSOR_TAG_F("tS|C")),
        cumulativeMovementSubSensor(retainedState.cumulativeMovementPC, V0p2_SENSOR_TAG_F("vC|%"))
      { }

    // Read-only access to physical device if any, else this; never NULL.
    // Used to make available get() for underlying if required, eg for stats.
    // This should NOT be used to get() valve position to send to the boiler,
    // since the logical position, eg wrt call-for-heat threshold, is critical,
    // not where the physical valve happens to be, dependent on other factors.
    const AbstractRadValve *getPhysicalDevice() const
        { return((NULL != physicalDeviceOpt) ? physicalDeviceOpt : this); }

    // Force a read/poll/recomputation of the target position and call for heat.
    // Sets/clears changed flag if computed valve position changed.
    // Pushes target to physical device if configured.
    // Call at a fixed rate (1/60s).
    // Potentially expensive/slow.
    virtual uint8_t read() override { computeCallForHeat(); return(value); }

    // Returns preferred poll interval (in seconds); non-zero.
    // Must be polled at near constant rate, about once per minute.
    virtual uint8_t preferredPollInterval_s() const override { return(60); }

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
    // and this needs more heat than can be passively drawn from
    // an already-running boiler.
    // Thread-safe and ISR safe.
    virtual bool isUnderTarget() const override { return(underTarget); }

    // Get target temperature in C as computed by computeTargetTemperature().
    uint8_t getTargetTempC() const { return(inputState.targetTempC); }

    // DEPRECATED IN FAVOUR OF targetTemperatureSubSensor.tag().
    // Returns a suggested (JSON) tag/field/key name including units of getTargetTempC(); not NULL.
    // The lifetime of the pointed-to text must be at least that of this instance.
    OTV0P2BASE::Sensor_tag_t tagTTC() const { return(targetTemperatureSubSensor.tag()); }

    // Facade/sub-sensor for target temperature (in C), at normal priority.
    const OTV0P2BASE::SubSensorSimpleRef<uint8_t, false> targetTemperatureSubSensor;

    // Get the current automated setback (if any) in the direction of energy saving in C; non-negative.
    // For heating this is the number of C below the nominal user-set
    // target temperature that getTargetTempC() is;
    // zero if no setback is in effect.
    // Generally will be 0 in FROST or BAKE modes.
    // Not ISR-/thread- safe.
    uint8_t getSetbackC() const { return(setbackC); }

    // DEPRECATED IN FAVOUR OF setbackSubSensor.tag().
    // Returns a (JSON) tag/field/key name including units of getSetbackC(); not NULL.
    // It would often be appropriate to mark this as low priority
    // since depth of setback matters more than speed.
    // The lifetime of the pointed-to text is at least that of this instance.
    OTV0P2BASE::Sensor_tag_t tagTSC() const { return(setbackSubSensor.tag()); }

    // Facade/sub-sensor for setback level (in C), at low priority.
    const OTV0P2BASE::SubSensorSimpleRef<uint8_t> setbackSubSensor;

    // Get cumulative valve movement %; rolls at 1024 in range [0,1023].
    // Most of the time JSON value is 3 digits or fewer, conserving bandwidth.
    // It would often be appropriate to mark this as low priority
    // since it can be approximated from observed valve positions over time.
    // This is computed from actual underlying valve movements if possible,
    // rather than just the modelled valve movements.
    uint16_t getCumulativeMovementPC() { return(retainedState.cumulativeMovementPC); }

    // DEPRECATED IN FAVOUR OF cumulativeMovementSubSensor.tag().
    // Returns a (JSON) tag/field/key name including units of getCumulativeMovementPC(); not NULL.
    // The lifetime of the pointed-to text is at least that of this instance.
    OTV0P2BASE::Sensor_tag_t tagCMPC() const { return(cumulativeMovementSubSensor.tag()); }

    // Facade/sub-sensor cumulative valve movement (in %), at low priority.
    const OTV0P2BASE::SubSensorSimpleRef<uint16_t> cumulativeMovementSubSensor;

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
    // Will clear any BAKE mode if the newly-computed target temperature
    // is already exceeded.
    void computeTargetTemperature();

    // Pass through a wiggle request to the underlying device if specified.
    virtual void wiggle() const override { if(NULL != physicalDeviceOpt) { physicalDeviceOpt->wiggle(); } }
  };


    }

#endif
