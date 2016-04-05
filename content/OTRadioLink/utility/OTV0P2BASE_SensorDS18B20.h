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

Author(s) / Copyright (s): Damon Hart-Davis 2014--2016
                           Jeremy Poulter 2016
*/

/*
 DS18B20 OneWire(TM) temperature detector.
 */

#ifndef OTV0P2BASE_SENSORDS18B20_H
#define OTV0P2BASE_SENSORDS18B20_H

//#include "OTV0P2BASE_Util.h"
#include "OTV0P2BASE_MinOW.h"
#include "OTV0P2BASE_Sensor.h"
#include "utility/OTV0P2BASE_SensorTemperatureC16Base.h"


namespace OTV0P2BASE
{


// External/off-board DS18B20 temperature sensor in nominal 1/16 C.
// Requires OneWire support.
// Will in future be templated on:
//   * the MinimalOneWire instance to use
//   * precision (9, 10, 11 or 12 bits, 12 for the full C/16 resolution),
//     noting that lower precision is faster,
//     and for example 1C will be 0x1X
//     with more bits of the final nibble defined for with higher precision
//   * enumeration order of this device on the OW bus,
//     with 0 (the default) being the first found by the usual deterministic scan
//   * whether the CRC should de checked for incoming data
//     to improve reliability on long connections at a code and CPU cost
// Multiple DS18B20s can nominally be supported on one or multiple OW buses.
// Not all template parameter combinations may be supported.
// Provides temperature as a signed int value with 0C == 0 at all precisions.
class TemperatureC16_DS18B20 : public TemperatureC16Base
  {
  private:
    // Reference to minimal OneWire support instance for appropriate GPIO.
    OTV0P2BASE::MinimalOneWireBase &minOW;

    // True once initialised.
    bool initialised;

    // Precision in range [9,12].
    const uint8_t precision;

    // The number of sensors found on the bus
    uint8_t sensorCount;

    // Initialise the device (if any) before first use.
    // Returns true iff successful.
    // Uses specified order DS18B20 found on bus.
    // May need to be reinitialised if precision changed.
    bool init();

  public:
    // Minimum supported precision, in bits, corresponding to 1/2 C resolution.
    static const uint8_t MIN_PRECISION = 9;
    // Maximum supported precision, in bits, corresponding to 1/16 C resolution.
    static const uint8_t MAX_PRECISION = 12;
    // Default precision; defaults to minimum for speed.
    static const uint8_t DEFAULT_PRECISION = MIN_PRECISION;

    // Returns number of useful binary digits after the binary point.
    // 8 less than total precision for DS18B20.
    virtual int8_t getBitsAfterPoint() const { return(precision - 8); }

    // Returns true if this sensor is definitely unavailable or behaving incorrectly.
    // This is after an attempt to initialise has not found a DS18B20 on the bus.
    virtual bool isUnavailable() const { return(initialised && (0 == sensorCount)); }

    // Create instance with given OneWire connection, bus ordinal and precision.
    // No two instances should attempt to target the same DS18B20,
    // though different DS18B20s on the same bus or different buses is allowed.
    // Precision defaults to minimum (9 bits, 0.5C resolution) for speed.
    TemperatureC16_DS18B20(OTV0P2BASE::MinimalOneWireBase &ow, uint8_t _precision = DEFAULT_PRECISION)
      : minOW(ow), initialised(false), precision(constrain(_precision, MIN_PRECISION, MAX_PRECISION))
      { }

    // Get current precision in bits [9,12]; 9 gives 1/2C resolution, 12 gives 1/16C resolution.
    uint8_t getPrecisionBits() const { return(precision); }

    // return the number of DS18B20 sensors on the bus
    uint8_t getSensorCount();

    // Force a read/poll of temperature and return the value sensed in nominal units of 1/16 C.
    // At sub-maximum precision lsbits will be zero or undefined.
    // Expensive/slow.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    // When multiple DS18B20 are connected this will read the 'first' one, use ReadMultiple to read the 
    // values from more than the just the first
    virtual int16_t read();

    // Force a read/poll of temperature from multiple DS18B20 sensors; returns number of values read.
    // The value sensed, in nominal units of 1/16 C,
    // is written to the array of uint16_t (with count elements) pointed to by values.
    // The values are written in the order they are found on the One-Wire bus.
    // index specifies the sensor to start reading at 0 being the first (and the default).
    // This can be used to read more sensors
    // than elements in the values array.
    // The return is the number of values read.
    // At sub-maximum precision lsbits will be zero or undefined.
    // Expensive/slow.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    uint8_t readMultiple(int16_t *values, uint8_t count, uint8_t index = 0);
  };


}
#endif
