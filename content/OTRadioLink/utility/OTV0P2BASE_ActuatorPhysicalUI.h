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

Author(s) / Copyright (s): Damon Hart-Davis 2016
*/

/*
 * OpenTRV radiator valve physical UI controls and output as an actuator.
 */

#ifndef ARDUINO_LIB_ACTUATOR_PHYSICAL_UI_H
#define ARDUINO_LIB_ACTUATOR_PHYSICAL_UI_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include <OTV0P2BASE_Actuator.h>


// Use namespaces to help avoid collisions.
namespace OTV0P2BASE
    {


// Base class for physical UI controls on V0p2 devices.
class ActuatorPhysicalUIBase : public OTV0P2BASE::SimpleTSUint8Actuator
  {
  protected:
    // Prevent direct creation of naked instance of this base/abstract class.
    ActuatorPhysicalUIBase() { }

  public:
    // Set a new target output indication, eg mode.
    // If this returns true then the new target value was accepted.
    virtual bool set(const uint8_t /*newValue*/) { return(false); }

    // Call this on even numbered seconds (with current time in seconds) to allow the UI to operate.
    // Should never be skipped, so as to allow the UI to remain responsive.
    // Runs in 350ms or less; usually takes only a few milliseconds or microseconds.
    // Returns a non-zero value iff the user interacted with the system, and maybe caused a status change.
    // NOTE: since this is on the minimum idle-loop code path, minimise CPU cycles, esp in frost mode.
    // Also re-activates CLI on main button push.
    virtual uint8_t read();

    // Preferred poll interval (in seconds); should be called at constant rate, usually 1/60s.
    virtual uint8_t preferredPollInterval_s() const { return(2); }
  };


// Null UI: always returns false for read() and does nothing with set().
// Has no physical interactions with devices.
class NullActuatorPhysicalUI : public ActuatorPhysicalUIBase
  {
  // Does nothing and forces 'sensor' value to 0 and returns 0.
  virtual uint8_t read() { value = 0; return(value); }
  };

    }

#endif
