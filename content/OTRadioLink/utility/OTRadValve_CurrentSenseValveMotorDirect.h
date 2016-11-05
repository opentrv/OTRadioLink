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
                           Deniz Erbilgin 2016
*/

/*
 * Hardware-independent logic to control a hardware valve base with proportional control.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_CURRENTSENSEVALVEMOTORDIRECT_H_
#define ARDUINO_LIB_OTRADVALVE_CURRENTSENSEVALVEMOTORDIRECT_H_


#include <stdint.h>
#include "OTRadValve_AbstractRadValve.h"

namespace OTRadValve
{


// Generic (unit-testable) motor driver using end-stop detection and simple shaft-encoder.
// Designed to be embedded in a motor controller instance.
// This uses the sub-cycle clock for timing.
// This is sensitive to sub-cycle position, ie will try to avoid causing a main loop overrun.
// May report some key status on Serial, with any error line(s) starting with "!'.
#define CurrentSenseValveMotorDirect_DEFINED
class CurrentSenseValveMotorDirect final : public OTRadValve::HardwareMotorDriverInterfaceCallbackHandler
  {
  public:
    // Maximum time to move pin between fully retracted and extended and vv, seconds, strictly positive.
    // Set as a limit to allow a timeout when things go wrong.
    static const constexpr uint8_t MAX_TRAVEL_S = 4 * 60; // 4 minutes.

    // Assumed calls to read() before timeout (assuming o call each 2s).
    // If calls are received less often this will presumably take longer to perform movements,
    // so it is appropriate to use a 2s ticks approximation.
    static const constexpr uint8_t MAX_TRAVEL_WALLCLOCK_2s_TICKS = OTV0P2BASE::fnmax(4, MAX_TRAVEL_S / 2);

    // Time before starting to retract pint during initialisation, in seconds.
    // Long enough for to leave the CLI some time for setting things like setting secret keys.
    // Short enough not to be annoying waiting for the pin to retract before fitting a valve.
    static const constexpr uint8_t initialRetractDelay_s = 30;

    // Runtime for dead-reckoning adjustments (from stopped) (ms).
    // Smaller values nominally allow greater precision when dead-reckoning,
    // but may force the calibration to take longer.
    // Based on DHD20151020 DORM1 prototype rig-up and NiMH battery; 250ms+ seems good.
    static const constexpr uint8_t minMotorDRMS = 250;

    // Computes minimum motor dead reckoning ticks given approx ms per tick (pref rounded down).
    // Keep inline in the header to allow compile-time computation.
    //
    // Was:
    //static const constexpr uint8_t minMotorDRTicks = OTV0P2BASE::fnmax((uint8_t)1, (uint8_t)(minMotorDRMS / OTV0P2BASE::SUBCYCLE_TICK_MS_RD));
    static constexpr uint8_t computeMinMotorDRTicks(const uint8_t subcycleTicksRoundedDown_ms)
        { return(OTV0P2BASE::fnmax((uint8_t)1, (uint8_t)(minMotorDRMS / subcycleTicksRoundedDown_ms))); }

    // Computes absolute limit in sub-cycle beyond which motor should not be started.
    // This should allow meaningful movement and stop and settle and no sub-cycle overrun.
    // Allows for up to 120ms enforced sleep either side of motor run for example.
    // This should not be so greedy as to (eg) make the CLI unusable: 90% is pushing it.
    // Keep inline in the header to allow compile-time computation.
    //
    // Was:
    //static const constexpr uint8_t sctAbsLimit = OTV0P2BASE::GSCT_MAX -
    //    OTV0P2BASE::fnmax((uint8_t)1, (uint8_t)( ((OTV0P2BASE::GSCT_MAX+1)/4) - OTRadValve::ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks - 1 - (240 / OTV0P2BASE::SUBCYCLE_TICK_MS_RD) ));
    static constexpr uint8_t computeSctAbsLimit(const uint8_t subcycleTicksRoundedDown_ms,
                                                const uint8_t gcst_max,
                                                const uint8_t minimumMotorRunupTicks)
        {
        return(gcst_max -
               OTV0P2BASE::fnmax((uint8_t)1, (uint8_t)( ((gcst_max+1)/4) - minimumMotorRunupTicks - 1 - (240 / subcycleTicksRoundedDown_ms) )));
        }

    // Calibration parameters.
    // Data received during the calibration process,
    // and outputs derived from it.
    // Contains (unit-testable) computations.
    class CalibrationParameters final
        {
        private:
          // Data gathered during calibration process.
          // Ticks counted (sub-cycle ticks for complete run from fully-open to fully-closed, end-stop to end-stop).
          uint16_t ticksFromOpenToClosed;
          // Ticks counted (sub-cycle ticks for complete run from fully-closed to fully-open, end-stop to end-stop).
          uint16_t ticksFromClosedToOpen;

          // Computed parameters based on measurements during calibration process.
          // Approx precision in % as min ticks / DR size in range [1,100].
          uint8_t approxPrecisionPC;
          // A reduced ticks open/closed in ratio to allow small conversions.
          uint8_t tfotcSmall, tfctoSmall;

        public:
          // Minimum sub-cycle ticks for dead reckoning; strictly positive.
          const uint8_t minMotorDRTicks;

          // Construct an instance of calibration parameters.
          // Keep all the potentially slow calculations in-line here to allow them to be done at compile-time .
          CalibrationParameters(const uint8_t _minMotorDRTicks)
            : ticksFromOpenToClosed(0), ticksFromClosedToOpen(0),
              minMotorDRTicks(_minMotorDRTicks)
            { }

          // (Re)populate structure and compute derived parameters.
          // Ensures that all necessary items are gathered at once and none forgotten!
          // Returns true in case of success.
          // May return false and force error state if inputs unusable.
          bool updateAndCompute(uint16_t ticksFromOpenToClosed, uint16_t ticksFromClosedToOpen);

          // Get a ticks either way.
          inline uint16_t getTicksFromOpenToClosed() const { return(ticksFromOpenToClosed); }
          inline uint16_t getTicksFromClosedToOpen() const { return(ticksFromClosedToOpen); }

          // Approx precision in % as min ticks / DR size in range [0,100].
          // A return value of zero indicates that sub-percent precision is possible.
          inline uint8_t getApproxPrecisionPC() const { return(approxPrecisionPC); }

          // Get a reduced ticks open/closed in ratio to allow small conversions; at least a few bits.
          inline uint8_t getTfotcSmall() const { return(tfotcSmall); }
          inline uint8_t getTfctoSmall() const { return(tfctoSmall); }

          // Compute reconciliation/adjustment of ticks, and compute % position [0,100].
          // Reconcile any reverse ticks (and adjust with forward ticks if needed).
          // Call after moving the valve in normal mode.
          // Unit testable.
          uint8_t computePosition(volatile uint16_t &ticksFromOpen,
                                  volatile uint16_t &ticksReverse) const;
        };

  private:
    // Hardware interface instance, passed by reference.
    // Must have a lifetime exceeding that of this enclosing object.
    OTRadValve::HardwareMotorDriverInterface * const hw;

    // Pointer to function to get current sub-cycle time; never NULL.
    uint8_t (*const getSubCycleTimeFn)();

    // Minimum percent at which valve is usually open [1,00];
    const uint8_t minOpenPC;
    // Minimum percent at which valve is usually moderately open [minOpenPC+1,00];
    const uint8_t fairlyOpenPC;

    // Absolute limit in sub-cycle beyond which motor should not be started.
    // This should allow meaningful movement and stop and settle and no sub-cycle overrun.
    // Allows for up to 120ms enforced sleep either side of motor run for example.
    // This should not be so greedy as to (eg) make the CLI unusable: 90% is pushing it.
    const uint8_t sctAbsLimit;
//          static const constexpr uint8_t sctAbsLimit = OTV0P2BASE::GSCT_MAX -
//              OTV0P2BASE::fnmax((uint8_t)1, (uint8_t)( ((OTV0P2BASE::GSCT_MAX+1)/4) - OTRadValve::ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks - 1 - (240 / OTV0P2BASE::SUBCYCLE_TICK_MS_RD) ));
    // Absolute limit in sub-cycle beyond which motor should not be started for dead-reckoning pulse.
    // This should allow meaningful movement and no sub-cycle overrun.
    uint8_t computeSctAbsLimitDR() const { return(sctAbsLimit - cp.minMotorDRTicks); }

    // Callback returns true if unnecessary activity should be suppressed to avoid disturbing occupants; can be NULL.
    // Eg when room dark and occupants may be sleeping.
    bool (*const minimiseActivityOpt)() = ((bool(*)())NULL);
    // Allows monitoring of supply voltage to avoid some activities with low batteries; can be NULL.
    // Non-const to allow call to read() to force re-measurement of supply.
    OTV0P2BASE::SupplyVoltageLow *lowBattOpt = NULL;

  public:
    // Basic/coarse state of driver.
    // There may be microstates within most these basic states.
    //
    // Power-up sequence will often require something like:
    //   * withdrawing the pin completely (to make valve easy to fit)
    //   * waiting for some user activation step such as pressing a button to indicate valve fitted
    //   * running an initial calibration for the valve.
    //   * entering a normal state tracking the target %-open and periodically recalibrating/decalcinating.
    enum driverState
      {
      init = 0, // Power-up state.
      initWaiting, // Waiting to withdraw pin.
      valvePinWithdrawing, // Retracting pin at power-up.
      valvePinWithdrawn, // Allows valve to be fitted; wait for user signal that valve has been fitted.
      valveCalibrating, // Calibrating full valve travel.
      valveNormal, // Normal operating state: values lower than this indicate that power-up is not complete.
      valveDecalcinating, // TODO: running decalcination cycle (and can recalibrate and mitigate valve seating issues).
      valveError // Error state can only normally be cleared by power-cycling.
      };

  private:
    // Major state of driver.
    // On power-up (or full reset) should be 0/init.
    // Stored as a uint8_t to save a little space and to make atomic operations easier.
    // Marked volatile so that individual reads are ISR-/thread- safe without a mutex.
    // Hold a mutex to perform compound operations such as read/modify/write.
    // Change state with changeState() which will do some other book-keeping.
    volatile /*driverState*/ uint8_t state;
    // Change state and perform some book-keeping.
    inline void changeState(const driverState newState) { state = (uint8_t)newState; clearPerState(); }

    // Data used only within one major state and not needing to be saved between states.
    // Thus it can be shared in a union to save space.
    // This can be cleared to all zeros with clearPerState(), so starts each state zeroed.
    union
      {
      // State used while waiting to withdraw pin.
      struct { uint8_t ticksWaited; } initWaiting;
      // State used while calibrating.
      struct
        {
        uint8_t calibState; // Current micro-state, starting at zero.
//        uint8_t runCount; // Completed round-trip calibration runs.
        uint16_t ticksFromOpenToClosed;
        uint16_t ticksFromClosedToOpen;
        // Measure of real time spent trying in current microstate.
        uint8_t wallclock2sTicks; // read() calls counted at ~2s intervals.
        } valveCalibrating;
      // State used while valve pin is initially fully withdrawing.
      struct { uint8_t wallclock2sTicks; } valvePinWithdrawing;
      // State used while waiting for the valve to be fitted.
      struct { volatile bool valveFitted; } valvePinWithdrawn;
      } perState;
    inline void clearPerState() { if(sizeof(perState) > 0) { memset(&perState, 0, sizeof(perState)); } }

    // Flag set on signalHittingEndStop() callback from end-top / stall / high-current input.
    // Marked volatile for thread-safe lock-free access (with care).
    volatile bool endStopDetected;

    // Set when valve needs (re)calibration, eg because dead-reckoning found to be significantly wrong.
    // May also need recalibrating after (say) a few weeks to allow for battery/speed droop.
    bool needsRecalibrating = true;

    // Calibration parameters gathered/computed from the calibration step.
    // Logically read-only other than during (re)calibration.
    CalibrationParameters cp;

    // Current sub-cycle ticks from fully-open (reference) end of travel, towards fully closed.
    // This is nominally ticks in the open-to-closed direction
    // since those may differ from the other direction.
    // Reset during calibration and upon hitting an end-stop.
    // Recalibration, full or partial, may be forced if this overflows or underflows significantly.
    // Significant underflow might be (say) the minimum valve-open percentage.
    // ISR-/thread- safe with a mutex.
    volatile uint16_t ticksFromOpen;
    // Reverse ticks not yet folded into ticksFromOpen;
    volatile uint16_t ticksReverse;
    // Maximum permitted value of ticksFromOpen (and ticksReverse).
    static const uint16_t MAX_TICKS_FROM_OPEN = ~0;

    // Current nominal percent open in range [0,100].
    // Initialised to 'closed'.
    uint8_t currentPC = 0;

    // Target % open in range [0,100].
    // Initialised to partly open (but below 'call-for-heat'), as a safe frost-protection state.
    uint8_t targetPC = OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN-1;

    // True if using positional encoder, else using crude dead-reckoning.
    // Only defined once calibration is complete.
    bool usingPositionalEncoder() const { return(false); }

    // Run fast towards/to end stop as far as possible in this call.
    // Terminates significantly before the end of the sub-cycle.
    // Possibly allows partial recalibration, or at least re-homing.
    // Returns true if end-stop has apparently been hit,
    // else will require one or more further calls in new sub-cycles
    // to hit the end-stop.
    // May attempt to ride through stiff mechanics.
    bool runFastTowardsEndStop(bool toOpen);

    // Run at 'normal' speed towards/to end for a fixed time/distance.
    // Terminates significantly before the end of the sub-cycle.
    // Runs at same speed as during calibration.
    // Does the right thing with dead-reckoning and/or position detection.
    // Returns true if end-stop has apparently been hit.
    bool runTowardsEndStop(bool toOpen);

    // Run at 'normal' or speed or fast towards/to end for a fixed time/distance.
    // Terminates significantly before the end of the sub-cycle.
    // Runs at same speed as during calibration.
    // Does the right thing with dead-reckoning and/or position detection.
    // Returns true if end-stop has apparently been hit.
    bool runTowardsEndStop(bool toOpen, bool normal) { return(normal ? runTowardsEndStop(toOpen) : runFastTowardsEndStop(toOpen)); }

    // Compute and apply reconciliation/adjustment of ticks and % position.
    // Uses computePosition() to adjust internal state.
    // Call after moving the valve in normal mode.
    // Does nothing if calibration is not in place.
    void recomputePosition() { if(!needsRecalibrating) { currentPC = cp.computePosition(ticksFromOpen, ticksReverse); } }

    // Report an apparent serious tracking error that may need full recalibration.
    void reportTrackingError();

  public:
    // Create an instance, passing in a reference to the non-NULL hardware driver.
    // The hardware driver instance lifetime must be longer than this instance.
    //   * _getSubCycleTimeFn  pointer to function to get current sub-cycle time; never NULL
    //   * _minMotorDRTicks  minimum sub-cycle ticks for dead reckoning; strictly positive
    //   * _sctAbsLimit  absolute limit in sub-cycle beyond which motor should not be started
    //   * lowBattOpt  allows monitoring of supply voltage to avoid some activities with low batteries; can be NULL
    //   * minimiseActivityOpt  callback returns true if unnecessary activity should be suppressed
    //     to avoid disturbing occupants, eg when room dark and occupants may be sleeping; can be NULL
    // Keep all the potentially slow calculations in-line here to allow them to be done at compile-time .
    CurrentSenseValveMotorDirect(OTRadValve::HardwareMotorDriverInterface * const hwDriver,
                                 uint8_t (*_getSubCycleTimeFn)(),
                                 const uint8_t _minMotorDRTicks,
                                 const uint8_t _sctAbsLimit,
                                 OTV0P2BASE::SupplyVoltageLow *_lowBattOpt = NULL,
                                 bool (*const _minimiseActivityOpt)() = ((bool(*)())NULL),
                                 uint8_t _minOpenPC = OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN,
                                 uint8_t _fairlyOpenPC = OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN) :
        hw(hwDriver),
        getSubCycleTimeFn(_getSubCycleTimeFn),
        minOpenPC(_minOpenPC), fairlyOpenPC(_fairlyOpenPC),
        sctAbsLimit(_sctAbsLimit),
        minimiseActivityOpt(_minimiseActivityOpt), lowBattOpt(_lowBattOpt),
        cp(_minMotorDRTicks)
        { changeState(init); }

    // Poll.
    // Regular poll every 1s or 2s,
    // though tolerates missed polls eg because of other time-critical activity.
    // May block for hundreds of milliseconds.
    void poll();

    // Get major state, mostly for testing.
    driverState getState() const { return((driverState) state); }

    // Get current estimated actual % open in range [0,100].
    uint8_t getCurrentPC() const { return(currentPC); }

    // Get current target % open in range [0,100].
    uint8_t getTargetPC() const { return(targetPC); }

    // Set current target % open in range [0,100].
    // Coerced into range.
    void setTargetPC(uint8_t newPC) { targetPC = OTV0P2BASE::fnmin(newPC, (uint8_t)100); }

    // Get estimated minimum percentage open for significant flow for this device; strictly positive in range [1,99].
    virtual uint8_t getMinPercentOpen() const;

    // Minimally wiggle the motor to give tactile feedback and/or show to be working.
    // May take a significant fraction of a second.
    // Finishes with the motor turned off.
    virtual void wiggle();

    // Called when end stop hit, eg by overcurrent detection.
    // Can be called while run() is in progress.
    // Is ISR-/thread- safe.
    virtual void signalHittingEndStop(bool /*opening*/) override { endStopDetected = true; }

    // Called when encountering leading edge of a mark in the shaft rotation in forward direction (falling edge in reverse).
    // Can be called while run() is in progress.
    // Is ISR-/thread- safe.
    virtual void signalShaftEncoderMarkStart(bool /*opening*/) override { /* TODO */ }

    // Called with each motor run sub-cycle tick.
    // Is ISR-/thread- safe ***on AVR***.
    virtual void signalRunSCTTick(bool opening) override;

    // Call when given user signal that valve has been fitted (ie is fully on).
    virtual void signalValveFitted() { if(isWaitingForValveToBeFitted()) { perState.valvePinWithdrawn.valveFitted = true; } }

    // Waiting for indication that the valvehead  has been fitted to the tail.
    bool isWaitingForValveToBeFitted() const { return(state == (uint8_t)valvePinWithdrawn); }

    // Returns true iff in normal running state.
    // True means not in error state and not (re)calibrating/(re)initialising/(re)syncing.
    // May be false temporarily while decalcinating.
    bool isInNormalRunState() const { return(state == (uint8_t)valveNormal); }

    // Returns true if in an error state.
    // May be recoverable by forcing recalibration.
    bool isInErrorState() const { return(state >= (uint8_t)valveError); }

    // True if (re)calibration should be deferred.
    // Potentially an expensive call in time and energy.
    // Primarily public to allow whitebox unit testing.
    bool shouldDeferCalibration();

    // If true, proportional mode is not being used and the valve is run to end stops instead.
    // Primarily public to allow whitebox unit testing.
    bool inNonProprtionalMode() const { return(needsRecalibrating); }
  };


}

#endif /* ARDUINO_LIB_OTRADVALVE_CURRENTSENSEVALVEMOTORDIRECT_H_ */
