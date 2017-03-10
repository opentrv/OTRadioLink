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
                           Deniz Erbilgin   2017
*/

#ifndef UTILITY_OTRADVALVE_MODELLEDRADVALVESTATE_H_
#define UTILITY_OTRADVALVE_MODELLEDRADVALVESTATE_H_

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

}




#endif /* UTILITY_OTRADVALVE_MODELLEDRADVALVESTATE_H_ */
