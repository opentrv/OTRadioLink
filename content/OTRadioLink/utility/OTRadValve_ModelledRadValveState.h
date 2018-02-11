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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2018
                           Deniz Erbilgin   2017
*/

#ifndef UTILITY_OTRADVALVE_MODELLEDRADVALVESTATE_H_
#define UTILITY_OTRADVALVE_MODELLEDRADVALVESTATE_H_

namespace OTRadValve
{

// Simple mean filter.
// Find mean of group of ints where sum can be computed in an int without loss.
// TODO: needs a unit test or three.
template<size_t N> int_fast16_t smallIntMean(const int_fast16_t data[N])
  {
  // Extract mean.
  // Assume values and sum will be nowhere near the limits.
  int_fast16_t sum = 0;
  for(int8_t i = N; --i >= 0; ) { sum += data[i]; }
  // Compute rounded-up mean.
  // Avoid accidental computation as unsigned...
  return((sum + (int_fast16_t)(N/2)) / (int_fast16_t)N);
  }

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
//
// Template parameters:
//     MINIMAL_BINARY_IMPL  ff true then support a minimal/binary valve impl
template <bool MINIMAL_BINARY_IMPL = false>
struct ModelledRadValveState final
{
    // FEATURE SUPPORT
    // If true then support proportional response in target 1C range.
    static constexpr bool SUPPORT_PROPORTIONAL = !MINIMAL_BINARY_IMPL;
    // If true then detect drafts from open windows and doors.
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
    ModelledRadValveState(const ModelledRadValveInputState &inputState, bool _alwaysGlacial = false) :
                alwaysGlacial(_alwaysGlacial),
                initialised(true)
    {
        // Fills array exactly as tick() would when !initialised.
        const int_fast16_t rawTempC16 = computeRawTemp16(inputState);
        _backfillTemperatures(rawTempC16);
    };

    // Perform per-minute tasks such as counter and filter updates then recompute valve position.
    // The input state must be complete including target/reference temperatures
    // before calling this including the first time
    // whereupon some further lazy initialisation is done.
    //   * valvePCOpenRef  current valve position UPDATED BY THIS CALL;
    //         in range [0,100]
    //   * inputState  immutable input state reference
    //   * physicalDeviceOpt  physical device to set() target open %
    //         with new target, if non-NULL
    // If the physical device is provided then its target will be set()
    // and its actual value will be monitored for cumulative movement,
    // else if not provided the movement in valvePCOpenRef
    // will be monitored/tracked instead.
    void tick(volatile uint8_t &valvePCOpenRef,
            const ModelledRadValveInputState &inputState,
            AbstractRadValve *const physicalDeviceOpt)
    {
        // Forget last event if any.
        clearEvent();

        // Ensure that the filter is longer than turn-about delays
        // to try to ensure that there is some chance of smooth control.
        static_assert(DEFAULT_ANTISEEK_VALVE_REOPEN_DELAY_M < filterLength, "reduce overshoot/whiplash");
        static_assert(DEFAULT_ANTISEEK_VALVE_RECLOSE_DELAY_M < filterLength, "reduce overshoot/whiplash");

        const int_fast16_t rawTempC16 = computeRawTemp16(inputState); // Remove adjustment for target centre.
        // Do some one-off work on first tick in new instance.
        if(!initialised) {
            // Fill the filter memory with the current room temperature.
            _backfillTemperatures(rawTempC16);
            // Also capture the current/initial valve position as passed in.
            prevValvePC = (NULL == physicalDeviceOpt) ? valvePCOpenRef :
                physicalDeviceOpt->get();
            initialised = true;
        }

        // Shift in the latest (raw) temperature.
        for(int_fast8_t i = filterLength; --i > 0; ) { prevRawTempC16[i] = prevRawTempC16[i-1]; }
        prevRawTempC16[0] = rawTempC16;

        // Disable/enable filtering.
        static constexpr uint8_t filter_minimum_ON =
          SUPPORT_LONG_FILTER ? (4 * filterLength) : 1;
        static constexpr uint8_t filter_OFF = 0;
        // Exit from filtering:
        // if the raw value is close enough to the current filtered value
        // that reverting to unfiltered would not itself cause a big jump.
        // Only test this if the filter minimum on-time has expired.
        if(isFiltering) {
            // Count down until ready to test for filter exit.
            if(SUPPORT_LONG_FILTER && (isFiltering > 1))
              { --isFiltering; }
            else
              { if(OTV0P2BASE::fnabsdiff(getSmoothedRecent(), rawTempC16) <= MAX_TEMP_JUMP_C16) { isFiltering = filter_OFF; } }
        }
        // Force filtering (back) on if big delta(s) over recent minutes.
        // This is NOT an else clause from the above so as to avoid flapping
        // filtering on and off if the current temp happens to be close to the mean,
        // which would produce more valve movement and noise than necessary.  (TODO-1027)
        if(!isFiltering) {
            static_assert(MIN_TICKS_0p5C_DELTA < filterLength, "filter must be long enough to detect delta over specified window");
            static_assert(MIN_TICKS_1C_DELTA < filterLength, "filter must be long enough to detect delta over specified window");
            // Quick test for needing filtering turned on.
            // Switches on filtering if large delta over recent interval(s).
            // This will happen for all-in-one TRV on rad, as rad warms up, for example,
            // and forces on low-pass filter to better estimate real room temperature.
            if((OTV0P2BASE::fnabs(getRawDelta(MIN_TICKS_0p5C_DELTA)) > 8))
            //       (OTV0P2BASE::fnabs(getRawDelta(MIN_TICKS_1C_DELTA)) > 16) ||
            //       (OTV0P2BASE::fnabs(getRawDelta(filterLength-1)) > int_fast16_t(((filterLength-1) * 16) / MIN_TICKS_1C_DELTA)))
              { isFiltering = filter_minimum_ON; }
        }
        if(FILTER_DETECT_JITTER && !isFiltering) {
            // Force filtering (back) on if adjacent readings are wildly different.
            // Slow/expensive test if temperature readings are jittery.
            // It is not clear how often this will be the case with good sensors.
            for(size_t i = 1; i < filterLength; ++i)
              { if(OTV0P2BASE::fnabsdiff(prevRawTempC16[i], prevRawTempC16[i-1]) > MAX_TEMP_JUMP_C16) { isFiltering = filter_minimum_ON; break; } }
        }

        // Count down timers.
        if(valveTurndownCountdownM > 0) { --valveTurndownCountdownM; }
        if(valveTurnupCountdownM > 0) { --valveTurnupCountdownM; }

        // Update the modelled state including the valve position
        // passed by reference.
        const uint8_t oldValvePC = prevValvePC;
        const uint8_t oldModelledValvePC = valvePCOpenRef;
        const uint8_t newModelledValvePC =
          computeRequiredTRVPercentOpen(valvePCOpenRef, inputState);
        const bool modelledValveChanged =
            (newModelledValvePC != oldModelledValvePC);
        if(modelledValveChanged) {
            // Defer re-closing valve to avoid excessive hunting.
            if(newModelledValvePC > oldModelledValvePC) { valveTurnup(); }
            // Defer re-opening valve to avoid excessive hunting.
            else { valveTurndown(); }
            valvePCOpenRef = newModelledValvePC;
        }
        // For cumulative movement tracking
        // use the modelled value by default
        // if no physical device available.
        uint8_t newValvePC = newModelledValvePC;
        if(NULL != physicalDeviceOpt) {
            // Set the target for the physical device unconditionally
            // to ensure that the driver/device sees
            // (eg) the first such request
            // even if the modelled value does not change.
            physicalDeviceOpt->set(newModelledValvePC);
            // Look for change in the reported physical
            // device position immediately,
            // though visible change will usually require some time
            // eg for asynchronous motor activity,
            // so this is typically capturing movements
            // up to just before the set().
            newValvePC = physicalDeviceOpt->get();
        }
        cumulativeMovementPC =
          (cumulativeMovementPC + OTV0P2BASE::fnabsdiff(oldValvePC, newValvePC))
          & MAX_CUMULATIVE_MOVEMENT_VALUE;
        prevValvePC = newValvePC;
        valveMoved = modelledValveChanged;
    }


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
    // is assumed to be ~600% (DHD20171118), was ~400% (DHD20141230),
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
    int_fast16_t getSmoothedRecent() const
        { return(smallIntMean<filterLength>(prevRawTempC16)); }

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

// Computes a new valve position given supplied input state
// including the current valve position; [0,100].
// Uses no state other than that passed as arguments (thus is unit testable).
// Does not alter any of the input state.
// Uses hysteresis and a proportional control and some other cleverness.
// Should be called at a regular rate, once per minute.
// All inputState values should be set to sensible values before starting.
// Usually called by tick() which does required state updates afterwards.
//
// In a basic binary "bang-bang" mode the valve is operated fully on or off.
// This may make sense where, for example, the radiator is instant electric.
// The top of the central range is as for proportional,
// and the bottom of the central range is 1C or 2C below.
//
// Basic strategy for proportional control:
//   * The aim is to stay within and at the top end of the 'target' 1C band.
//   * The target 1C band is offset so that at a nominal XC.
//     temperature should be held somewhere between X.0C and X.5C.
//   * There is an outer band which when left has the valve immediately
//     completely opens or shuts as in binary mode, as an end stop on
//     behaviour.
//   * The outer band is wide, even without a wide deadband,
//     to allow the valve not necessarily to be immediately pushed
//     to end stops even when switching between setback levels,
//     and to allow temporary overshoot when the temperature sensor
//     is close to the heater for all-in-one TRVs for example.
//   * When dark or unoccupied or otherwise needing to be quiet
//     the temperature is allowed to drift in a somewhat wider band
//     to reduce valve movement and noise (and battery consumption)
//     and boiler running and energy consumption and noise.
//   * When the device sees rapid temperature movements,
//     eg for an all-in-one TRV mounted on the radiator,
//     temporarily larger excursions are allowed.
//   * To save noise and battery life, and help avoid valve sticking,
//     the valve will lazily try to avoid unnecessary movement,
//     and avoid running further or faster than necessary.
//   * The valve will try to avoid calling for heat from the boiler
//     without being open enough to allow decent flow.
//   * The valve will try to avoid calling for heat indefinitely
//     with the valve static.  (TODO-1096)
//   * The valve may be held open without calling for heat
//     to help quietly scavenge heat if the boiler is already running.
//   * The valve will attempt to respond rapidly to (eg) manual controls
//     and new room occupancy.
//
// More detail:
//   * There is a 'sweet-spot' 0.5C wide in the target 1C;
//     wider but at the same centre with a wide deadband requested.
//   * Providing that there is no call for heat
//     then the valve can rest indefinitely at or close to the sweet-spot
//     ie avoid movement.
//   * Outside the sweet-spot the valve will always try to seek back to it,
//     either passively if the temperature is moving in the right direction,
//     or actively by adjusting the valve.
//   * Valve movement may be faster the further from the target/sweet-spot.
//   * The valve can be run in a glacial mode,
//     where the valve will always adjust at minimum speed,
//     to minimise flow eg where there is a charge by volume.
//   * In order to allow for valves only open enough at/near 100%,
//     and to reduce battery drain and valve wear/sticking,
//     the algorithm is biased towards fully opening but not fully closing.
//
uint8_t computeRequiredTRVPercentOpen(uint8_t valvePCOpen, const ModelledRadValveInputState &inputState) const
{
  // Possibly-adjusted and/or smoothed temperature to use for targeting.
  const int_fast16_t adjustedTempC16 = isFiltering ?
      (getSmoothedRecent() + ModelledRadValveInputState::refTempOffsetC16) :
      inputState.refTempC16;
  // When reduced to whole Celsius then fewer bits are needed
  // to cover the expected temperature range
  // and operations may be faster eg on 8-bit MCUs.
  const int_fast8_t adjustedTempC = (int_fast8_t) (adjustedTempC16 >> 4);

  // Be glacial if always so or temporarily requested to be so.
  const bool beGlacial = alwaysGlacial || inputState.glacial;

  // Heavily used fields broken out to potentially save read costs.
  const uint8_t tTC = inputState.targetTempC;
  const bool wide = inputState.widenDeadband;
  const bool worf = (wide || isFiltering);

  // Typical valve slew rate (percent/minute) when close to target temperature.
  // Keeping the slew small reduces noise and overshoot and surges of water
  // (eg for when additionally charged by volume in district heating systems)
  // and will likely work better with high-thermal-mass / slow-response systems
  // such as UFH,
  // but if too small then users will not get the quick-enough response.
  // Should be << 50%/min, and probably << 10%/min,
  // given that <30% may be the effective control range of many rad valves.
  // Typical mechanical TRVs have response times of ~20 minutes,
  // so aping that probably matches infrastructure and expectations best.
  static constexpr uint8_t TRV_SLEW_PC_PER_MIN = 5; // 20 mins full travel.
  // Derived from basic slew value...
//  // Slow.
//  static constexpr uint8_t TRV_SLEW_PC_PER_MIN_SLOW =
//      OTV0P2BASE::fnmax(1, TRV_SLEW_PC_PER_MIN/2);
  // Fast: takes <= fastResponseTicksTarget minutes for full travel.
  static constexpr uint8_t TRV_SLEW_PC_PER_MIN_FAST =
      uint8_t(1+OTV0P2BASE::fnmax(100/fastResponseTicksTarget,1+TRV_SLEW_PC_PER_MIN));
//  // Very fast: takes <= vFastResponseTicksTarget minutes for full travel.
//  static constexpr uint8_t TRV_SLEW_PC_PER_MIN_VFAST =
//      uint8_t(1+OTV0P2BASE::fnmax(100/vFastResponseTicksTarget,1+TRV_SLEW_PC_PER_MIN_FAST));

    // New non-binary implementation as of 2017Q1.
    // Does not make any particular assumptions about
    // at what percentage open significant/any water flow will happen,
    // but does take account of the main call-for-heat level for the boiler.
    //
    // Tries to avoid calling for heat longer than necessary,
    // ie with a valve open at/above OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN,
    // unless at max open so as to avoid futile/noisy/wasteful
    // continuous running of the boiler with the room temperature static
    // eg from a stuck valve; bursty is better for example.  (TODO-1096).
    //
    // Valve % does not correspond to temperature shortfall below target.

    // Possibly-higher upper limit, eg non-set-back temperature.
    const uint8_t higherTargetC =
        OTV0P2BASE::fnmax(tTC, inputState.maxTargetTempC);
    // (Well) under temperature target: open valve up.
    if(MINIMAL_BINARY_IMPL ? (adjustedTempC < tTC) :
        (adjustedTempC < OTV0P2BASE::fnmax(int(tTC) - int(_proportionalRange),
                                           int(OTRadValve::MIN_TARGET_C))))
        {
        // Don't open if recently turned down, unless in BAKE mode.
        if(dontTurnup() && !inputState.inBakeMode) { return(valvePCOpen); }
        if(!MINIMAL_BINARY_IMPL)
            {
            // Honour glacial restriction for opening if not binary.
            if(beGlacial) { if(valvePCOpen < inputState.maxPCOpen) { return(valvePCOpen + 1); } }
            }
        // Fully open immediately.
        setEvent(MRVE_OPENFAST);
        return(inputState.maxPCOpen);
        }
    // (Well) over temperature target: close valve down.
    // Allow more temporary headroom at the top than below with wide deadband
    // in proportional mode to try to allow graceful handling of overshoot
    // (eg where TRV on rad sees larger temperature swings vs eg split unit),
    // though central temperature target remains the same.
    //
    // When not in binary mode the temperature will be pushed down gently
    // even without a wide deadband when just above the central degree.
    else if(MINIMAL_BINARY_IMPL ? (adjustedTempC > tTC) :
        (adjustedTempC > OTV0P2BASE::fnmin(uint8_t(higherTargetC + _proportionalRange),
                                           OTRadValve::MAX_TARGET_C)))
        {
        // Don't close if recently turned up.
        if(dontTurndown()) { return(valvePCOpen); }
        // Fully close immediately.
        return(0);
        }
    // Else, if supporting proportional mode,
    // move the valve towards open/closed
    // modulating the speed of response depending on
    // wide deadband, etc.
    //
    // With a wide deadband far more over-/under- shoot is tolerated.
    // (The wider deadband should probably be enabled automatically
    // at a higher level when filtering has been engaged,
    // to deal more gracefully with wild temp swings for all-in-one design.)
    //
    // Managing to avoid having to run the valve entirely to the end stops,
    // especially fully-closed with spring-loaded TRV bases,
    // may save significant energy, noise and time.
    else if(!MINIMAL_BINARY_IMPL)
        {
        // In BAKE mode open immediately to maximum; only true rarely.
        if(BRANCH_HINT_unlikely(inputState.inBakeMode)) { return(inputState.maxPCOpen); }

        // Raw temperature error: amount ambient is above target (1/16C).
        static constexpr int8_t centreOffsetC16 = 12;
        const int_fast16_t errorC16 =
            adjustedTempC16 - (int_fast16_t(tTC) << 4) - centreOffsetC16;
        // True when below target, ie the error is negative.
        const bool belowTarget = (errorC16 < 0);

        // Leave valve as-is if blocked from moving in appropriate direction.
        if(belowTarget)
            { if(dontTurnup()) { return(valvePCOpen); } }
        else
            { if(dontTurndown()) { return(valvePCOpen); } }

        // Leave valve as-is if already at limit in appropriate direction.
        if(belowTarget)
            { if(valvePCOpen >= inputState.maxPCOpen) { return(valvePCOpen); } }
        else // Out of heating season will most likely stay at 0.
            { if(BRANCH_HINT_likely(0 == valvePCOpen)) { return(valvePCOpen); } }

        // When well off target then valve closing may be sped up.
        // Have a significantly higher ceiling if filtering,
        // eg because the sensor is near the heater;
        // also when a higher non set-back temperature is supplied
        // then any wide deadband is pushed up based on it.
        // Note that this very large band also applies for the wide deadband
        // in order to let the valve rest even while setbacks are applied.
        // Else a somewhat wider band (~1.5C) is allowed when requested.
        // Else a ~0.75C 'way off target' default band is used,
        // to surround the 0.5C normal sweet-spot.
        static constexpr uint8_t halfNormalBand = 6;
        // Basic behaviour is to double the deadband with wide or filtering.
        const int wOTC16basic = (worf ? (2*halfNormalBand) : halfNormalBand);
        // The expected excursion above the sweet-spot when filtering.
        // This takes into account that with a sensor near the radiator
        // the measured temperature will need to seem to overshoot the target
        // by this much to allow heat to be effectively pushed into the room.
        // This is set at up to around halfway to the outer/limit boundary
        // (though capped at an empirically-reasonable level);
        // far enough away to react in time to avoid breaching the outer
        // limit.
        static constexpr uint8_t wATC16 = OTV0P2BASE::fnmin(4 * 16,
            _proportionalRange * 4);
        // Filtering pushes limit up well above the target for all-in-1 TRVs,
        // though if sufficiently set back the non-set-back value prevails.
        // Keeps general wide deadband downwards-only to save some energy.
        const uint8_t wOTC16highSide = isFiltering ? wATC16 : halfNormalBand;
        const bool wellAboveTarget = errorC16 > wOTC16highSide;
        const bool wellBelowTarget = errorC16 < -wOTC16basic;
        // Same calc for herrorC16 as errorC16 but possibly not set back.
        // This allows the room temperature to fall passively during setback.
        const int_fast16_t herrorC16 = errorC16 -
            (int_fast16_t(higherTargetC - tTC) << 4);
        // True if well above the highest permitted (non-set-back)
        // temperature, allowing for filtering.
        // This is relative to (and above) the non-set-back temperature
        // to avoid the valve having to drift closed for no other reason
        // when the target temperature is set back
        // and this valve is not actually calling for heat.
        const bool wellAboveTargetMax = herrorC16 > wOTC16highSide;

        // Compute proportional slew rates to fix temperature errors.
        // Note that non-rounded shifts effectively set the deadband also.
        // Note that slewF == 0 in central sweet spot / deadband.
        static constexpr uint8_t worfErrShift = 3;
        const uint8_t errShift = worf ? worfErrShift : (worfErrShift-1);
        // Fast slew when responding to manual control or similar.
        const uint8_t slewF = OTV0P2BASE::fnmin(TRV_SLEW_PC_PER_MIN_FAST,
            uint8_t((errorC16 < 0) ?
                ((-errorC16) >> errShift) : (errorC16 >> errShift)));
        const bool inCentralSweetSpot = (0 == slewF);

        // Move quickly when requested, eg responding to manual control use.
        //
        // Also used when well below target to quickly open value up
        // and avoid getting caught with a flow too small to be useful,
        // eg just warming the all-in-one valve but not the room!
        // This ignores any current temperature fluctuations.
        // This asymmetry is needed because some valves
        // may not open significantly until near 100%.
        //
        // Get to right side of call-for-heat threshold in first tick
        // if not in central sweet-spot already  (TODO-1099)
        // to have boiler respond appropriately ASAP also.
        // As well as responding quickly thermally to requested changes,
        // this is about giving rapid confidence-building feedback to the
        // user.
        // Note that a manual adjustment of the temperature set-point
        // is very likely to force this unit out of the sweet-spot.
        //
        // Glacial mode must be set for valves with unusually small ranges,
        // as a guard to avoid large and out-of-range swings here.
        if(!beGlacial &&
           (inputState.fastResponseRequired || wellBelowTarget) &&
           (slewF > 0))
            {
            if(belowTarget)
                {
                // Default to safe and fast full open.
                // Aim to reduce movement by avoiding closing fast/fully.
                return(inputState.maxPCOpen);
//                static constexpr uint8_t minOpen = DEFAULT_VALVE_PC_MODERATELY_OPEN;
//                static constexpr uint8_t baseSlew = TRV_SLEW_PC_PER_MIN;
//                // Verify that there is theoretically time for
//                // a response from the boiler before hitting 100% open.
//                static_assert((100-minOpen) / (1+baseSlew) >= BOILER_RESPONSE_TIME_FROM_OFF,
//                    "should be time notionally to get a response from boiler "
//                    "before valve reaches 100% open");
//                return(OTV0P2BASE::fnconstrain(
//                    uint8_t(valvePCOpen + slewF + baseSlew),
//                    uint8_t(minOpen + TRV_SLEW_PC_PER_MIN),
//                    inputState.maxPCOpen));
                }
            else
                {
                // Immediately get below call-for-heat threshold on way down
                // but close at a rate afterwards such that full close
                // may not even be necessary after likely temporary overshoot.
                // Users are unlikely to mind cooling more slowly...
                // If temperature is well above target then shut fast
                // so as to not leave the user sweating for whatever reason.
                return(uint8_t(OTV0P2BASE::fnconstrain(
                    int(valvePCOpen) - int(slewF),
                    0,
                    int(OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN-1))));
                }
            }

        // True if the current valve open %age is also a boiler call for heat.
        const bool callingForHeat =
            (valvePCOpen >= OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN);

        // Check direction of latest raw temperature movement, if any.
        const int_fast16_t rise = getRawDelta();

        // Avoid movement to save valve energy and noise if ALL of:
        //   * not calling for heat (which also saves boiler energy and noise)
        //   * in sweet-spot OR not moving in the wrong direction.
        //   * not very far away from target
        if(!callingForHeat)
            {
            if(inCentralSweetSpot) { return(valvePCOpen); }
            else
                {
                // When below sweet-spot and not falling, hold valve steady.
                // If well below then hold steady only if temperature rising.
                if(belowTarget)
                    {
                    if(wellBelowTarget ? (rise > 0) : (rise >= 0))
                        { return(valvePCOpen); }
                    }
                // When above max sweet-spot and not rising, hold valve steady.
                // (Note that this is relative to the not-set-back deadband.)
                // If well above then hold steady only if temperature falling.
                // (Any rise will fall through and valve will close a little,
                // ie this will at least act to prevent temperature rise
                // and should help ratchet the temperature down.)
                // This could prevent the temperature falling to setback target,
                // eg because something else is keeping the boiler running
                // and this valve is still allowing some water through,
                // but the alternative is to allow intermittent valve creep,
                // eg all night, which could annoy users.  (TODO-1027)
                // Note that a noisy temperature sensor,
                // or a very draughty location, may force the valve to shut.
                // Generally temperatures will drop steadily
                // if heat input is needed but nothing else is calling for heat.
                // Thus the valve can stay put without significant risk
                // of failing to save expected energy
                // or (say) keeping users from sleeping by being too warm.
                else
                    {
                    if(wellAboveTargetMax ? (rise < 0) : (rise <= 0))
                        { return(valvePCOpen); }
                    }
                }
            }

        // Avoid fast movements if being glacial or in/near central sweet-spot.
        //
        // Glacial mode must be set for valves with unusually small ranges,
        // as a guard to avoid large swings here.
        if(!beGlacial)
            {
            // This handles being significantly over temperature and rising,
            // attempting to force a relatively rapid return to the target,
            // but not so rapid as to prematurely close the valve
            // implying excess noise and battery consumption.
            // (If well above target but not rising this will fall through
            // to the default glacial close.)
            //
            // This is dealing with being well above the current target,
            // including any setback in place, to ensure that the setback
            // is effective.
            //
            // Below this any residual error can be dealt with glacially.
            //
            // The 'well below' case is dealt elsewhere.
            if(wellAboveTarget && (rise > 0))
                {
                // Immediately stop calling for heat.
                static constexpr uint8_t maxOpen = DEFAULT_VALVE_PC_SAFER_OPEN-1;
                // Should otherwise close slow enough let the rad start to cool
                // before the valve completely closes,
                // ie to be able to ride out the rising temperature 'wave',
                // and get decent heat into a room,
                // but not egregiously overheat the room.
                //
                // Target time (minutes/ticks) to ride out the heat 'wave'.
                // This chance to close may start after the turndown delay.
                static constexpr uint8_t rideoutM = 20;
                // Computed slew: faster than glacial since temp is rising.
                static constexpr uint8_t maxSlew =
                    OTV0P2BASE::fnmax(2, maxOpen / rideoutM);
                // Verify that there is theoretically time for
                // a response from the boiler and the rad to start cooling
                // before the valve reaches 100% open.
                static_assert((maxOpen / maxSlew) > 2*DEFAULT_MAX_RUN_ON_TIME_M,
                    "should be time notionally for boiler to stop "
                    "and rad to stop getting hotter, "
                    "before valve reaches 0%");
                // Within bounds, attempt to fix faster when further off target
                // but not so fast as to force a full close unnecessarily.
                // Not calling for heat, so may be able to dawdle.
                // Note: even if slew were 0, it could not cause bad hovering,
                // because this also ensures that there is no call for heat.
                return(uint8_t(OTV0P2BASE::fnconstrain(
                    int(valvePCOpen) - int(maxSlew),
                    0,
                    int(maxOpen))));
                }
            }

        // Compute general need to open or close valve.
        // Both cannot be true at once.
        // Both can be false at once only when
        // the temperature is changing,
        // which prevents bad indefinite hovering.  (TODO-1096)
        // Implies delta T >= 60/16C ~ 4C per hour to avoid moving.
        // Only move if the temperature is not moving
        // in the right direction.
        const bool shouldOpen = belowTarget && (rise <= 0);
        const bool shouldClose = !belowTarget && (rise >= 0);

        // By default, move valve glacially to full open/closed.
        // Guards above ensure that these glacial movements are safe.
        // Aim to (efficiently) dither about the target,
        // with the aim of avoiding leaving the proportional range.
        // The valve does not hover mid-travel.  (TODO-1096)
        if(shouldClose) { return(valvePCOpen - 1); }
        else if(shouldOpen) { return(valvePCOpen + 1); }

        // Fall through to return valve position unchanged.
        }

    // Leave valve position unchanged.
    return(valvePCOpen);
    }

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
