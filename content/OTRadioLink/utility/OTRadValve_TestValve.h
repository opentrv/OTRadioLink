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
*/

/*
 * Driver for DORM1/REV7 direct motor drive.
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


// Generic (unit-testable) motor driver login using end-stop detection and simple shaft-encoder.
// Designed to be embedded in a motor controller instance.
// This used the sub-cycle clock for timing.
// This is sensitive to sub-cycle position, ie will try to avoid causing a main loop overrun.
// May report some key status on Serial, with any error line(s) starting with "!'.
class TestValveMotor : public OTRadValve::HardwareMotorDriverInterfaceCallbackHandler
  {
  private:
    // Hardware interface instance, passed by reference.
    // Must have a lifetime exceeding that of this enclosing object.
    OTRadValve::HardwareMotorDriverInterface * const hw;

    // Major state of driver.
    // On power-up (or full reset) should be 0/init.
    // Stored as a uint8_t to save a little space and to make atomic operations easier.
    // Marked volatile so that individual reads are ISR-/thread- safe without a mutex.
    // Hold a mutex to perform compound operations such as read/modify/write.
    // Change state with changeState() which will do some other book-keeping.
    volatile bool state;

    // Flag set on signalHittingEndStop() callback from end-top / stall / high-current input.
    // Marked volatile for thread-safe lock-free access (with care).
    volatile bool endStopDetected;

    volatile uint32_t counter;

    // Run fast towards/to end stop as far as possible in this call.
    // Terminates significantly before the end of the sub-cycle.
    // Possibly allows partial recalibration, or at least re-homing.
    // Returns true if end-stop has apparently been hit,
    // else will require one or more further calls in new sub-cycles
    // to hit the end-stop.
    // May attempt to ride through stiff mechanics.
    bool runFastTowardsEndStop(bool toOpen);

  public:
    // Create an instance, passing in a reference to the non-NULL hardware driver.
    // The hardware driver instance lifetime must be longer than this instance.
    TestValveMotor(OTRadValve::HardwareMotorDriverInterface * const hwDriver)
  	  : hw(hwDriver), state(false), endStopDetected(false), counter(0) {}

    // Poll.
    // Regular poll every 1s or 2s,
    // though tolerates missed polls eg because of other time-critical activity.
    // May block for hundreds of milliseconds.
    void poll();

    // unused abstract functions
    void signalHittingEndStop(bool) {}
    void signalShaftEncoderMarkStart(bool) {}
    void signalRunSCTTick(bool) {}


  };


    }


#endif /* CONTENT_OTRADIOLINK_UTILITY_OTRADVALVE_TESTVALVE_H_ */
