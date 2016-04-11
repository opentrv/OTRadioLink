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
 * Driver for DORM1/REV7 direct motor drive.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_VALVEMOTORDIRECTV1_H
#define ARDUINO_LIB_OTRADVALVE_VALVEMOTORDIRECTV1_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include "OTRadValve_AbstractRadValve.h"


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


// Generic (unit-testable) motor driver login using end-stop detection and simple shaft-encoder.
// Designed to be embedded in a motor controller instance.
// This used the sub-cycle clock for timing.
// This is sensitive to sub-cycle position, ie will try to avoid causing a main loop overrun.
// May report some key status on Serial, with any error line(s) starting with "!'.
class CurrentSenseValveMotorDirect : public OTRadValve::HardwareMotorDriverInterfaceCallbackHandler
  {
  public:
    // Maximum time to move pin between fully retracted and extended and vv, seconds, strictly positive.
    // Set as a limit to allow a timeout when things go wrong.
    static const uint8_t MAX_TRAVEL_S = 4 * 60; // 4 minutes.

    // Assumed calls to read() before timeout (assuming o call each 2s).
    // If calls are received less often this will presumably take longer to perform movements,
    // so it is appropriate to use a 2s ticks approximation.
    static const uint8_t MAX_TRAVEL_WALLCLOCK_2s_TICKS = max(4, MAX_TRAVEL_S / 2);

    // Calibration parameters.
    // Data received during the calibration process,
    // and outputs derived from it.
    // Contains (unit-testable) computations.
    class CalibrationParameters
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
          CalibrationParameters() : ticksFromOpenToClosed(0), ticksFromClosedToOpen(0) { }

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

    // Minimum percent at which valve is usually open [1,00];
    const uint8_t minOpenPC;
    // Minimum percent at which valve is usually moderately open [minOpenPC+1,00];
    const uint8_t fairlyOpenPC;

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

    // Set when valve needs recalibration, eg because dead-reckoning found to be significantly wrong.
    // May also need recalibrating after (say) a few weeks to allow for battery/speed droop.
    bool needsRecalibrating;

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
    uint8_t currentPC;

    // Target % open in range [0,100].
    // Maintained across all states; defaults to 'closed'/0.
    uint8_t targetPC;

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

    // Compute and apply reconciliation/adjustment of ticks and % position.
    // Uses computePosition() to adjust internal state.
    // Call after moving the valve in normal mode.
    void recomputePosition() { currentPC = cp.computePosition(ticksFromOpen, ticksReverse); }

    // Report an apparent serious tracking error that may need full recalibration.
    void trackingError();

  public:
    // Create an instance, passing in a reference to the non-NULL hardware driver.
    // The hardware driver instance lifetime must be longer than this instance.
    CurrentSenseValveMotorDirect(OTRadValve::HardwareMotorDriverInterface * const hwDriver,
                                 uint8_t _minOpenPC = OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN,
                                 uint8_t _fairlyOpenPC = OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN) :
        hw(hwDriver), minOpenPC(_minOpenPC), fairlyOpenPC(_fairlyOpenPC), currentPC(0), targetPC(0)
        { changeState(init); }

    // Poll.
    // Regular poll every 1s or 2s,
    // though tolerates missed polls eg because of other time-critical activity.
    // May block for hundreds of milliseconds.
    void poll();

    // Get major state, mostly for testing.
    driverState getState() const { return((driverState) state); }

    // Get current estimated actual % open in range [0,100].
    uint8_t getCurrentPC() { return(currentPC); }

    // Get current target % open in range [0,100].
    uint8_t getTargetPC() { return(targetPC); }

    // Set current target % open in range [0,100].
    // Coerced into range.
    void setTargetPC(uint8_t newPC) { targetPC = min(newPC, 100); }

    // Get estimated minimum percentage open for significant flow for this device; strictly positive in range [1,99].
    virtual uint8_t getMinPercentOpen() const;

    // Minimally wiggle the motor to give tactile feedback and/or show to be working.
    // May take a significant fraction of a second.
    // Finishes with the motor turned off.
    virtual void wiggle();

    // Called when end stop hit, eg by overcurrent detection.
    // Can be called while run() is in progress.
    // Is ISR-/thread- safe.
    virtual void signalHittingEndStop(bool /*opening*/) { endStopDetected = true; }

    // Called when encountering leading edge of a mark in the shaft rotation in forward direction (falling edge in reverse).
    // Can be called while run() is in progress.
    // Is ISR-/thread- safe.
    virtual void signalShaftEncoderMarkStart(bool /*opening*/) { /* TODO */ }

    // Called with each motor run sub-cycle tick.
    // Is ISR-/thread- safe.
    virtual void signalRunSCTTick(bool opening);

    // Call when given user signal that valve has been fitted (ie is fully on).
    virtual void signalValveFitted() { if(isWaitingForValveToBeFitted()) { perState.valvePinWithdrawn.valveFitted = true; } }

    // Waiting for indication that the valvehead  has been fitted to the tail.
    virtual bool isWaitingForValveToBeFitted() const { return(state == (uint8_t)valvePinWithdrawn); }

    // Returns true iff in normal running state.
    // True means not in error state and not (re)calibrating/(re)initialising/(re)syncing.
    // May be false temporarily while decalcinating.
    virtual bool isInNormalRunState() const { return(state == (uint8_t)valveNormal); }

    // Returns true if in an error state.
    // May be recoverable by forcing recalibration.
    virtual bool isInErrorState() const { return(state >= (uint8_t)valveError); }
  };



class ValveMotorDirectV1HardwareDriverBase : public OTRadValve::HardwareMotorDriverInterface
  {
  public:
    // Approx minimum time to let H-bridge settle/stabilise (ms).
    static const uint8_t minMotorHBridgeSettleMS = 8;
    // Min sub-cycle ticks for H-bridge to settle.
    static const uint8_t minMotorHBridgeSettleTicks = max(1, minMotorHBridgeSettleMS / OTV0P2BASE::SUBCYCLE_TICK_MS_RD);

    // Approx minimum runtime to get motor up to speed (from stopped) and not give false high-current readings (ms).
    // Based on DHD20151019 DORM1 prototype rig-up and NiMH battery; 32ms+ seems good.
    static const uint8_t minMotorRunupMS = 32;
    // Min sub-cycle ticks to run up.
    static const uint8_t minMotorRunupTicks = max(1, minMotorRunupMS / OTV0P2BASE::SUBCYCLE_TICK_MS_RD);

  private:
    // Maximum current reading allowed when closing the valve (against the spring).
    static const uint16_t maxCurrentReadingClosing = 600;
    // Maximum current reading allowed when opening the valve (retracting the pin, no resisting force).
    // Keep this as low as possible to reduce the chance of skipping the end-stop and game over...
    // DHD20151229: at 500 Shenzhen sample unit without outer case (so with more flex) was able to drive past end-stop.
    static const uint16_t maxCurrentReadingOpening = 450; // DHD20151023: 400 seemed marginal.

  protected:
    // Spin for up to the specified number of SCT ticks, monitoring current and position encoding.
    //   * maxRunTicks  maximum sub-cycle ticks to attempt to run/spin for); strictly positive
    //   * minTicksBeforeAbort  minimum ticks before abort for end-stop / high-current,
    //       don't attempt to run at all if less than this time available before (close to) end of sub-cycle;
    //       should be no greater than maxRunTicks
    //   * dir  direction to run motor (open or closed) or off if waiting for motor to stop
    //   * callback  handler to deliver end-stop and position-encoder callbacks to;
    //     non-null and callbacks must return very quickly
    // If too few ticks remain before the end of the sub-cycle for the minimum run,
    // then this will return true immediately.
    // Invokes callbacks for high current (end stop) and position (shaft) encoder where applicable.
    // Aborts early if high current is detected at the start,
    // or after the minimum run period.
    // Returns true if aborted early from too little time to start, or by high current (assumed end-stop hit).
    bool spinSCTTicks(uint8_t maxRunTicks, uint8_t minTicksBeforeAbort, OTRadValve::HardwareMotorDriverInterface::motor_drive dir, OTRadValve::HardwareMotorDriverInterfaceCallbackHandler &callback);
  };

// Implementation for V1 (REV7/DORM1) motor.
// Usually not instantiated except within ValveMotorDirectV1.
// Creating multiple instances (trying to drive same motor) almost certainly a BAD IDEA.
template <uint8_t MOTOR_DRIVE_ML_DigitalPin, uint8_t MOTOR_DRIVE_MR_DigitalPin, uint8_t MOTOR_DRIVE_MI_AIN_DigitalPin, uint8_t MOTOR_DRIVE_MC_AIN_DigitalPin>
class ValveMotorDirectV1HardwareDriver : public ValveMotorDirectV1HardwareDriverBase
  {
    // Last recorded direction.
    // Helpful to record shaft-encoder and other behaviour correctly around direction changes.
    // Marked volatile and stored as uint8_t to help thread-safety, and potentially save space.
    volatile uint8_t last_dir;

  public:
    ValveMotorDirectV1HardwareDriver() : last_dir((uint8_t)motorOff) { }

    // Detect if end-stop is reached or motor current otherwise very high.
    virtual bool isCurrentHigh(OTRadValve::HardwareMotorDriverInterface::motor_drive mdir = motorDriveOpening) const
      {
      // Check for high motor current indicating hitting an end-stop.
      // Measure motor current against (fixed) internal reference.
      const uint16_t mi = OTV0P2BASE::analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN_DigitalPin, INTERNAL);
//      const uint16_t mi = OTV0P2BASE::analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN_DigitalPin, DEFAULT);
//#if 1 // 0 && defined(V0P2BASE_DEBUG)
//OTV0P2BASE::serialPrintAndFlush(F("    MI: "));
//OTV0P2BASE::serialPrintAndFlush(mi, DEC);
//OTV0P2BASE::serialPrintlnAndFlush();
//#endif
      const uint16_t miHigh = (OTRadValve::HardwareMotorDriverInterface::motorDriveClosing == mdir) ?
          maxCurrentReadingClosing : maxCurrentReadingOpening;
      const bool currentSense = (mi > miHigh); // &&
        // Recheck the value read in case spiky.
        // (OTV0P2BASE::analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN_DigitalPin, INTERNAL) > miHigh) &&
        // (OTV0P2BASE::analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN_DigitalPin, INTERNAL) > miHigh);
      return(currentSense);
      }

    // Poll simple shaft encoder output; true if on mark, false if not or if unused for this driver.
    virtual bool isOnShaftEncoderMark() const
      {
      // Power up IR emitter for shaft encoder and assume instant-on, as this has to be as fast as reasonably possible.
      OTV0P2BASE::power_intermittent_peripherals_enable();
//      const bool result = OTV0P2BASE::analogueVsBandgapRead(MOTOR_DRIVE_MC_AIN_DigitalPin, true);
      const uint16_t mc = OTV0P2BASE::analogueNoiseReducedRead(MOTOR_DRIVE_MC_AIN_DigitalPin, INTERNAL);
       const bool result = (mc < 120); // Arrrgh! FIXME: needs autocalibration during wiggle().
      OTV0P2BASE::power_intermittent_peripherals_disable();
#if 0 && defined(V0P2BASE_DEBUG)
OTV0P2BASE::serialPrintAndFlush(F("    MC: "));
OTV0P2BASE::serialPrintAndFlush(mc, DEC);
OTV0P2BASE::serialPrintlnAndFlush();
#endif
      return(result);
      }

    // Call to actually run/stop motor.
    // May take as much as (say) 200ms eg to change direction.
    // Stopping (removing power) should typically be very fast, << 100ms.
    //   * maxRunTicks  maximum sub-cycle ticks to attempt to run/spin for); zero will run for shortest reasonable time
    //   * dir  direction to run motor (or off/stop)
    //   * callback  callback handler
    // DHD20160105: note that for REV7/DORM1 with ~2.4V+ battery, H-bridge drive itself ~20mA+, motor ~200mA.
    virtual void motorRun(const uint8_t maxRunTicks,
                          const OTRadValve::HardwareMotorDriverInterface::motor_drive dir,
                          OTRadValve::HardwareMotorDriverInterfaceCallbackHandler &callback)
      {
      // Remember previous state of motor.
      // This may help to correctly allow for (eg) position encoding inputs while a motor is slowing.
      const uint8_t prev_dir = last_dir;

      // *** MUST NEVER HAVE L AND R LOW AT THE SAME TIME else board may be destroyed at worst. ***
      // Operates as quickly as reasonably possible, eg to move to stall detection quickly...
      // TODO: consider making atomic to block some interrupt-related accidents...
      // TODO: note that the mapping between L/R and open/close not yet defined.
      // DHD20150205: 1st cut REV7 all-in-in-valve, seen looking down from valve into base, cw => close (ML=HIGH), ccw = open (MR=HIGH).
      switch(dir)
        {
        case motorDriveClosing:
          {
          // Pull one side high immediately *FIRST* for safety.
          // Stops motor if other side is not already low.
          // (Has no effect if motor is already running in the correct direction.)
          fastDigitalWrite(MOTOR_DRIVE_ML_DigitalPin, HIGH);
          pinMode(MOTOR_DRIVE_ML_DigitalPin, OUTPUT); // Ensure that the HIGH side is an output (can be done after, as else will be safe weak pull-up).
          // Let H-bridge respond and settle, and motor slow down if changing direction.
          // Otherwise there is a risk of browning out the device with a big current surge.
          if(prev_dir != dir) { OTV0P2BASE::nap(WDTO_120MS); } // Enforced low-power sleep on change of direction....
          pinMode(MOTOR_DRIVE_MR_DigitalPin, OUTPUT); // Ensure that the LOW side is an output.
          fastDigitalWrite(MOTOR_DRIVE_MR_DigitalPin, LOW); // Pull LOW last.
          // Let H-bridge respond and settle and let motor run up.
          spinSCTTicks(max(maxRunTicks, minMotorRunupTicks), minMotorRunupTicks, dir, callback);
          break; // Fall through to common case.
          }

        case motorDriveOpening:
          {
          // Pull one side high immediately *FIRST* for safety.
          // Stops motor if other side is not already low.
          // (Has no effect if motor is already running in the correct direction.)
          fastDigitalWrite(MOTOR_DRIVE_MR_DigitalPin, HIGH);
          pinMode(MOTOR_DRIVE_MR_DigitalPin, OUTPUT); // Ensure that the HIGH side is an output (can be done after, as else will be safe weak pull-up).
          // Let H-bridge respond and settle, and motor slow down if changing direction.
          // Otherwise there is a risk of browning out the device with a big current surge.
          if(prev_dir != dir) { OTV0P2BASE::nap(WDTO_120MS); } // Enforced low-power sleep on change of direction....
          pinMode(MOTOR_DRIVE_ML_DigitalPin, OUTPUT); // Ensure that the LOW side is an output.
          fastDigitalWrite(MOTOR_DRIVE_ML_DigitalPin, LOW); // Pull LOW last.
          // Let H-bridge respond and settle and let motor run up.
          spinSCTTicks(max(maxRunTicks, minMotorRunupTicks), minMotorRunupTicks, dir, callback);
          break; // Fall through to common case.
          }

        case motorOff: default: // Explicit off, and default for safety.
          {
          // Everything off, unconditionally.
          //
          // Turn one side of bridge off ASAP.
          fastDigitalWrite(MOTOR_DRIVE_MR_DigitalPin, HIGH); // Belt and braces force pin logical output state high.
          pinMode(MOTOR_DRIVE_MR_DigitalPin, INPUT_PULLUP); // Switch to weak pull-up; slow but possibly marginally safer.
          // Let H-bridge respond and settle.
          // Accumulate any shaft movement & time to the previous direction if not already stopped.
          // Wait longer if not previously off to allow for inertia, if shaft encoder is in use.
          const bool shaftEncoderInUse = false; // FIXME.
          const bool wasOffBefore = (HardwareMotorDriverInterface::motorOff == prev_dir);
          const bool longerWait = shaftEncoderInUse || !wasOffBefore;
          spinSCTTicks(!longerWait ? minMotorHBridgeSettleTicks : minMotorRunupTicks, !longerWait ? 0 : minMotorRunupTicks/2, (motor_drive)prev_dir, callback);
          fastDigitalWrite(MOTOR_DRIVE_ML_DigitalPin, HIGH); // Belt and braces force pin logical output state high.
          pinMode(MOTOR_DRIVE_ML_DigitalPin, INPUT_PULLUP); // Switch to weak pull-up; slow but possibly marginally safer.
          // Let H-bridge respond and settle.
          spinSCTTicks(minMotorHBridgeSettleTicks, 0, HardwareMotorDriverInterface::motorOff, callback);
          if(prev_dir != dir) { OTV0P2BASE::nap(WDTO_60MS); } // Enforced low-power sleep on change of direction....
          break; // Fall through to common case.
          }
        }

      // Record new direction.
      last_dir = dir;
      }
  };

// Actuator/driver for direct local (radiator) valve motor control.
template <uint8_t MOTOR_DRIVE_ML_DigitalPin, uint8_t MOTOR_DRIVE_MR_DigitalPin, uint8_t MOTOR_DRIVE_MI_AIN_DigitalPin, uint8_t MOTOR_DRIVE_MC_AIN_DigitalPin>
class ValveMotorDirectV1 : public OTRadValve::AbstractRadValve
  {
  private:
    // Driver for the V1/DORM1 hardware.
    ValveMotorDirectV1HardwareDriver<MOTOR_DRIVE_ML_DigitalPin, MOTOR_DRIVE_MR_DigitalPin, MOTOR_DRIVE_MI_AIN_DigitalPin, MOTOR_DRIVE_MC_AIN_DigitalPin> driver;
    // Logic to manage state, etc.
    CurrentSenseValveMotorDirect logic;

  public:
    ValveMotorDirectV1(uint8_t minOpenPC = OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN,
                       uint8_t fairlyOpenPC = OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN)
      : logic(&driver, minOpenPC, fairlyOpenPC) { }

    // Regular poll/update.
    // This and get() return the actual estimated valve position.
    virtual uint8_t read()
      {
      logic.poll();
      value = logic.getCurrentPC();
      return(value);
      }

    // Set new target %-open value (if in range).
    // Returns true if the specified value is accepted.
    virtual bool set(const uint8_t newValue)
      {
      if(newValue > 100) { return(false); }
      logic.setTargetPC(newValue);
      return(true);
      }

    // Get estimated minimum percentage open for significant flow for this device; strictly positive in range [1,99].
    virtual uint8_t getMinPercentOpen() const { return(logic.getMinPercentOpen()); }

    // Call when given user signal that valve has been fitted (ie is fully on).
    virtual void signalValveFitted() { logic.signalValveFitted(); }

    // Waiting for indication that the valve head has been fitted to the tail.
    virtual bool isWaitingForValveToBeFitted() const { return(logic.isWaitingForValveToBeFitted()); }

    // Returns true iff not in error state and not (re)calibrating/(re)initialising/(re)syncing.
    virtual bool isInNormalRunState() const { return(logic.isInNormalRunState()); }

    // Returns true if in an error state,
    virtual bool isInErrorState() const { return(logic.isInErrorState()); }

    // Minimally wiggles the motor to give tactile feedback and/or show to be working.
    // May take a significant fraction of a second.
    // Finishes with the motor turned off, and a bias to closing the valve.
    virtual void wiggle() { logic.wiggle(); }
  };


    }

#endif
