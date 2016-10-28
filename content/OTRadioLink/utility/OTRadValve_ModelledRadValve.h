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
struct ModelledRadValveInputState
  {
  // Offset from raw temperature to get reference temperature in C/16.
  static const int_fast8_t refTempOffsetC16 = 8;

  // All initial values set by the constructor are sane, but should not be relied on.
  ModelledRadValveInputState(const int_fast16_t realTempC16 = 0);

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


// Internal model of radiator valve position, embodying control logic.
#define ModelledRadValve_DEFINED
class ModelledRadValve : public AbstractRadValve
  {
  public:
    // Sensor, control and stats inputs for computations.
    // Read access to all necessary underlying devices.
    struct ModelledRadValveSensorCtrlStats
        {
        // Read-only valve control parameters.
        const ValveControlParametersRTBase *const vcp;

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

        // Returns true if set-back lockout is in place; never NULL.
        // This usually returns false, but when true disables most automatic energy-saving features.
        bool (*const setbackLockout)();

        // Construct an instance wrapping read-only access to all input devices.
        // All arguments must be non-NULL.
        ModelledRadValveSensorCtrlStats(
            const ValveControlParametersRTBase *const _vcp,
            const ValveMode *const _valveMode,
            const OTV0P2BASE::TemperatureC16Base *const _temperatureC16,
            const TempControlBase *const _tempControl,
            const OTV0P2BASE::PseudoSensorOccupancyTracker *const _occupancy,
            const OTV0P2BASE::SensorAmbientLightBase *const _ambLight,
            const ActuatorPhysicalUIBase *const _physicalUI,
            const OTV0P2BASE::SimpleValveScheduleBase *const _schedule,
            const OTV0P2BASE::NVByHourByteStatsBase *const _byHourStats,
            bool (*const _setbackLockout)() = []{return(false);})
           : vcp(_vcp),
             valveMode(_valveMode),
             temperatureC16(_temperatureC16),
             tempControl(_tempControl),
             occupancy(_occupancy),
             ambLight(_ambLight),
             physicalUI(_physicalUI),
             schedule(_schedule),
             byHourStats(_byHourStats),
             setbackLockout(_setbackLockout)
             { }
        };

  private:
    // All input state for deciding where to set the radiator valve in normal operation.
    struct ModelledRadValveInputState inputState;
    // All retained state for deciding where to set the radiator valve in normal operation.
    struct ModelledRadValveState retainedState;
    // Sensor, control and stats inputs for computations.
    struct ModelledRadValveSensorCtrlStats sensorCtrlStats;

    // True if this node is calling for heat.
    // Marked volatile for thread-safe lock-free access.
    volatile bool callingForHeat = false;

    // True if the room/ambient temperature is below target, enough to likely call for heat.
    // Marked volatile for thread-safe lock-free access.
    volatile bool underTarget = false;

    // The current automated setback (if any) in the direction of energy saving in C; non-negative.
    // Not intended for ISR/threaded access.
    uint8_t setbackC = 0;

    // True if always in glacial mode, eg to minimise flow and overshoot.
    const bool alwaysGlacial;

    // True if in glacial mode.
    bool glacial;

    // Maximum percentage valve is allowed to be open [0,100].
    // Usually 100, but special circumstances may require otherwise.
    const uint8_t maxPCOpen;

    // Cache of minValvePcReallyOpen value [0,99] to save some EEPROM / nv-store access.
    // A value of 0 (eg at initialisation) means not yet loaded from EEPROM / nv-store.
    mutable uint8_t mVPRO_cache = 0;

    // Compute target temperature and set heat demand for TRV and boiler; update state.
    // CALL REGULARLY APPROXIMATELY ONCE PER MINUTE TO ALLOW SIMPLE TIME-BASED CONTROLS.
    // Inputs are inWarmMode(), isRoomLit().
    // The inputs must be valid (and recent).
    // Values set are targetTempC, value (TRVPercentOpen).
    // This may also prepare data such as TX command sequences for the TRV, boiler, etc.
    // This routine may take significant CPU time; no I/O is done, only internal state is updated.
    // Returns true if valve target changed and thus messages may need to be recomputed/sent/etc.
    void computeCallForHeat();

    // Read-write (non-const) access to valveMode instance; never NULL.
    ValveMode *const valveModeRW;

  public:
    // Create an instance.
    ModelledRadValve(
        const ValveControlParametersRTBase *const _vcp,
        ValveMode *const _valveMode,
        const OTV0P2BASE::TemperatureC16Base *const _temperatureC16,
        const TempControlBase *const _tempControl,
        const OTV0P2BASE::PseudoSensorOccupancyTracker *const _occupancy,
        const OTV0P2BASE::SensorAmbientLightBase *const _ambLight,
        const ActuatorPhysicalUIBase *const _physicalUI,
        const OTV0P2BASE::SimpleValveScheduleBase *const _schedule,
        const OTV0P2BASE::NVByHourByteStatsBase *const _byHourStats,
        const bool _alwaysGlacial = false, const uint8_t _maxPCOpen = 100)
      : sensorCtrlStats(
          _vcp,
          _valveMode,
          _temperatureC16,
          _tempControl,
          _occupancy,
          _ambLight,
          _physicalUI,
          _schedule,
          _byHourStats
#if defined(ENABLE_SETBACK_LOCKOUT_COUNTDOWN) && defined(ARDUINO_ARCH_AVR)
          // If allowing setback lockout, eg for testing, then inject suitable lambda.
          , []{return(0xff != eeprom_read_byte((uint8_t *)OTV0P2BASE::V0P2BASE_EE_START_SETBACK_LOCKOUT_COUNTDOWN_D_INV));}
#endif
          ),
        alwaysGlacial(_alwaysGlacial), glacial(_alwaysGlacial),
        maxPCOpen(OTV0P2BASE::fnmin(_maxPCOpen, (uint8_t)100U)),
        valveModeRW(_valveMode)
      { }

    // Force a read/poll/recomputation of the target position and call for heat.
    // Sets/clears changed flag if computed valve position changed.
    // Call at a fixed rate (1/60s).
    // Potentially expensive/slow.
    virtual uint8_t read() override { computeCallForHeat(); return(value); }

    // Returns preferred poll interval (in seconds); non-zero.
    // Must be polled at near constant rate, about once per minute.
    virtual uint8_t preferredPollInterval_s() const override{ return(60); }

    // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
    // The lifetime of the pointed-to text must be at least that of the Sensor instance.
    virtual const char *tag() const override { return("v|%"); }

    // Returns true if (re)calibrating/(re)initialising/(re)syncing.
    // The target valve position is not lost while this is true.
    // By default there is no recalibration step.
    virtual bool isRecalibrating() const;

    // If possible exercise the valve to avoid pin sticking and recalibrate valve travel.
    // Default does nothing.
    virtual void recalibrate();

    // True if the controlled physical valve is thought to be at least partially open right now.
    // If multiple valves are controlled then is this true only if all are at least partially open.
    // Used to help avoid running boiler pump against closed valves.
    // The default is to use the check the current computed position
    // against the minimum open percentage.
    // True iff the valve(s) (if any) controlled by this unit are really open.
    //
    // When driving a remote wireless valve such as the FHT8V,
    // this waits until at least the command has been sent.
    // This also implies open to OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN or equivalent.
    // Must be exactly one definition/implementation supplied at link time.
    // If more than one valve is being controlled by this unit,
    // then this should return true if any of the valves are (significantly) open.
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
    // For heat-pump, district-heating and similar slow-reponse and pay-by-volume environments.
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

    // Compute and return target (usually room) temperature (stateless).
    // Computes the target temperature based on various sensors, controls and stats.
    // Can be called as often as required though may be slow/expensive.
    // Will be called by computeCallForHeat().
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
    static uint8_t computeTargetTemp(const ModelledRadValveSensorCtrlStats &sensorCtrlStats);

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
    void computeTargetTemperature();

    // Computes optimal valve position given supplied input state including current position; [0,100].
    // Uses no state other than that passed as the arguments (thus unit testable).
    // This supplied 'retained' state may be updated.
    // Uses hysteresis and a proportional control and other cleverness.
    // Is always willing to turn off quickly, but on slowly (AKA "slow start" algorithm),
    // and tries to eliminate unnecessary 'hunting' which makes noise and uses actuator energy.
    // Nominally called at a regular rate, once per minute.
    // All inputState values should be set to sensible values before starting.
    // Usually called by tick() which does required state updates afterwards.
    static uint8_t computeRequiredTRVPercentOpen(uint8_t valvePCOpen, const struct ModelledRadValveInputState &inputState, struct ModelledRadValveState &retainedState);

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
    /*static*/ uint8_t getMinValvePcReallyOpen() const;

    // Set and cache minimum valve percentage open to be considered really open.
    // Applies to local valve and, at hub, to calls for remote calls for heat.
    // Any out-of-range value (eg >100) clears the override and OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN will be used.
    /*static*/ void setMinValvePcReallyOpen(uint8_t percent);
  };


    }

#endif
