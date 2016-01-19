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
 Abstract temperature sensor in 1/16th of one degree Celsius.
 */

#ifndef OTV0P2BASE_SENSORDSTEMPERATUREC16BASE_H
#define OTV0P2BASE_SENSORDSTEMPERATUREC16BASE_H

#include "OTV0P2BASE_Sensor.h"


namespace OTV0P2BASE
{


// Abstract temperature sensor in 1/16th of one degree Celsius.
// Nominally covers a range from well below 0C to at least 100C
// for room and DHW temperature monitoring.
// May cover a wider range for other specialist monitoring.
// Some devices may indicate an error by returning a zero or (very) negative value.
// A returned value can be tested for validity with isErrorValue().
class TemperatureC16Base : public OTV0P2BASE::Sensor<int16_t>
  {
  protected:
    // Prevent instantiation of a naked instance.
    TemperatureC16Base() { }

  public:
    // Returns true if the given value indicates, or may indicate, an error.
    // If false then the value passed is likely legitimate.
    virtual bool isErrorValue(int16_t value) const = 0;

    // Returns number of useful binary digits after the binary point; default is 4.
    // May be negative if some of the digits BEFORE the binary point are not usable.
    // Some sensors may dynamically return fewer places.
    virtual int8_t getBitsAfterPoint() const { return(4); }

    // Returns true if fewer than 4 bits of useful data after the binary point.
    bool isLowPrecision() const { return(getBitsAfterPoint() < 4); }
  };


}
#endif
