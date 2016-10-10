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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2016
*/

/*
 SHT21 temperature and relative humidity sensor.

 V0p2/AVR specific for now.
 */

#ifndef OTV0P2BASE_SENSORSHT21_H
#define OTV0P2BASE_SENSORSHT21_H

#include "OTV0P2BASE_SensorTemperatureC16Base.h"


namespace OTV0P2BASE
{


#ifdef ARDUINO_ARCH_AVR

// SHT21 sensor for ambient/room temperature in 1/16th of one degree Celsius.
#define RoomTemperatureC16_SHT21_DEFINED
class RoomTemperatureC16_SHT21 : public OTV0P2BASE::TemperatureC16Base
  { public: virtual int16_t read(); };


// Sensor for relative humidity percentage; 0 is dry, 100 is condensing humid, 255 for error.
#define HumiditySensorSHT21_DEFINED
class HumiditySensorSHT21 : public OTV0P2BASE::SimpleTSUint8Sensor
  {
  public:
    // High and low bounds on relative humidity for comfort and (eg) mite/mould growth.
    // See http://www.cdc.gov/niosh/topics/indoorenv/temperature.html: "The EPA recommends maintaining indoor relative humidity between 30 and 60% to reduce mold growth [EPA 2012]."
    static const uint8_t HUMIDTY_HIGH_RHPC = 70;
    static const uint8_t HUMIDTY_LOW_RHPC = 30;
    // Epsilon bounds (absolute % +/- around thresholds) for accuracy and hysteresis.
    static const uint8_t HUMIDITY_EPSILON_RHPC = 5;
    //#if ((HUMIDTY_HIGH_RHPC + HUMIDITY_EPSILON_RHPC) >= 100)
    //#error bad RH constants!
    //#endif
    //#if ((HUMIDTY_LOW_RHPC - HUMIDITY_EPSILON_RHPC) <= 0)
    //#error bad RH constants!
    //#endif

    // If RH% rises by at least this per hour, then it may indicate occupancy.
    static const uint8_t HUMIDITY_OCCUPANCY_PC_MIN_RISE_PER_H = 3;

  private:
    // True if RH% is high, with hysteresis.
    // Marked volatile for thread-safe lock-free access.
    volatile bool highWithHyst;

  public:
    HumiditySensorSHT21() : SimpleTSUint8Sensor(255), highWithHyst(false) { }

    // Force a read/poll of the relative humidity % and return the value sensed [0,100] (dry to wet).
    // Initially (and in case of error) the value 255 is returned as a fail-safe.
    // Potentially expensive/slow.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    virtual uint8_t read();

    // Returns true if the sensor reading value passed is potentially valid, ie in range [0,100].
    virtual bool isValid(const uint8_t value) const { return(value <= 100); }

    // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
    // The lifetime of the pointed-to text must be at least that of the Sensor instance.
    virtual const char *tag() const { return("H|%"); }

    // True if RH% high.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    bool isRHHigh() { return(get() > (HUMIDTY_HIGH_RHPC+HUMIDITY_EPSILON_RHPC)); }

    // True if RH% high with a hysteresis band of 2 * HUMIDITY_EPSILON_RHPC.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    bool isRHHighWithHyst() { return(highWithHyst); }
  };

#endif // ARDUINO_ARCH_AVR

// Placeholder namespace with dummy static status methods to reduce code complexity.
class DummyHumiditySensorSHT21
  {
  public:
    // Not available, so always returns false.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    static bool isAvailable() { return(false); }

    // Unknown, so always false.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    static bool isRHHigh() { return(false); }

    // Unknown, so always false.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    static bool isRHHighWithHyst() { return(false); }
  };


}
#endif
