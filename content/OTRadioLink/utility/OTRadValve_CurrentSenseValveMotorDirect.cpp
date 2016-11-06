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


#include "OTRadValve_CurrentSenseValveMotorDirect.h"


namespace OTRadValve
{


#ifdef CurrentSenseValveMotorDirect_DEFINED

// Called with each motor run sub-cycle tick.
// Is ISR-/thread- safe ***on AVR***.
void CurrentSenseValveMotorDirect::signalRunSCTTick(const bool opening)
  {
#ifdef ARDUINO_ARCH_AVR
  ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
#else
  // FIXME: use portable concurrent mechanisms...
#endif // ARDUINO_ARCH_AVR
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
bool CurrentSenseValveMotorDirect::CalibrationParameters::updateAndCompute(
    const uint16_t _ticksFromOpenToClosed, const uint16_t _ticksFromClosedToOpen, const uint8_t minMotorDRTicks)
  {
  ticksFromOpenToClosed = _ticksFromOpenToClosed;
  ticksFromClosedToOpen = _ticksFromClosedToOpen;

  // Compute approx precision in % as min ticks / DR size in range [0,100].
  // Inflate estimate slightly to allow for inertia, etc.
  const uint16_t minticks = OTV0P2BASE::fnmin(_ticksFromOpenToClosed, _ticksFromClosedToOpen);
  approxPrecisionPC = (uint8_t) OTV0P2BASE::fnmin(100UL, (128UL*minMotorDRTicks) / minticks);

  // Compute a small conversion ratio back and forth
  // which does not add too much error but allows single dead-reckoning steps
  // to be converted back and forth.
  uint16_t tfotc = _ticksFromOpenToClosed;
  uint16_t tfcto = _ticksFromClosedToOpen;
  while(OTV0P2BASE::fnmax(tfotc, tfcto) > minMotorDRTicks)
    {
    tfotc >>= 1;
    tfcto >>= 1;
    }
  tfotcSmall = tfotc;
  tfctoSmall = tfcto;

  // Fail if precision far too poor to be usable.
  if(approxPrecisionPC > 25) { return(false); }
  // Fail if lower ratio value so low (< 4 bits) as to introduce huge error.
  if(OTV0P2BASE::fnmin(tfotc, tfcto) < 8) { return(false); }

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
    // If in dead-reckoning mode then use a very safe estimate,
    // else use a somewhat tighter one.
    // TODO: optimise, ie don't compute each time if frequently called.
    return(usingPositionalEncoder() ?
            OTV0P2BASE::fnmax((uint8_t)(10 + cp.getApproxPrecisionPC()), (uint8_t)DEFAULT_VALVE_PC_MIN_REALLY_OPEN) :
            OTV0P2BASE::fnmax((uint8_t)(50 + cp.getApproxPrecisionPC()), (uint8_t)DEFAULT_VALVE_PC_SAFER_OPEN));
    }

// Minimally wiggle the motor to give tactile feedback and/or show to be working.
// May take a significant fraction of a second.
// Finishes with the motor turned off, and a bias to closing the valve.
// Should also have enough movement/play to allow calibration of the shaft encoder.
// May also help set some bounds on stall current, eg if highly asymmetric at each end of travel.
void CurrentSenseValveMotorDirectBinaryOnly::wiggle()
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
// May attempt to ride through stiff mechanics.
bool CurrentSenseValveMotorDirectBinaryOnly::runFastTowardsEndStop(const bool toOpen)
  {
  // Clear the end-stop detection flag ready.
  endStopDetected = false;
  // Run motor as far as possible on this sub-cycle.
  hw->motorRun(~0, toOpen ?
      OTRadValve::HardwareMotorDriverInterface::motorDriveOpening
    : OTRadValve::HardwareMotorDriverInterface::motorDriveClosing, *this);
  // Stop motor and ensure power off.
  hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorOff, *this);
  // If end-stop not hit, return false now.
  if(!endStopDetected) { return(false); }
  // Attempt another short pulse to finish the job if there is time.
  return(runTowardsEndStop(toOpen));
  }

// Run at 'normal' speed towards/to end for a fixed time/distance.
// Terminates significantly before the end of the sub-cycle.
// Runs at same speed as during calibration.
// Does the right thing with dead-reckoning and/or position detection.
// Returns true if end-stop has apparently been hit.
bool CurrentSenseValveMotorDirectBinaryOnly::runTowardsEndStop(const bool toOpen)
  {
  // Clear the end-stop detection flag ready.
  endStopDetected = false;
  // Run motor for fixed time.
  hw->motorRun(minMotorDRTicks, toOpen ?
      OTRadValve::HardwareMotorDriverInterface::motorDriveOpening
    : OTRadValve::HardwareMotorDriverInterface::motorDriveClosing, *this);
  // Stop motor and ensure power off.
  hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorOff, *this);
  // Report if end-stop has apparently been hit.
  return(endStopDetected);
  }

// True if (re)calibration should be deferred.
// Potentially an expensive call in time and energy.
bool CurrentSenseValveMotorDirect::shouldDeferCalibration()
    {
    // Try to force measurement of supply voltage now.
    const bool haveBattMonitor = (NULL != lowBattOpt);
    if(haveBattMonitor) { lowBattOpt->read(); }
    // Defer calibration if doing it now would be a bad idea, eg in a bedroom at night.
    const bool deferRecalibration =
        (haveBattMonitor && lowBattOpt->isSupplyVoltageLow()) ||
        ((NULL != minimiseActivityOpt) && minimiseActivityOpt());
    return(deferRecalibration);
    }

// Poll.
// Regular poll every 1s or 2s,
// though tolerates missed polls eg because of other time-critical activity.
// May block for hundreds of milliseconds.
void CurrentSenseValveMotorDirectBinaryOnly::poll()
  {

#if 0 && defined(V0P2BASE_DEBUG)
OTV0P2BASE::serialPrintAndFlush(F("    isOnShaftEncoderMark(): "));
OTV0P2BASE::serialPrintAndFlush(hw->isOnShaftEncoderMark());
OTV0P2BASE::serialPrintlnAndFlush();
#endif

#if 0
OTV0P2BASE::serialPrintAndFlush("poll(): ");
OTV0P2BASE::serialPrintAndFlush(state);
OTV0P2BASE::serialPrintlnAndFlush();
#endif

  // If too late in the system cycle then exit immediately.
  if(getSubCycleTimeFn() >= sctAbsLimit) { return; }

  // Run the state machine based on the major state.
  switch(state)
    {
    // Power-up: wiggle and then wait to move to 'pin withdrawing' state.
    case init:
      {
//V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("  init");
//      // Make sure that the motor is unconditionally turned off.
//      hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorOff, *this);

      // Tactile feedback and ensure that the motor is left stopped.
      // Should also allow calibration of the shaft-encoder outputs, ie [min.max].
      // May also help free 'stuck' mechanics.
      wiggle();

      // Wait before withdrawing pin (just after power-up).
      changeState(initWaiting);
      break;
      }

    // Wait to start withdrawing pin.
    // A strategic wait here helps make other start-up easier, including CLI-based provisioning.
    case initWaiting:
      {
//V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("  initWaiting");

      // Assume 2s between calls to poll().
      if(perState.initWaiting.ticksWaited < initialRetractDelay_s/2) { ++perState.initWaiting.ticksWaited; break; } // Postpone pin withdraw after power-up.

      // Tactile feedback and ensure that the motor is left stopped.
      // Should also allow calibration of the shaft-encoder outputs, ie [min.max].
      // May also help free 'stuck' mechanics.
      wiggle();

      // Now start on fully withdrawing pin.
      changeState(valvePinWithdrawing);
      // TODO: record time withdrawal starts (to allow time out).
      break;
      }

    // Fully withdrawing pin (nominally opening valve) to make valve head easy to fit.
    case valvePinWithdrawing:
      {
//V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("  valvePinWithdrawing");

      // If taking stupidly long to withdraw the pin fully
      // then assume a problem with the motor/mechanics and give up.
      // Don't panic() so that the unit can still (for example) transmit stats.
      if(++perState.valvePinWithdrawing.wallclock2sTicks > MAX_TRAVEL_WALLCLOCK_2s_TICKS)
          {
          OTV0P2BASE::serialPrintlnAndFlush(F("!valve pin withdraw fail"));
          changeState(valveError);
          break;
          }

      // Once end-stop has been hit, move to state to wait for user signal and then start calibration.

      // Run cautiously while supply voltage low to try to avoid browning out.
      const bool low = ((NULL != lowBattOpt) && ((0 == lowBattOpt->read()) || lowBattOpt->isSupplyVoltageLow()));

      if(runTowardsEndStop(true, low))
          {
          // Note that the valve is now fully open.
          resetPosition(true, true); // Regard as tentative.
          changeState(valvePinWithdrawn);
          }

      break;
      }

    // Running (initial) calibration cycle.
    case valvePinWithdrawn:
      {
//V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("  valvePinWithdrawn");

      // Wait for signal from user that valve has been fitted...
      // Once the valve has been fitted, move to calibration.
      if(perState.valvePinWithdrawn.valveFitted)
        {
        // Wiggle to acknowledge signal from user.
        wiggle();
        changeState(valveCalibrating);
        }

      break;
      }

    // Running (initial or re-) calibration cycle.
    case valveCalibrating:
      {
//V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("  valveCalibrating");
//      V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("    calibState: ");
//      V0P2BASE_DEBUG_SERIAL_PRINT(perState.calibrating.calibState);
//      V0P2BASE_DEBUG_SERIAL_PRINTLN();

      if(do_valveCalibrating_prop()) { return; }

      // By default skip immediately to the normal state if not calibrating.
      changeState(valveNormal);

      break;
      }

    // Normal running state: attempt to track the specified target valve open percentage.
    case valveNormal:
      {
//V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("  valveNormal");

      // If the current estimated position exactly matches the target
      // then there is nothing to do.
      if(currentPC == targetPC) { break; }

      if(do_valveNormal_prop()) { return; }

      // If the current estimated position does NOT match the target
      // then (incrementally) try to adjust to match.
#if 0 && defined(V0P2BASE_DEBUG)
V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("  valve err: @");
V0P2BASE_DEBUG_SERIAL_PRINT(currentPC);
V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING(" vs target ");
V0P2BASE_DEBUG_SERIAL_PRINT(targetPC);
V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif

      // In binary mode, the valve is targeted to be fully open or fully closed.
      // Set to the same threshold value used to trigger boiler call for heat.
      const bool binaryOpen = (targetPC >= OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN);
      const uint8_t binaryTarget = binaryOpen ? 100 : 0;

      // If already at correct end-stop then nothing to do.
      if(binaryTarget == currentPC) { break; }

      // Refuse to close the valve while supply voltage low to try to avoid browning out or leaving valve shut.
      const bool low = ((NULL != lowBattOpt) && ((0 == lowBattOpt->read()) || lowBattOpt->isSupplyVoltageLow()));
      if(low && (targetPC < currentPC)) { break; }

      // If not apparently yet at end-stop
      // (ie not at correct end stop or with spurious unreconciled ticks)
      // then try again to run to end-stop.
      // If end-stop is hit then reset positional values.
      const bool tentative = (currentPC == 1) || (currentPC == 99);
      // Try running fast if not tentative from previous step, and end up tentative.
      if(!tentative && runTowardsEndStop(binaryOpen, low)) { resetPosition(binaryOpen, true); }
      // Else follow tentative by running slow to attempt to seat securely.
      else if(runTowardsEndStop(binaryOpen)) { resetPosition(binaryOpen, false); }
      // Re-estimate intermediate position.
      else { recomputePosition(); }

      break;
      }

    // Unexpected: go to error state, stop motor and report error on serial.
    default:
      {
      hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorOff, *this);
      OTV0P2BASE::serialPrintlnAndFlush(F("!valve error"));
      changeState(valveError);
      //panic(); // FIXME // Not expected to return.
      return;
      }
    }
  }

// Do valveCalibrating for proportional drive; returns true to return from poll() immediately.
// Calls changeState() directly if it needs to change state.
// If this returns false, processing falls through to that for the non-proportional case.
bool CurrentSenseValveMotorDirect::do_valveCalibrating_prop()
    {
    // Note that (re)calibration is needed / in progress.
    needsRecalibrating = true;

    // Defer calibration if doing it now would be a bad idea, eg in a bedroom at night.
    if(shouldDeferCalibration())
      {
      changeState(valveNormal);
      return(true);
      }

    // If taking stupidly long to calibrate
    // then assume a problem with the motor/mechanics and give up.
    // Don't panic() so that the unit can still (for example) transmit stats.
    if(++perState.valveCalibrating.wallclock2sTicks > MAX_TRAVEL_WALLCLOCK_2s_TICKS)
      {
      OTV0P2BASE::serialPrintlnAndFlush(F("!valve calibration fail"));
      changeState(valveError);
      return(true);
      }

    // Select activity based on micro-state.
    switch(perState.valveCalibrating.calibState)
      {
      case 0:
        {
#if 0 && defined(V0P2BASE_DEBUG)
V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("+calibrating");
#endif
        ++perState.valveCalibrating.calibState; // Move to next micro state.
        break;
        }
      case 1:
        {
        // Run fast to fully retracted (easy to fit, nomninally valve fully open).
        if(runFastTowardsEndStop(true))
          {
          // Reset tick count.
          ticksFromOpen = 0;
          ticksReverse = 0;
          perState.valveCalibrating.wallclock2sTicks = 0;
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
            perState.valveCalibrating.wallclock2sTicks = 0;
            ++perState.valveCalibrating.calibState; // Move to next micro state.
            break;
            }
          } while(getSubCycleTimeFn() <= computeSctAbsLimitDR());
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
              perState.valveCalibrating.wallclock2sTicks = 0;
              ++perState.valveCalibrating.calibState; // Move to next micro state.
              }
            break; // In all cases when end-stop hit don't try to run further in this sub-cycle.
            }
          } while(getSubCycleTimeFn() <= computeSctAbsLimitDR());
        break;
        }
      case 4:
        {
        // Set all measured calibration input parameters and current position.
        cp.updateAndCompute(perState.valveCalibrating.ticksFromOpenToClosed, perState.valveCalibrating.ticksFromClosedToOpen, minMotorDRTicks);

#if 0 && defined(V0P2BASE_DEBUG)
//V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("    ticksFromOpenToClosed: ");
//V0P2BASE_DEBUG_SERIAL_PRINT(cp.getTicksFromOpenToClosed());
//V0P2BASE_DEBUG_SERIAL_PRINTLN();
//V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("    ticksFromClosedToOpen: ");
//V0P2BASE_DEBUG_SERIAL_PRINT(cp.getTicksFromClosedToOpen());
//V0P2BASE_DEBUG_SERIAL_PRINTLN();
//V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("    precision %: ");
//V0P2BASE_DEBUG_SERIAL_PRINT(cp.getApproxPrecisionPC());
//V0P2BASE_DEBUG_SERIAL_PRINTLN();

OTV0P2BASE::serialPrintAndFlush(F("    ticksFromOpenToClosed: "));
OTV0P2BASE::serialPrintAndFlush(cp.getTicksFromOpenToClosed());
OTV0P2BASE::serialPrintlnAndFlush();
OTV0P2BASE::serialPrintAndFlush(F("    ticksFromClosedToOpen: "));
OTV0P2BASE::serialPrintAndFlush(cp.getTicksFromClosedToOpen());
OTV0P2BASE::serialPrintlnAndFlush();
OTV0P2BASE::serialPrintAndFlush(F("    precision %: "));
OTV0P2BASE::serialPrintAndFlush(cp.getApproxPrecisionPC());
OTV0P2BASE::serialPrintlnAndFlush();
#endif

        // Move to normal valve running state...
        needsRecalibrating = false;
        resetPosition(true); // Valve is currently fully open.
        changeState(valveNormal);
        return(true);
        }
      // In case of unexpected microstate shut down gracefully.
      default: { changeState(valveError); return(true); }
      }

  // Remain in valveCalibrating state, but don't do any non-prop processing.
  return(true);
  }

// Do valveNormal start for proportional drive; returns true to return from poll() immediately.
// Falls through to do drive to end stops or when in run-time binary-only mode.
// Calls changeState() directly if it needs to change state.
// If this returns false, processing falls through to that for the non-proportional case.
bool CurrentSenseValveMotorDirect::do_valveNormal_prop()
    {
    // Recalibrate if a serious tracking error was detected.
    if(needsRecalibrating)
      {
      // Defer calibration if doing it now would be a bad idea, eg in a bedroom at night.
      if(!shouldDeferCalibration())
          {
          changeState(valveCalibrating);
          return(true); // Leave poll().
          }
      }

    // If in non-proportional mode
    // then fall back to non-prop behaviour.
    if(inNonProprtionalMode())
        { return(false); } // Fall through.

    // If the desired target is close to either end
    // then fall back to non-prop behaviour and hit the end stops (fast) instead.
    // Makes ends 'sticky' and allows for some light-weight recalibration to scale ends.
    const uint8_t eps = cp.getApproxPrecisionPC();
    if((targetPC >= (100 - 2*eps)) ||
       (targetPC <= OTV0P2BASE::fnmax(2*eps, minOpenPC>>1)))
        { return(false); } // Fall through.

    // If close enough to the target position then stay as is and leave poll().
    // Carefully avoid overflow/underflow in comparison.
    if(((targetPC >= currentPC) && (targetPC <= currentPC + eps)) ||
       ((currentPC >= targetPC) && (currentPC <= targetPC + eps)))
        { return(true); } // Leave poll().

    // Move incrementally as close as possible to the target position.
    // Not open enough.
    if(targetPC > currentPC)
      {
      // TODO: use shaft encoder positioning by preference, ie when available.
      const bool hitEndStop = runTowardsEndStop(true);
      recomputePosition();
      // Hit the end-stop, possibly prematurely.
      if(hitEndStop)
        {
        // Report serious tracking error (well before 'fairly open' %).
        if(currentPC < OTV0P2BASE::fnmin(fairlyOpenPC, (uint8_t)(100 - 8*eps)))
          { reportTrackingError(); }
        // Silently auto-adjust when end-stop hit close to expected position.
        else
          { resetPosition(true); }
        }
#if 0 && defined(V0P2BASE_DEBUG)
V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("->");
#endif
      }
    // Not closed enough.
    else
      {
      // TODO: use shaft encoder positioning by preference, ie when available.
      const bool hitEndStop = runTowardsEndStop(false);
      recomputePosition();
      // Hit the end-stop, possibly prematurely.
      if(hitEndStop)
        {
        // Report serious tracking error.
        if(currentPC > OTV0P2BASE::fnmax(2*minOpenPC, 8*eps))
          { reportTrackingError(); }
        // Silently auto-adjust when end-stop hit close to expected position.
        else { resetPosition(false); }
        }
#if 0 && defined(V0P2BASE_DEBUG)
V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("-<");
#endif
      }

    // Remain in valveNormal state, but do no non-prop processing.
    return(true);
    }


#endif // CurrentSenseValveMotorDirect_DEFINED


}

