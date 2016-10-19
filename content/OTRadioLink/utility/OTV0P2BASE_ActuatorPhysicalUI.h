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
    // Returns true if this target valve open % value passed is valid, ie in range [0,100].
    virtual bool isValid(const uint8_t value) const { return(value <= 100); }

    // Set new target valve percent open.
    // Ignores invalid values.
    // Some implementations may ignore/reject all attempts to directly set the values.
    // If this returns true then the new target value was accepted.
    virtual bool set(const uint8_t /*newValue*/) { return(false); }


  };


    }

#endif
