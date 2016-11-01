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
  public:
    // Error value returned if device unavailable or not yet read.
    // Negative and below minimum value that DS18B20 can return legitimately (-55C).
    static const int16_t DEFAULT_INVALID_TEMP = -128 * 16; // Nominally -128C.

  protected:
    // Room temperature in 16*C, eg 1 is 1/16 C, 32 is 2C, -64 is -4C.
    int16_t value;

    // Prevent instantiation of a naked instance.
    // Starts off with a detectably-invalid value, eg for before read() is called first.
    TemperatureC16Base() : value(DEFAULT_INVALID_TEMP) { }

  public:
    // Returns true if the given value indicates, or may indicate, an error.
    // If false then the value passed is likely legitimate.
    virtual bool isErrorValue(int16_t value) const { return(DEFAULT_INVALID_TEMP == value); }

    // Returns number of useful binary digits after the binary point; default is 4.
    // May be negative if some of the digits BEFORE the binary point are not usable.
    // Some sensors may dynamically return fewer places.
    virtual int8_t getBitsAfterPoint() const { return(4); }

    // Returns true if fewer than 4 bits of useful data after the binary point.
    bool isLowPrecision() const { return(getBitsAfterPoint() < 4); }

    // Preferred poll interval (in seconds).
    // This should be called at a regular rate, usually 1/60, so make stats such as velocity measurement easier.
    virtual uint8_t preferredPollInterval_s() const { return(60); }

    // Return last value fetched by read(); undefined before first read().
    // Fast.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    virtual int16_t get() const { return(value); }

    // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
    // The lifetime of the pointed-to text must be at least that of the Sensor instance.
    virtual const char *tag() const { return("T|C16"); }
  };

// Extension of TemperatureC16Base primarily for mocking in unit tests.
class TemperatureC16Mock : public TemperatureC16Base
  {
  public:
    // Set new value.
    virtual bool set(const int16_t newValue) { value = newValue; return(true); }
    // Returns the existing value: use set() to set a new one.
    int16_t read() override { return(value); }
  };

}
#endif
