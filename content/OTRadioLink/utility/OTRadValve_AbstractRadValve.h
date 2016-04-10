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
 * OpenTRV abstract/base (thermostatic) radiator valve driver class.
 *
 * Also includes some common supporting base/interface classes.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_ABSTRACTRADVALVE_H
#define ARDUINO_LIB_OTRADVALVE_ABSTRACTRADVALVE_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


// Abstract class for motor drive.
// Supports abstract model plus remote (wireless) and local/direct implementations.
// Implementations may require read() called at a fixed rate,
// though should tolerate calls being skipped when time is tight for other operations,
// since read() may take substantial time (hundreds of milliseconds).
// Implementations must document when read() calls are critical,
// and/or expose alternative API for the time-critical elements.
class AbstractRadValve : public OTV0P2BASE::SimpleTSUint8Actuator
  {
  protected:
    // Prevent direct creation of naked instance of this base/abstract class.
    AbstractRadValve() { }

  public:
    // Returns true if this target valve open % value passed is valid, ie in range [0,100].
    virtual bool isValid(const uint8_t value) const { return(value <= 100); }

    // Set new target valve percent open.
    // Ignores invalid values.
    // Some implementations may ignore/reject all attempts to directly set the values.
    // If this returns true then the new target value was accepted.
    virtual bool set(const uint8_t /*newValue*/) { return(false); }

    // Call when given user signal that valve has been fitted (ie is fully on).
    // By default does nothing (no valve fitting may be needed).
    virtual void signalValveFitted() { }

    // Waiting for indication that the valve head has been fitted to the tail.
    // By default returns false (no valve fitting may be needed).
    virtual bool isWaitingForValveToBeFitted() const { return(false); }

    // Returns true iff not in error state and not (re)calibrating/(re)initialising/(re)syncing.
    // By default there is no recalibration step.
    virtual bool isInNormalRunState() const { return(true); }

    // Returns true if in an error state.
    // May be recoverable by forcing recalibration.
    virtual bool isInErrorState() const { return(false); }

    // True if the controlled physical valve is thought to be at least partially open right now.
    // If multiple valves are controlled then is this true only if all are at least partially open.
    // Used to help avoid running boiler pump against closed valves.
    // Must not be true while (re)calibrating.
    // The default is to use the check the current computed position
    // against the minimum open percentage.
    virtual bool isControlledValveReallyOpen() const { return(isInNormalRunState() && (value >= getMinPercentOpen())); }

    // Get estimated minimum percentage open for significant flow for this device; strictly positive in range [1,99].
    // Defaults to 1 which is minimum possible legitimate value.
    virtual uint8_t getMinPercentOpen() const { return(1); }

    // Minimally wiggles the motor to give tactile feedback and/or show to be working.
    // May take a significant fraction of a second.
    // Finishes with the motor turned off.
    // May also be used to (re)calibrate any shaft/position encoder and end-stop detection.
    // By default does nothing.
    virtual void wiggle() { }
  };


// Null radiator valve driver implementation.
// Never in normal (nor error) state.
class NullRadValve : public AbstractRadValve
  {
  public:
    // Returns true iff not in error state and not (re)calibrating/(re)initialising/(re)syncing.
    // Always false for null implementation.
    virtual bool isInNormalRunState() const { return(false); }
  };



// Default minimum valve percentage open to be considered actually/significantly open; [1,99].
// Anything like this will usually be shut or very minimal flows.
// Setting this above 0 delays calling for heat from a central boiler until water is likely able to flow.
// (It may however be possible to scavenge some heat if a particular valve opens below this and the circulation pump is already running, for example.)
// DHD20130522: FHT8V + valve heads in use have not typically been open until around 6%; at least one opens at ~20%.
// DHD20151014: may need reduction to <5 for use in high-pressure systems.
// DHD20151030: with initial dead-reckoning direct drive impl valves may not be open until ~45%.
// Allowing valve to linger at just below this level without calling for heat when shutting
// may allow comfortable boiler pump overrun in older systems with no/poor bypass to avoid overheating.
static const uint8_t DEFAULT_VALVE_PC_MIN_REALLY_OPEN = 15;

// Safer value for valves to very likely be significantly open, in range [DEFAULT_VALVE_PC_MIN_REALLY_OPEN+1,DEFAULT_VALVE_PC_MODERATELY_OPEN-1].
// NOTE: below this value is likely to let a boiler switch off also,
// ie a value at/above this is a definite call for heat.
// so DO NOT CHANGE this value between boiler and valve code without good reason.
// DHD20151030: with initial dead-reckoning direct drive impl valves may not be open until ~45%.
static const uint8_t DEFAULT_VALVE_PC_SAFER_OPEN = 50;

// Default valve percentage at which significant heating power is being provided [DEFAULT_VALVE_PC_SAFER_OPEN+1,99].
// For many valves much of the time this may be effectively fully open,
// ie no change beyond this makes significant difference to heat delivery.
// NOTE: at/above this value is likely to force a boiler on also,
// so DO NOT CHANGE this value between boiler and valve code without good reason.
// Should be significantly higher than DEFAULT_MIN_VALVE_PC_REALLY_OPEN.
// DHD20151014: has been ~33% but ~66% more robust, eg for tricky all-in-one units.
static const uint8_t DEFAULT_VALVE_PC_MODERATELY_OPEN = 67;





// Generic callback handler for hardware valve motor driver.
class HardwareMotorDriverInterfaceCallbackHandler
  {
  public:
    // Called when end stop hit, eg by overcurrent detection.
    // Can be called while run() is in progress.
    // Is ISR-/thread- safe.
    virtual void signalHittingEndStop(bool opening) = 0;

    // Called when encountering leading edge of a mark in the shaft rotation in forward direction (falling edge in reverse).
    // Can be called while run() is in progress.
    // Is ISR-/thread- safe.
    virtual void signalShaftEncoderMarkStart(bool opening) = 0;

    // Called with each motor run sub-cycle tick.
    // Is ISR-/thread- safe.
    virtual void signalRunSCTTick(bool opening) = 0;
  };

// Trivial do-nothing implementation of HardwareMotorDriverInterfaceCallbackHandler.
class NullHardwareMotorDriverInterfaceCallbackHandler : public HardwareMotorDriverInterfaceCallbackHandler
  {
  public:
    virtual void signalHittingEndStop(bool) { }
    virtual void signalShaftEncoderMarkStart(bool) { }
    virtual void signalRunSCTTick(bool) { }
  };

// Minimal end-stop-noting implementation of HardwareMotorDriverInterfaceCallbackHandler.
// The field endStopHit should be cleared before starting/running the motor.
class EndStopHardwareMotorDriverInterfaceCallbackHandler : public HardwareMotorDriverInterfaceCallbackHandler
  {
  public:
    bool endStopHit;
    virtual void signalHittingEndStop(bool) { endStopHit = true; }
    virtual void signalShaftEncoderMarkStart(bool) { }
    virtual void signalRunSCTTick(bool) { }
  };


// Interface/base for low-level hardware motor driver.
class HardwareMotorDriverInterface
  {
  public:
    // Legal motor drive states.
    enum motor_drive
      {
      motorOff = 0, // Motor switched off (default).
      motorDriveClosing, // Drive towards the valve-closed position.
      motorDriveOpening, // Drive towards the valve-open position.
      motorStateInvalid // Higher than any valid state.
      };

  public:
    // Detect (poll) if end-stop is reached or motor current otherwise very high.
    virtual bool isCurrentHigh(HardwareMotorDriverInterface::motor_drive mdir = motorDriveOpening) const = 0;

    // Poll simple shaft encoder output; true if on mark, false if not or if unused for this driver.
    virtual bool isOnShaftEncoderMark() const { return(false); }

    // Call to actually run/stop motor.
    // May take as much as (say) 200ms eg to change direction.
    // Stopping (removing power) should typically be very fast, << 100ms.
    //   * maxRunTicks  maximum sub-cycle ticks to attempt to run/spin for);
    //     0 will run for shortest reasonable time and may raise or ignore stall current limits,
    //     ~0 will run as logn as possible and may attempt to ride through sticky mechanics
    //     eg with some run time ignoring stall current entirely
    //   * dir  direction to run motor (or off/stop)
    //   * callback  callback handler
    virtual void motorRun(uint8_t maxRunTicks, motor_drive dir, HardwareMotorDriverInterfaceCallbackHandler &callback) = 0;
  };


    }

#endif
