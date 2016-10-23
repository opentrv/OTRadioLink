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
 * Notion of basic mode of OpenTRV thermostatic radiator valve: FROST, WARM or BAKE.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_VALVEMODE_H
#define ARDUINO_LIB_OTRADVALVE_VALVEMODE_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include "OTV0P2BASE_Sensor.h"


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
class ValveMode : public OTV0P2BASE::SimpleTSUint8Sensor
  {
  public:
    typedef enum { VMODE_FROST, VMODE_WARM, VMODE_BAKE } mode_t;

    // Returns true if the mode value passed is valid, ie in range [0,2].
    virtual bool isValid(const uint8_t value) const { return(value <= VMODE_BAKE); }

    // Set new mode value; if VMODE_BAKE restarts the BAKE period.
    // Ignores invalid values.
    // If this returns true then the new target value was accepted.
    virtual bool set(const uint8_t newValue);

    // Overload to allow set directly with enum value.
    bool set(const mode_t newValue) { return(set((uint8_t) newValue)); }

  };



#endif
