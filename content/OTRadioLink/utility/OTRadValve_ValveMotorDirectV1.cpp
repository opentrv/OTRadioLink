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

#include "OTRadValve_AbstractRadValve.h"
#include "OTRadValve_ValveMotorDirectV1.h"

namespace OTRadValve
    {


//// Approx minimum time to let H-bridge settle/stabilise (ms).
//static const uint8_t minMotorHBridgeSettleMS = 8;
//// Min sub-cycle ticks for H-bridge to settle.
//static const uint8_t minMotorHBridgeSettleTicks = max(1, minMotorHBridgeSettleMS / SUBCYCLE_TICK_MS_RD);
//
//// Approx minimum runtime to get motor up to speed (from stopped) and not give false high-current readings (ms).
//// Based on DHD20151019 DORM1 prototype rig-up and NiMH battery; 32ms+ seems good.
//static const uint8_t minMotorRunupMS = 32;
//// Min sub-cycle ticks to run up.
//static const uint8_t minMotorRunupTicks = max(1, minMotorRunupMS / SUBCYCLE_TICK_MS_RD);

// Runtime for dead-reckoning adjustments (from stopped) (ms).
// Smaller values nominally allow greater precision when dead-reckoning,
// but may force the calibration to take longer.
// Based on DHD20151020 DORM1 prototype rig-up and NiMH battery; 250ms+ seems good.
static const uint8_t minMotorDRMS = 250;
// Min sub-cycle ticks for dead reckoning.
static const uint8_t minMotorDRTicks = max(1, (uint8_t)(minMotorDRMS / OTV0P2BASE::SUBCYCLE_TICK_MS_RD));

// Absolute limit in sub-cycle beyond which motor should not be started.
// This should allow meaningful movement and stop and settle and no sub-cycle overrun.
// Allows for up to 120ms enforced sleep either side of motor run for example.
// This should not be so greedy as to (eg) make the CLI unusable: 90% is pushing it.
static const uint8_t sctAbsLimit = OTV0P2BASE::GSCT_MAX - max(1, ((OTV0P2BASE::GSCT_MAX+1)/8)) - OTRadValve::ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks - (uint8_t)(240 / OTV0P2BASE::SUBCYCLE_TICK_MS_RD);

// Absolute limit in sub-cycle beyond which motor should not be started for dead-reckoning pulse.
// This should allow meaningful movement and no sub-cycle overrun.
static const uint8_t sctAbsLimitDR = sctAbsLimit - minMotorDRTicks;


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
bool ValveMotorDirectV1HardwareDriverBase::spinSCTTicks(const uint8_t maxRunTicks, const uint8_t minTicksBeforeAbort, const OTRadValve::HardwareMotorDriverInterface::motor_drive dir, OTRadValve::HardwareMotorDriverInterfaceCallbackHandler &callback)
  {
  // Sub-cycle time now.
  const uint8_t sctStart = OTV0P2BASE::getSubCycleTime();
  // Only run up to ~90% point of the minor cycle to leave time for other processing.
  uint8_t sct = sctStart;
  const uint8_t maxTicksBeforeAbsLimit = (sctAbsLimit - sct);
  // Abort immediately if not enough time to do minimum run.
  if((sct > sctAbsLimit) || (maxTicksBeforeAbsLimit < minTicksBeforeAbort)) { return(true); }
  // Note if opening or closing...
  const bool stopped = (HardwareMotorDriverInterface::motorOff == dir);
  const bool isOpening = (HardwareMotorDriverInterface::motorDriveOpening == dir);
  bool currentHigh = false;
  // Compute time minimum time before return, then target time before stop/return.
  const uint8_t sctMinRunTime = sctStart + minTicksBeforeAbort; // Min run time to avoid false readings.
  const uint8_t sctMaxRunTime = sctStart + min(maxRunTicks, maxTicksBeforeAbsLimit);
  // Do minimum run time, NOT checking for end-stop / high current.
  for( ; ; )
    {
    // Poll shaft encoder output and update tick counter.
    const uint8_t newSct = OTV0P2BASE::getSubCycleTime();
    if(newSct != sct)
      {
      sct = newSct; // Assumes no intermediate values missed.
      if(!stopped) { callback.signalRunSCTTick(isOpening); }
      if(sct >= sctMinRunTime) { break; }
      }
    // TODO: shaft encoder
    }

  // Do as much of requested above-minimum run-time as possible,
  // iff run time beyond the minimum was actually requested
  // (else avoid the current sampling entirely).
  if(sctMaxRunTime > sctMinRunTime)
    {
    for( ; ; )
      {
      // Check for high current and abort if detected.
      if(isCurrentHigh(dir)) { currentHigh = true; break; }
      // Poll shaft encoder output and update tick counter.
      const uint8_t newSct = OTV0P2BASE::getSubCycleTime();
      if(newSct != sct)
        {
        sct = newSct; // Assumes no intermediate values missed.
        if(!stopped) { callback.signalRunSCTTick(isOpening); }
        if(sct >= sctMaxRunTime) { break; }
        }
      }
    }

  // Call back and return true if current high / end-stop seen.
  if(currentHigh)
    {
    callback.signalHittingEndStop(isOpening);
    return(true);
    }
  return(false);
  }

//// Call to actually run/stop motor.
//// May take as much as (say) 200ms eg to change direction.
//// Stopping (removing power) should typically be very fast, << 100ms.
////   * maxRunTicks  maximum sub-cycle ticks to attempt to run/spin for); zero will run for shortest reasonable time
////   * dir  direction to run motor (or off/stop)
////   * callback  callback handler
//void ValveMotorDirectV1HardwareDriver::motorRun(const uint8_t maxRunTicks,
//                                                const motor_drive dir,
//                                                OTRadValve::HardwareMotorDriverInterfaceCallbackHandler &callback)
//  {
//  // Remember previous state of motor.
//  // This may help to correctly allow for (eg) position encoding inputs while a motor is slowing.
//  const uint8_t prev_dir = last_dir;
//
//  // *** MUST NEVER HAVE L AND R LOW AT THE SAME TIME else board may be destroyed at worst. ***
//  // Operates as quickly as reasonably possible, eg to move to stall detection quickly...
//  // TODO: consider making atomic to block some interrupt-related accidents...
//  // TODO: note that the mapping between L/R and open/close not yet defined.
//  // DHD20150205: 1st cut REV7 all-in-in-valve, seen looking down from valve into base, cw => close (ML=HIGH), ccw = open (MR=HIGH).
//  switch(dir)
//    {
//    case motorDriveClosing:
//      {
//      // Pull one side high immediately *FIRST* for safety.
//      // Stops motor if other side is not already low.
//      // (Has no effect if motor is already running in the correct direction.)
//      fastDigitalWrite(MOTOR_DRIVE_ML, HIGH);
//      pinMode(MOTOR_DRIVE_ML, OUTPUT); // Ensure that the HIGH side is an output (can be done after, as else will be safe weak pull-up).
//      // Let H-bridge respond and settle, and motor slow down if changing direction.
//      // Otherwise there is a risk of browning out the device with a big current surge.
//      if(prev_dir != dir) { OTV0P2BASE::nap(WDTO_120MS); } // Enforced low-power sleep on change of direction....
//      pinMode(MOTOR_DRIVE_MR, OUTPUT); // Ensure that the LOW side is an output.
//      fastDigitalWrite(MOTOR_DRIVE_MR, LOW); // Pull LOW last.
//      // Let H-bridge respond and settle and let motor run up.
//      spinSCTTicks(max(maxRunTicks, minMotorRunupTicks), minMotorRunupTicks, dir, callback);
//      break; // Fall through to common case.
//      }
//
//    case motorDriveOpening:
//      {
//      // Pull one side high immediately *FIRST* for safety.
//      // Stops motor if other side is not already low.
//      // (Has no effect if motor is already running in the correct direction.)
//      fastDigitalWrite(MOTOR_DRIVE_MR, HIGH);
//      pinMode(MOTOR_DRIVE_MR, OUTPUT); // Ensure that the HIGH side is an output (can be done after, as else will be safe weak pull-up).
//      // Let H-bridge respond and settle, and motor slow down if changing direction.
//      // Otherwise there is a risk of browning out the device with a big current surge.
//      if(prev_dir != dir) { OTV0P2BASE::nap(WDTO_120MS); } // Enforced low-power sleep on change of direction....
//      pinMode(MOTOR_DRIVE_ML, OUTPUT); // Ensure that the LOW side is an output.
//      fastDigitalWrite(MOTOR_DRIVE_ML, LOW); // Pull LOW last.
//      // Let H-bridge respond and settle and let motor run up.
//      spinSCTTicks(max(maxRunTicks, minMotorRunupTicks), minMotorRunupTicks, dir, callback);
//      break; // Fall through to common case.
//      }
//
//    case motorOff: default: // Explicit off, and default for safety.
//      {
//      // Everything off, unconditionally.
//      //
//      // Turn one side of bridge off ASAP.
//      fastDigitalWrite(MOTOR_DRIVE_MR, HIGH); // Belt and braces force pin logical output state high.
//      pinMode(MOTOR_DRIVE_MR, INPUT_PULLUP); // Switch to weak pull-up; slow but possibly marginally safer.
//      // Let H-bridge respond and settle.
//      // Accumulate any shaft movement & time to the previous direction if not already stopped.
//      // Wait longer if not previously off to allow for inertia, if shaft encoder is in use.
//      const bool shaftEncoderInUse = false; // FIXME.
//      const bool wasOffBefore = (HardwareMotorDriverInterface::motorOff == prev_dir);
//      const bool longerWait = shaftEncoderInUse || !wasOffBefore;
//      spinSCTTicks(!longerWait ? minMotorHBridgeSettleTicks : minMotorRunupTicks, !longerWait ? 0 : minMotorRunupTicks/2, (motor_drive)prev_dir, callback);
//      fastDigitalWrite(MOTOR_DRIVE_ML, HIGH); // Belt and braces force pin logical output state high.
//      pinMode(MOTOR_DRIVE_ML, INPUT_PULLUP); // Switch to weak pull-up; slow but possibly marginally safer.
//      // Let H-bridge respond and settle.
//      spinSCTTicks(minMotorHBridgeSettleTicks, 0, HardwareMotorDriverInterface::motorOff, callback);
//      if(prev_dir != dir) { OTV0P2BASE::nap(WDTO_60MS); } // Enforced low-power sleep on change of direction....
//      break; // Fall through to common case.
//      }
//    }
//
//  // Record new direction.
//  last_dir = dir;
//  }


//// IF DEFINED: MI output swing asymmetric or is not enough to use fast comparator.
//static const bool MI_NEEDS_ADC = true;

//// Maximum current reading allowed when closing the valve (against the spring).
//static const uint16_t maxCurrentReadingClosing = 600;
//// Maximum current reading allowed when opening the valve (retracting the pin, no resisting force).
//static const uint16_t maxCurrentReadingOpening = 500; // DHD20151023: 400 seemed marginal.

//// Detect if end-stop is reached or motor current otherwise very high.] indicating stall.
//bool ValveMotorDirectV1HardwareDriver::isCurrentHigh(HardwareMotorDriverInterface::motor_drive mdir) const
//  {
//  // Check for high motor current indicating hitting an end-stop.
//#if !defined(MI_NEEDS_ADC)
//  const bool currentSense = analogueVsBandgapRead(MOTOR_DRIVE_MI_AIN, true);
//#else
//  // Measure motor current against (fixed) internal reference.
//  const uint16_t mi = analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN, INTERNAL);
//  const uint16_t miHigh = (HardwareMotorDriverInterface::motorDriveClosing == mdir) ?
//      maxCurrentReadingClosing : maxCurrentReadingOpening;
//  const bool currentSense = (mi > miHigh) &&
//    // Recheck the value read in case spiky.
//    (analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN, INTERNAL) > miHigh) && (analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN, INTERNAL) > miHigh);
////  if(mi > ((2*miHigh)/4)) { DEBUG_SERIAL_PRINT(mi); DEBUG_SERIAL_PRINTLN(); }
//#endif
//  return(currentSense);
//  }

//// Regular poll/update.
//// This and get() return the actual estimated valve position.
//uint8_t ValveMotorDirectV1::read()
//  {
//  logic.poll();
//  value = logic.getCurrentPC();
//  return(value);
//  }
//
//// Set new target value (if in range).
//// Returns true if specified value accepted.
//// Does not set 'value' as that tracks actual position, not target.
//bool ValveMotorDirectV1::set(const uint8_t newValue)
//  {
//  if(newValue > 100) { return(false); }
//  logic.setTargetPC(newValue);
//  return(true);
//  }




// Called with each motor run sub-cycle tick.
// Is ISR-/thread- safe.
void CurrentSenseValveMotorDirect::signalRunSCTTick(const bool opening)
  {
  ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
    {
    // Crudely avoid/ignore underflow/overflow for now.
    // Accumulate ticks in different directions in different counters
    // and resolve/reconcile later in significant chunks.
    if(!opening)
      {
      if(ticksFromOpen < MAX_TICKS_FROM_OPEN) { ++ticksFromOpen; }
      }
    else
      {
      if(ticksReverse < MAX_TICKS_FROM_OPEN) { ++ticksReverse; }
      }
    }
  }


// (Re)populate structure and compute derived parameters.
// Ensures that all necessary items are gathered at once and none forgotten!
// Returns true in case of success.
// May return false and force error state if inputs unusable,
// though will still try to compute all values.
bool CurrentSenseValveMotorDirect::CalibrationParameters::updateAndCompute(const uint16_t _ticksFromOpenToClosed, const uint16_t _ticksFromClosedToOpen)
  {
  ticksFromOpenToClosed = _ticksFromOpenToClosed;
  ticksFromClosedToOpen = _ticksFromClosedToOpen;

  // Compute approx precision in % as min ticks / DR size in range [0,100].
  // Inflate estimate slightly to allow for inertia, etc.
  approxPrecisionPC = (uint8_t) min(100, (128UL*minMotorDRTicks) / min(_ticksFromOpenToClosed, _ticksFromClosedToOpen));

  // Compute a small conversion ratio back and forth
  // which does not add too much error but allows single dead-reckoning steps
  // to be converted back and forth.
  uint16_t tfotc = _ticksFromOpenToClosed;
  uint16_t tfcto = _ticksFromClosedToOpen;
  while(max(tfotc, tfcto) > minMotorDRTicks)
    {
    tfotc >>= 1;
    tfcto >>= 1;
    }
  tfotcSmall = tfotc;
  tfctoSmall = tfcto;

  // Fail if precision far too poor to be usable.
  if(approxPrecisionPC > 25) { return(false); }
  // Fail if lower ratio value so low (< 4 bits) as to introduce huge error.
  if(min(tfotc, tfcto) < 8) { return(false); }

  // All OK.
  return(true);
  }


// Compute reconciliation/adjustment of ticks, and compute % position [0,100].
// Reconcile any reverse ticks (and adjust with forward ticks if needed).
// Call after moving the valve in normal mode.
// Unit testable.
uint8_t CurrentSenseValveMotorDirect::CalibrationParameters::computePosition(
            volatile uint16_t &ticksFromOpen,
            volatile uint16_t &ticksReverse) const
  {
  // Back out the effect of reverse ticks in blocks for dead-reckoning...
  // Should only usually be about 1 block at a time,
  // so don't do anything too clever here.
  while(ticksReverse >= tfctoSmall)
    {
    if(0 == tfctoSmall) { break; } // Prevent hang if not initialised correctly.
    ticksReverse -= tfctoSmall;
    if(ticksFromOpen > tfotcSmall) { ticksFromOpen -= tfotcSmall; }
    else { ticksFromOpen = 0; }
    }

  // TODO: use shaft encoder tracking by preference, ie when available.

  // Do simple % open calcs for range extremes, based on dead-reckoning.
  if(0 == ticksFromOpen) { return(100); }
  if(ticksFromOpen >= ticksFromOpenToClosed) { return(0); }
  // Compute percentage open for intermediate position, based on dead-reckoning.
  // TODO: optimise!
  return((uint8_t) (((ticksFromOpenToClosed - ticksFromOpen) * 100UL) / ticksFromOpenToClosed));
  }


// Get estimated minimum percentage open for significant flow for this device; strictly positive in range [1,99].
uint8_t CurrentSenseValveMotorDirect::getMinPercentOpen() const
    {
    // If in dead-reckoning mode use a very safe estimate,
    // else use a somewhat tighter one.
    // TODO: optimise, ie don't compute each time if frequently called.
    return(usingPositionalEncoder() ?
            max(10 + cp.getApproxPrecisionPC(), DEFAULT_VALVE_PC_MIN_REALLY_OPEN) :
            max(50 + cp.getApproxPrecisionPC(), DEFAULT_VALVE_PC_SAFER_OPEN));
    }

// Minimally wiggle the motor to give tactile feedback and/or show to be working.
// May take a significant fraction of a second.
// Finishes with the motor turned off, and a bias to closing the valve.
void CurrentSenseValveMotorDirect::wiggle()
  {
  hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorOff, *this);
  hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorDriveOpening, *this);
  hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorDriveClosing, *this);
  hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorOff, *this);
  }

// Run fast towards/to end stop as far as possible in this call.
// Terminates significantly before the end of the sub-cycle.
// Possibly allows partial recalibration, or at least re-homing.
// Returns true if end-stop has apparently been hit,
// else will require one or more further calls in new sub-cycles
// to hit the end-stop.
bool CurrentSenseValveMotorDirect::runFastTowardsEndStop(const bool toOpen)
  {
  // Clear the end-stop detection flag ready.
  endStopDetected = false;
  // Run motor as far as possible on this sub-cycle.
  hw->motorRun(~0, toOpen ?
      OTRadValve::HardwareMotorDriverInterface::motorDriveOpening
    : OTRadValve::HardwareMotorDriverInterface::motorDriveClosing, *this);
  // Stop motor and ensure power off.
  hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorOff, *this);
  // Report if end-stop has apparently been hit.
  return(endStopDetected);
  }

// Run at 'normal' speed towards/to end for a fixed time/distance.
// Terminates significantly before the end of the sub-cycle.
// Runs at same speed as during calibration.
// Does the right thing with dead-reckoning and/or position detection.
// Returns true if end-stop has apparently been hit.
bool CurrentSenseValveMotorDirect::runTowardsEndStop(const bool toOpen)
  {
  // Clear the end-stop detection flag ready.
  endStopDetected = false;
  // Run motor as far as possible on this sub-cycle.
  hw->motorRun(minMotorDRTicks, toOpen ?
      OTRadValve::HardwareMotorDriverInterface::motorDriveOpening
    : OTRadValve::HardwareMotorDriverInterface::motorDriveClosing, *this);
  // Stop motor and ensure power off.
  hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorOff, *this);
  // Report if end-stop has apparently been hit.
  return(endStopDetected);
  }


// Report an apparent serious tracking error that may need full recalibration.
void CurrentSenseValveMotorDirect::trackingError()
  {
  // TODO: possibly ignore tracking errors for a minimum interval.
  needsRecalibrating = true;
  }

// Poll.
// Regular poll every 1s or 2s,
// though tolerates missed polls eg because of other time-critical activity.
// May block for hundreds of milliseconds.
void CurrentSenseValveMotorDirect::poll()
  {
  // Run the state machine based on the major state.
  switch(state)
    {
    // Power-up: move to 'pin withdrawing' state and possibly start a timer.
    case init:
      {
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("  init");
      wiggle(); // Tactile feedback and ensure that the motor is left stopped.
      changeState(valvePinWithdrawing);
      // TODO: record time withdrawl starts (to allow time out).
      break;
      }

    // Fully withdrawing pin (nominally opening valve) to make valve head easy to fit.
    case valvePinWithdrawing:
      {
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("  valvePinWithdrawing");
      // Once end-stop has been hit, move to state to wait for user signal and then start calibration.
      if(runFastTowardsEndStop(true)) { changeState(valvePinWithdrawn); }
      break;
      }

    // Running (initial) calibration cycle.
    case valvePinWithdrawn:
      {
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("  valvePinWithdrawn");

      // Wait for signal from user that valve has been fitted...
      // TODO: alternative timeout allows for automatic recovery from crash/restart after say 10 mins.
      // From: void signalValveFitted() { perState.valvePinWithdrawn.valveFitted = true; }

      // Once fitted, move to calibration.
      if(perState.valvePinWithdrawn.valveFitted)
        { changeState(valveCalibrating); }
      break;
      }

    // Running (initial or re-) calibration cycle.
    case valveCalibrating:
      {
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("  valveCalibrating");
//      DEBUG_SERIAL_PRINT_FLASHSTRING("    calibState: ");
//      DEBUG_SERIAL_PRINT(perState.calibrating.calibState);
//      DEBUG_SERIAL_PRINTLN();
      // Select activity based on micro-state.
      switch(perState.valveCalibrating.calibState)
        {
        case 0:
          {
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINTLN_FLASHSTRING("+calibrating");
#endif
          ++perState.valveCalibrating.calibState; // Move to next micro state.
          break;
          }
        case 1:
          {
          // Run fast to fully open.
          if(runFastTowardsEndStop(true))
            {
            // Reset tick count.
            ticksFromOpen = 0;
            ticksReverse = 0;
            ++perState.valveCalibrating.calibState; // Move to next micro state.
            }
          break;
          }
        case 2:
          {
          // Run pin to fully extended (valve closed).
          // Be prepared to run the (usually small) dead-reckoning pulse while lots of sub-cycle still available.
          do
            {
            // Once end-stop has been hit, capture run length and prepare to run in opposite direction.
            if(runTowardsEndStop(false))
              {
              const uint16_t tfotc = ticksFromOpen;
              perState.valveCalibrating.ticksFromOpenToClosed = tfotc;
              ++perState.valveCalibrating.calibState; // Move to next micro state.
              break;
              }
            } while(OTV0P2BASE::getSubCycleTime() <= sctAbsLimitDR);
          break;
          }
        case 3:
          {
          // Run pin to fully retracted again (valve open).
          // Be prepared to run the (usually small) pulse while lots of sub-cycle still available.
          do
            {
            // Once end-stop has been hit, capture run length and prepare to run in opposite direction.
            if(runTowardsEndStop(true))
              {
              const uint16_t tfcto = ticksReverse;
              // Help avoid premature termination of this direction
              // by NOT terminating this run if much shorter than run in other direction.
              if(tfcto >= (perState.valveCalibrating.ticksFromOpenToClosed >> 1))
                {
                perState.valveCalibrating.ticksFromClosedToOpen = tfcto;
                // Reset tick count.
                ticksFromOpen = 0;
                ticksReverse = 0;
                ++perState.valveCalibrating.calibState; // Move to next micro state.
                }
              break; // In all cases when end-stop hit don't try to run further in this sub-cycle.
              }
            } while(OTV0P2BASE::getSubCycleTime() <= sctAbsLimitDR);
          break;
          }
        case 4:
          {
          // Set all measured calibration input parameters and current position.
          cp.updateAndCompute(perState.valveCalibrating.ticksFromOpenToClosed, perState.valveCalibrating.ticksFromClosedToOpen);

#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINT_FLASHSTRING("    ticksFromOpenToClosed: ");
DEBUG_SERIAL_PRINT(cp.getTicksFromOpenToClosed());
DEBUG_SERIAL_PRINTLN();
DEBUG_SERIAL_PRINT_FLASHSTRING("    ticksFromClosedToOpen: ");
DEBUG_SERIAL_PRINT(cp.getTicksFromClosedToOpen());
DEBUG_SERIAL_PRINTLN();
DEBUG_SERIAL_PRINT_FLASHSTRING("    precision %: ");
DEBUG_SERIAL_PRINT(cp.getApproxPrecisionPC());
DEBUG_SERIAL_PRINTLN();
#endif

          // Move to normal valve running state...
          needsRecalibrating = false;
          currentPC = 100; // Valve is currently fully open.
          // Reset tick count.
          ticksFromOpen = 0;
          ticksReverse = 0;
          changeState(valveNormal);
          break;
          }
        // In case of unexpected microstate shut down gracefully.
        default: { changeState(valveError); break; }
        }
      break;
      }

    // Normal running state: attempt to track the specified target valve open percentage.
    case valveNormal:
      {
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("  valveNormal");

      // Recalibrate if a serious tracking error was detected.
      if(needsRecalibrating)
        {
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINTLN_FLASHSTRING("!needsRecalibrating");
#endif
        changeState(valveCalibrating);
        break;
        }

      // If the current estimated position matches the target
      // then there is usually nothing to do.
      if(currentPC == targetPC) { break; }

      // If the current estimated position does NOT match the target
      // then (incrementally) try to adjust to match.
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINT_FLASHSTRING("  valve err: @");
DEBUG_SERIAL_PRINT(currentPC);
DEBUG_SERIAL_PRINT_FLASHSTRING(" vs target ");
DEBUG_SERIAL_PRINT(targetPC);
DEBUG_SERIAL_PRINTLN();
#endif

      // Special case where target is an end-point (or close to).
      // Run fast to the end-stop.
      // Be eager and pull to end stop if near for continuous auto-recalibration.
      // Must work when eps is zero (ie with sub-percent precision).
      const uint8_t eps = cp.getApproxPrecisionPC();
      const bool toOpenFast = (targetPC >= (100 - 2*eps));
      if(toOpenFast || (targetPC <= max(2*eps, minOpenPC>>1)))
        {
        // If not apparently yet at end-stop
        // (ie not at correct end stop or with spurious unreconciled ticks)
        // then try again to run to end-stop.
        if((0 == ticksReverse) && (currentPC == (toOpenFast ? 100 : 0))) { break; } // Done
        else if(runFastTowardsEndStop(toOpenFast))
            {
            // TODO: may need to protect against spurious stickiness before end...
            // Reset positional values.
            currentPC = toOpenFast ? 100 : 0;
            ticksReverse = 0;
            ticksFromOpen = toOpenFast ? 0 : cp.getTicksFromOpenToClosed();
            }
        // Estimate intermediate position.
        else { recomputePosition(); }
#if 0 && defined(DEBUG)
if(toOpenFast) { DEBUG_SERIAL_PRINTLN_FLASHSTRING("-->"); } else { DEBUG_SERIAL_PRINTLN_FLASHSTRING("--<"); }
#endif
        break;
        }

      // More general case where target position is somewhere between end-stops.
      // Don't do anything if close enough, ie within computed precision (eps).
      // Else move incrementally to reduce the error.
      // (Incremental small moves may also help when absolute accuracy not that good,
      // allowing closed-loop feedback time to work.)

      // Not open enough.
      if((targetPC > currentPC) && (targetPC >= currentPC + eps)) // Overflow not possible with eps addition.
        {
        // TODO: use shaft encoder positioning by preference, ie when available.
        const bool hitEndStop = runTowardsEndStop(true);
        recomputePosition();
        // Hit the end-stop, possibly prematurely.
        if(hitEndStop)
          {
          // Report serious tracking error (well before 'fairly open' %).
          if(currentPC < min(fairlyOpenPC, 100 - 8*eps))
            { trackingError(); }
          // Silently auto-adjust when end-stop hit close to expected position.
          else
            {
            currentPC = 100;
            ticksReverse = 0;
            ticksFromOpen = 0;
            }
          }
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINTLN_FLASHSTRING("->");
#endif
        break;
        }
      // Not closed enough.
      else if((targetPC < currentPC) && (targetPC + eps <= currentPC)) // Overflow not possible with eps addition.
        {
        // TODO: use shaft encoder positioning by preference, ie when available.
        const bool hitEndStop = runTowardsEndStop(false);
        recomputePosition();
        // Hit the end-stop, possibly prematurely.
        if(hitEndStop)
          {
          // Report serious tracking error.
//          if(currentPC > max(min(DEFAULT_VALVE_PC_MODERATELY_OPEN-1, 2*DEFAULT_VALVE_PC_MODERATELY_OPEN), 8*eps))
          if(currentPC > max(2*minOpenPC, 8*eps))
            { trackingError(); }
          // Silently auto-adjust when end-stop hit close to expected position.
          else
            {
            currentPC = 0;
            ticksReverse = 0;
            ticksFromOpen = cp.getTicksFromOpenToClosed();
            }
          }
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINTLN_FLASHSTRING("-<");
#endif
        break;
        }

      // Within eps; do nothing.

      break;
      }

    // Unexpected: go to error state, stop motor and panic.
    valveError:
    default:
      {
      changeState(valveError);
      hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorOff, *this);
      //panic(); // FIXME // Not expected to return.
      return;
      }
    }
  }


    }
