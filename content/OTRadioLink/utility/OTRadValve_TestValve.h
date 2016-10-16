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

Author(s) / Copyright (s): Deniz Erbilgin 2016
                           Damon Hart-Davis 2016
*/

/*
 * Driver for DORM1/REV7 direct motor drive.
 * TODO!!! Rename!
 */


#ifndef CONTENT_OTRADIOLINK_UTILITY_OTRADVALVE_TESTVALVE_H_
#define CONTENT_OTRADIOLINK_UTILITY_OTRADVALVE_TESTVALVE_H_


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include "OTRadValve_AbstractRadValve.h"
#include "OTRadValve_ValveMotorDirectV1.h"


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


/**
 * @class   TestValveMotor
 * @brief   Generic high level motor driver with minimal logic. Intended for battery and motor drive testing.
 */
class TestValveMotor : public OTRadValve::HardwareMotorDriverInterfaceCallbackHandler
  {
  private:
    // Hardware interface instance, passed by reference.
    // Must have a lifetime exceeding that of this enclosing object.
    OTRadValve::HardwareMotorDriverInterface * const hw;

    // Direction the motor is running in.
    volatile bool state;

    // Flag set on signalHittingEndStop() callback from end-top / stall / high-current input.
    // Marked volatile for thread-safe lock-free access (with care).
    volatile bool endStopDetected;

    // Stores the number of times motor has hit an endstop.
    volatile uint32_t counter;

    // Run fast towards/to end stop as far as possible in this call.
    // Terminates significantly before the end of the sub-cycle.
    // Possibly allows partial recalibration, or at least re-homing.
    // Returns true if end-stop has apparently been hit,
    // else will require one or more further calls in new sub-cycles
    // to hit the end-stop.
    // May attempt to ride through stiff mechanics.
    // Prints counter to serial in hex each time an end stop has been reached.
    bool runFastTowardsEndStop(bool toOpen);

  public:
    // Create an instance, passing in a reference to the non-NULL hardware driver.
    // The hardware driver instance lifetime must be longer than this instance.
    TestValveMotor(OTRadValve::HardwareMotorDriverInterface * const hwDriver)
  	  : hw(hwDriver), state(false), endStopDetected(false), counter(0) {}

    /**
     * @brief   Updates the state of the motor logic.
     *          - Will run until end stop is detected and then reverse
     *          the direction of the motor and increment counter.
     */
    void poll();

    /**
     * @brief   Returns the end stop counter.
     */
    inline uint32_t getCounter() { return counter; }

    // Unused abstract functions.
    void signalHittingEndStop(bool) { endStopDetected = true; }
    void signalShaftEncoderMarkStart(bool) {}
    void signalRunSCTTick(bool) {}
  };


    }


#endif /* CONTENT_OTRADIOLINK_UTILITY_OTRADVALVE_TESTVALVE_H_ */
