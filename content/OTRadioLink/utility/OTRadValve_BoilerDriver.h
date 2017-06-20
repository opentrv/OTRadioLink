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
 * Smart driver for boiler.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_BOILERDRIVER_H
#define ARDUINO_LIB_OTRADVALVE_BOILERDRIVER_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include "OTRadValve_AbstractRadValve.h"


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {

/**
 * @brief   Manage hub mode and where getMinBoilerOnMinutes gets its values from.
 * @param   enableDefaultAlwaysRX:
 * @param   enableRadioRX:
 * @param   useEEPROM:
 * @todo    kill it with fire.
 */
template <bool enableDefaultAlwaysRX, bool enableRadioRX, bool useEEPROM = true>
class OTHubManager
{
private:
    // Default minimum on/off time in minutes for the boiler relay.
    // Set to 5 as the default valve Tx cycle is 4 mins and 5 mins is a good amount for most boilers.
    // This constant is necessary as if V0P2BASE_EE_START_MIN_BOILER_ON_MINS_INV is not set, the boiler relay will never be turned on.
    static constexpr uint8_t DEFAULT_MIN_BOILER_ON_MINS = 5;
public:
#ifdef ARDUINO_ARCH_AVR
    /**
     * @brief   Set minimum on (and off) time for pointer (minutes); zero to disable hub mode.
     * @note    Suggested minimum of 4 minutes for gas combi; much longer for heat pumps for example.
     *          Does nothing if not using EEPROM.
     */
    inline void setMinBoilerOnMinutes(uint8_t mins) {
        if(useEEPROM) { OTV0P2BASE::eeprom_smart_update_byte((uint8_t *)V0P2BASE_EE_START_MIN_BOILER_ON_MINS_INV, ~(mins)); }
    }  // FIXME seens to be unused! (DE20170602)
    /**
     * @brief   Get minimum on (and off) time for pointer (minutes); zero if not in hub mode.
     * @retval  value in EEPROM if useEEPROM true (by default) or use the DEFAULT_MIN_BOILER_ON_MINS value.
     */
    inline uint8_t getMinBoilerOnMinutes() {
        if(useEEPROM) { return (~eeprom_read_byte((uint8_t *)V0P2BASE_EE_START_MIN_BOILER_ON_MINS_INV)); }
        else { return (DEFAULT_MIN_BOILER_ON_MINS); }
    }
#else // ARDUINO_ARCH_AVR
    // Assume no EEPROM on non AVR arch.
    inline void setMinBoilerOnMinutes(uint8_t /*mins*/) {};
    inline uint8_t getMinBoilerOnMinutes() { return DEFAULT_MIN_BOILER_ON_MINS; }
#endif // ARDUINO_ARCH_AVR

    /**
     * @brief   Check if unit should be in hub mode (Boiler/relay functions enabled).
     * @retval  Returns true if unit should be in central hub/listen mode.
     *          - Unit should always be in hub mode if enableDefaultAlwaysRX is true
     *          - Unit should never be in hub mode if enableRadioRX is not true.
     *          - True if in central hub/listen mode (possibly with local radiator also). FIXME Not sure about last case..
     */
    inline bool inHubMode()
    {
        if (enableDefaultAlwaysRX) { return (true); }
        else if (!enableRadioRX) {return (false); }
        else { return (0 != getMinBoilerOnMinutes()); }
    }
    /**
     * @brief   Check if unit should be in hub mode (Boiler/relay functions enabled).
     * @retval  Returns true if unit should be in stats hub/listen mode.
     *          - Unit should always be in hub mode if enableDefaultAlwaysRX is true
     *          - Unit should never be in hub mode if enableRadioRX is not true.
     *          - True if in stats hub/listen mode (minimum timeout). FIXME Not sure about last case..
     */
    inline bool inStatsHubMode()
    {
        if (enableDefaultAlwaysRX) { return (true); }
        else if (!enableRadioRX) {return (false); }
        else { return (1 == getMinBoilerOnMinutes()); }
    }
};

namespace BoilerLogic
{
/**Manages simple binary (on/off) boiler.
 * @param   outHeatCall: GPIO pin to call for heat on (high/1 => call for heat)
 * @param   forceMinBoilerOnTime: Forces boiler to use the default minimum on time rather than the value stored in
 *          EEPROM. If this is set to false and V0P2BASE_EE_START_MIN_BOILER_ON_MINS_INV is not set, the boiler will
 *          never activate.
 *          Defaults true so that the boiler driver will always work unless explicitly overridden..
 * @param   isRadValve: Unit is controlling a rad valve (local or remote).
 * @note    (DE20170602) Removed support for:
 *          - case where unit is a boilerhub controller and also a TRV.
 *          - dealing with ID of caller for heat (was redundant in old implementation anyway).
 * @note Not ISR-/thread- safe; do not call from ISR RX.
 * @note: DHD20170614: TODO: refactor as an Actuator.
 */
template<typename hm_t, hm_t &hm,
         uint8_t outHeatCallPin, bool forceMinOnBoilerTime = true, bool isRadValve = false>
class OnOffBoilerDriverLogic
{
private:
    // Set true on receipt of plausible call for heat,
    // to be polled, evaluated and cleared by the main control routine.
    // Atomic to allow thread-safe lock-free access.
    bool callForHeatRX = false;

    // Minutes that the boiler has been off for, allowing minimum off time to be enforced.
    // Does not roll once at its maximum value (255).
    // DHD20160124: starting at zero forces at least for off time after power-up before firing up boiler (good after power-cut).
    uint8_t boilerNoCallM = 0;
    // Reducing listening if quiet for a while helps reduce self-heating temperature error
    // (~2C as of 2013/12/24 at 100% RX, ~100mW heat dissipation in V0.2 REV1 box) and saves some energy.
    // Time thresholds could be affected by eco/comfort switch.
    //#define RX_REDUCE_MIN_M 20 // Minimum minutes quiet before considering reducing RX duty cycle listening for call for heat; [1--255], 10--60 typical.
    // IF DEFINED then give backoff threshold to minimise duty cycle.
    //#define RX_REDUCE_MAX_M 240 // Minutes quiet before considering maximally reducing RX duty cycle; ]RX_REDUCE_MIN_M--255], 30--240 typical.

    // Ticks until locally-controlled boiler should be turned off; boiler should be on while this is positive.
    // Ticks are of the main loop, ie 2s (almost always).
    // Used in hub mode only.
    // DHD20170714: FIXME: for non-heat-pump-type system, 255 ticks (thus uint8_t) should suffice.
    uint16_t boilerCountdownTicks = 0;

    // Get minimum valve open value.
    // NOTE: Case where boiler hub also controls a radvalve is not implemented.
    inline uint8_t getMinValveReallyOpen() {
        constexpr uint8_t default_minimum = OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN;
//        if (isRadValve) {
//            return OTV0P2BASE::fnmax(default_minimum, rv.getMinValvePcReallyOpen());
//        } else {
//            return default_minimum;
//        }
        return default_minimum;
    }

public:
    // True if boiler should be on.
    inline bool isBoilerOn() { return(0 != boilerCountdownTicks); }

    // Raw notification of received call for heat from remote (eg FHT8V) unit.
    // This form has a 16-bit ID (eg FHT8V housecode) and percent-open value [0,100].
    // Note that this may include 0 percent values for a remote unit explicitly confirming
    // that is is not, or has stopped, calling for heat (eg instead of replying on a timeout).
    // This is not filtered, and can be delivered at any time from RX data, from a non-ISR thread.
    // Does not have to be thread-/ISR- safe.
    // Note: callForHeat ID functionality disabled as unused anyway. API is preserved. (DE20170602)
    void remoteCallForHeatRX(const uint16_t /*id*/, const uint8_t percentOpen, const uint8_t minuteCount)
    {
        #if 1
        OTV0P2BASE::MemoryChecks::recordIfMinSP();
        #endif
        // TODO: Should be filtering first by housecode
        // then by individual and tracked aggregate valve-open percentage.
        // Only individual valve levels used here; no state is retained.

        // Normal minimum single-valve percentage open that is not ignored.
        // Somewhat higher than typical per-valve minimum,
        // to help provide boiler with an opportunity to dump heat before switching off.
        // May be too high to respond to valves with restricted max-open / range.
        const uint8_t minvro = getMinValveReallyOpen();

        // TODO-553: after over an hour of continuous boiler running
        // raise the percentage threshold to successfully call for heat (for a while).
        // The aim is to allow a (combi) boiler to have reached maximum efficiency
        // and to have potentially made a significant difference to room temperature
        // but then turn off for a short while if demand is a little lower
        // to allow it to run a little harder/better when turned on again.
        // Most combis have power far higher than needed to run rads at full blast
        // and have only limited ability to modulate down,
        // so may end up cycling anyway while running the circulation pump if left on.
        // Modelled on DHD habit of having many 15-minute boiler timer segments
        // in 'off' period even during the day for many many years!
        //
        // Note: could also consider pause if mains frequency is low indicating grid stress.
        const uint8_t boilerCycleWindowMask = 0x3f;
        const uint8_t boilerCycleWindow = (minuteCount & boilerCycleWindowMask);
        const bool considerPause = (boilerCycleWindow < (boilerCycleWindowMask >> 2));

        // Equally the threshold could be lowered in the period after a possible pause (TODO-593, TODO-553)
        // to encourage the boiler to start and run harder
        // and to get a little closer to target temperatures.
        const bool encourageOn = !considerPause && (boilerCycleWindow < (boilerCycleWindowMask >> 1));

        // TODO-555: apply some basic hysteresis to help reduce boiler short-cycling.
        // Try to force a higher single-valve-%age threshold to start boiler if off,
        // at a level where at least a single valve is moderately open.
        // Selecting "quick heat" at a valve should immediately pass this,
        // as should normal warm in cold but newly-occupied room (TODO-593).
        // (This will not provide hysteresis for very high minimum really-open valve values.)
        // Be slightly tolerant with the 'moderately open' threshold
        // to allow quick start from a range of devices (TODO-593)
        // and in the face of imperfect rounding/conversion to/from percentages over the air.
        const uint8_t threshold = (!considerPause && (encourageOn || isBoilerOn())) ?
            minvro : OTV0P2BASE::fnmax(minvro, (uint8_t) (OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN-1));

        if(percentOpen >= threshold) {
        // && FHT8VHubAcceptedHouseCode(command.hc1, command.hc2))) // Accept if house code OK.
            callForHeatRX = true;
        }
    }

    // Process calls for heat, ie turn boiler on and off as appropriate.
    // Has control of OUT_HEATCALL if defined(ENABLE_BOILER_HUB).
    // Called every tick (typically 2s); drives timing.
    void processCallsForHeat(const bool second0, const bool hubMode)
    {
        #if 1
        OTV0P2BASE::MemoryChecks::recordIfMinSP();
        #endif
        if(hubMode) {
            // Check if call-for-heat has been received, and clear the flag.
            // Record call for heat, both to start boiler-on cycle and possibly to defer need to listen again.
            // Ignore new calls for heat until minimum off/quiet period has been reached.
            // Possible optimisation: may be able to stop RX if boiler is on for local demand (can measure local temp better: less self-heating) and not collecting stats.
            if(callForHeatRX) {
                callForHeatRX = false;
                const uint8_t minOnMins = hm.getMinBoilerOnMinutes();
                bool ignoreRCfH = false;
                if(!isBoilerOn()) {
                    // Boiler was off.
                    // Ignore new call for heat if boiler has not been off long enough,
                    // forcing a time longer than the specified minimum,
                    // regardless of when second0 happens to be.
                    // (The min(254, ...) is to ensure that the boiler can come on even if minOnMins == 255.)
                    // TODO: randomly extend the off-time a little (eg during grid stress) partly to randomise whole cycle length.
                    if(boilerNoCallM <= OTV0P2BASE::fnmin((uint8_t)254, minOnMins)) { ignoreRCfH = true; }
            //        if(OTV0P2BASE::getSubCycleTime() >= nearOverrunThreshold) { } // { tooNearOverrun = true; }
            //        else
                      if(ignoreRCfH) { OTV0P2BASE::serialPrintlnAndFlush(F("RCfH-")); } // Remote call for heat ignored.
                    else { OTV0P2BASE::serialPrintlnAndFlush(F("RCfH1")); } // Remote call for heat on.
                }
                if(!ignoreRCfH) {
                    const uint16_t onTimeTicks = minOnMins * (uint16_t) (60U / OTV0P2BASE::MAIN_TICK_S);
                    // Restart count-down time (keeping boiler on) with new call for heat.
                    boilerCountdownTicks = onTimeTicks;
                    boilerNoCallM = 0; // No time has passed since the last call.
                }
            }

            // If boiler is on, then count down towards boiler off.
            if(isBoilerOn()) {
                if(0 == --boilerCountdownTicks) {
                    // Boiler should now be switched off.
            //        if(OTV0P2BASE::getSubCycleTime() >= nearOverrunThreshold) { } // { tooNearOverrun = true; }
            //        else
                        { OTV0P2BASE::serialPrintlnAndFlush(F("RCfH0")); } // Remote call for heat off
                }
            }
            // Else boiler is off so count up quiet minutes until at max...
            else if(second0 && (boilerNoCallM < 255))
                { ++boilerNoCallM; }

            // Set BOILER_OUT as appropriate for calls for heat.
            // Local calls for heat come via the same route (TODO-607).
#ifdef ARDUINO_ARCH_AVR
            fastDigitalWrite(outHeatCallPin, (isBoilerOn() ? HIGH : LOW));
#endif // ARDUINO_ARCH_AVR
        } else {
#ifdef ARDUINO_ARCH_AVR
            // Force boiler off when not in hub mode.
            fastDigitalWrite(outHeatCallPin, LOW);
#endif // ARDUINO_ARCH_AVR
        }
    }
};

//// Smarter logic for simple on/off boiler output, fully testable.
//class OnOffBoilerDriverLogic
//  {
//  public:
//    // Maximum distinct radiators tracked by this system.
//    // The algorithms uses will assume that this is a smallish number,
//    // and may work best of a power of two.
//    // Reasonable candidates are 8 or 16.
//    static const uint8_t maxRadiators = 8;
//
//    // Per-radiator data status.
//    struct PerIDStatus
//      {
//      // ID of remote device; an empty slot is marked with the (invalid) ID of 0xffffu.
//      uint16_t id;
//      // Ticks until we expect new update from given valve else its data has expired when non-negative.
//      // A zero or negative value means no active call for heat from the matching ID.
//      // A negative value indicates that a signal is overdue by that number of ticks
//      // or units greater than ticks to allow old entries to linger longer for tracking or performance reasons.
//      // An empty slot is given up for an incoming new ID calling for heat.
//      int8_t ticksUntilOff;
//      // Last percent open for given valve.
//      // A zero value means no active call for heat from the matching ID.
//      // An empty slot is given up for an incoming new ID calling for heat.
//      uint8_t percentOpen;
//      };
//
//  private:
//    // True to call for heat from the boiler.
//    bool callForHeat;
//
//    // Number of ticks that boiler has been in current state, on or off, to avoid short-cycling.
//    // The state cannot be changed until the specified minimum off/on ticks have been passed.
//    // (This rule may be ignored when the boiler is currently on if all valves seem to have been turned off prematurely.)
//    // This value does not roll back round to zero, ie will stop at maximum until reset.
//    // The max representable value allows for several hours at 1 or 2 seconds per tick.
//    uint8_t ticksInCurrentState;
//
//    // Ticks minimum for boiler to stay in each state to avoid short-cycling; should be significantly positive but won't fail if otherwise.
//    // Typically the equivalent of 2--10 minutes.
//    uint8_t minTicksInEitherState;
//
//    // Minimum individual valve percentage to be considered open [1,100].
//    uint8_t minIndividualPC;
//
//    // Minimum aggregate valve percentage to be considered open, no lower than minIndividualPC; [1,100].
//    uint8_t minAggregatePC;
//
//    // 'Bad' (never valid as housecode or OpenTRV code) ID.
//    static const uint16_t badID = 0xffffu;
//
//#if defined(BOILER_RESPOND_TO_SPECIFIED_IDS_ONLY)
//    // List of authorised IDs.
//    // Entries beyond the last valid authorised ID are set to 0xffffu (never a valid ID);
//    // if there are none then [0] is set to 0xffffu.
//    // If no authorised IDs then no authorisation is done and all IDs are accepted
//    // and kicked out expiry-first if there is a shortage of slots.
//    uint16_t authedIDs[maxRadiators];
//#endif
//
//    // List of recently-heard IDs and ticks until their last call for heat will expire (matched by index).
//    // If there any authorised IDs then only such IDs are admitted to the list.
//    // The first empty empty slot is marked with the (invalid) ID of 0xffffu.
//    // Marked volatile since may be updated by ISR.
//    volatile PerIDStatus status[maxRadiators];
//
//#if defined(OnOffBoilerDriverLogic_CLEANUP)
//    // Do some incremental clean-up to speed up future operations.
//    // Aim to free up at least one status slot if possible.
//    void cleanup();
//#endif
//
//  public:
//    OnOffBoilerDriverLogic() : callForHeat(false), ticksInCurrentState(0), minTicksInEitherState(60), minIndividualPC(OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN), minAggregatePC(OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN)
//      {
//#if defined(BOILER_RESPOND_TO_SPECIFIED_IDS_ONLY)
//      authedIDs[0] = badID;
//#endif
//      status[0].id = badID;
//      }
//
//    // Set thresholds for per-value and minimum-aggregate percentages to fire the boiler.
//    // Coerces values to be valid:
//    // minIndividual in range [1,100] and minAggregate in range [minIndividual,100].
//    void setThresholds(uint8_t minIndividual, uint8_t minAggregate);
//
//    // Set minimum ticks for boiler to stay in each state to avoid short-cycling; should be significantly positive but won't fail if otherwise.
//    // Typically the equivalent of 2--10 minutes (eg ~2+ for gas, ~8 for oil).
//    void setMinTicksInEitherState(uint8_t minTicks) { minTicksInEitherState = minTicks; }
//
//    // Called upon incoming notification of status or call for heat from given (valid) ID.
//    // ISR-/thread- safe to allow for interrupt-driven comms, and as quick as possible.
//    // Returns false if the signal is rejected, eg from an unauthorised ID.
//    // The basic behaviour is that a signal with sufficient percent open
//    // is good for 2 minutes (120s, 60 ticks) unless explicitly cancelled earlier,
//    // for all valve types including FS20/FHT8V-style.
//    // That may be slightly adjusted for IDs that indicate FS20 housecodes, etc.
//    //   * id  is the two-byte ID or house code; 0xffffu is never valid
//    //   * percentOpen  percentage open that the remote valve is reporting
//    bool receiveSignal(uint16_t id, uint8_t percentOpen);
//
//    // Iff true then call for heat from the boiler.
//    bool isCallingForHeat() const { return(callForHeat); }
//
//    // Poll every 2 seconds in real/virtual time to update state in particular the callForHeat value.
//    // Not to be called from ISRs,
//    // in part because this may perform occasional expensive-ish operations
//    // such as incremental clean-up.
//    // Because this does not assume a tick is in real time
//    // this remains entirely unit testable,
//    // and no use of wall-clock time is made within this or sibling class methods.
//    void tick2s();
//
//    // Fetches statuses of valves recently heard from and returns the count; 0 if none.
//    // Optionally filters to return only those still live and apparently calling for heat.
//    //   * valves  array to copy status to the start of; never null
//    //   * size  size of valves[] in entries (not bytes), no more entries than that are used,
//    //     and no more than maxRadiators entries are ever needed
//    //   * onlyLiveAndCallingForHeat  if true retrieves only current entries
//    //     'calling for heat' by percentage
//    uint8_t valvesStatus(PerIDStatus valves[], uint8_t size, bool onlyLiveAndCallingForHeat) const;
//  };
//
//
//// Boiler output control (call-for-heat driver).
//// Nominally drives on scale of [0,100]%
//// but any non-zero value should be regarded as calling for heat from an on/off boiler,
//// and only values of 0 and 100 may be produced.
//// Implementations require poll() called at a fixed rate (every 2s).
//class BoilerDriver : public OTV0P2BASE::SimpleTSUint8Actuator
//  {
//  private:
//    OnOffBoilerDriverLogic logic;
//
//  public:
//    // Regular poll/update.
//    virtual uint8_t read();
//
//    // Preferred poll interval (2 seconds).
//    virtual uint8_t preferredPollInterval_s() const { return(2); }
//
//  };


    }
    }
#endif
