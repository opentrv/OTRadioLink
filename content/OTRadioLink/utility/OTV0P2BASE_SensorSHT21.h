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


// Base humidity sensor class.
// TODO: move to own header.
class HumiditySensorBase : public OTV0P2BASE::SimpleTSUint8Sensor
    {
    public:
      // Default high and low bounds on relative humidity for comfort and (eg) mite/mould growth.
      // See http://www.cdc.gov/niosh/topics/indoorenv/temperature.html: "The EPA recommends maintaining indoor relative humidity between 30 and 60% to reduce mold growth [EPA 2012]."
      static constexpr uint8_t HUMIDTY_HIGH_RHPC = 70;
      static constexpr uint8_t HUMIDTY_LOW_RHPC = 30;
      // Default epsilon bounds (absolute % +/- around thresholds) for accuracy and hysteresis.
      static constexpr uint8_t HUMIDITY_EPSILON_RHPC = 5;

      // If RH% rises by at least this per hour, then it may indicate occupancy.
      static constexpr uint8_t HUMIDITY_OCCUPANCY_PC_MIN_RISE_PER_H = 3;

      // Invalid (and initial) reading.
      static constexpr uint8_t INVALID_RH = 255;

    protected:
      // True if RH% is high, with hysteresis.
      // Marked volatile for thread-safe lock-free access.
      volatile bool highWithHyst = true;

    public:
      HumiditySensorBase() : SimpleTSUint8Sensor(INVALID_RH) { }

      // Does nothing: value remains invalid.
      virtual uint8_t read() override { return(value); }

      // Returns true if the sensor reading value passed is potentially valid, ie in range [0,100].
      virtual bool isValid(const uint8_t value) const override final { return(value <= 100); }

      // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
      // The lifetime of the pointed-to text must be at least that of the Sensor instance.
      virtual OTV0P2BASE::Sensor_tag_t tag() const override { return(V0p2_SENSOR_TAG_F("H|%")); }

      // True if RH% high.
      // Thread-safe and usable within ISRs (Interrupt Service Routines).
      bool isRHHigh() const { return(get() > (HUMIDTY_HIGH_RHPC+HUMIDITY_EPSILON_RHPC)); }

      // True if RH% high with a hysteresis band of 2 * HUMIDITY_EPSILON_RHPC.
      // Thread-safe and usable within ISRs (Interrupt Service Routines).
      bool isRHHighWithHyst() const { return(highWithHyst); }
    };

// Simple mock object for testing.
class HumiditySensorMock final : public HumiditySensorBase
  {
  public:
    // Set new value.
    bool set(uint8_t newValue) { value = newValue; highWithHyst = (newValue > HUMIDTY_HIGH_RHPC); return(true); }
    bool set(uint8_t newValue, bool _highWithHyst) { value = newValue; highWithHyst = _highWithHyst; return(true); }

    // Returns the existing value: use set() to set a new one.
    // Simplistically updates other flags and outputs based on current value.
    uint8_t read() override { return(get()); }

    // Reset to initial state; useful in unit tests.
    void reset() { value = INVALID_RH; highWithHyst = true; }
  };


#if defined(ARDUINO_ARCH_AVR) || defined(__arm__)

// Sensor for relative humidity percentage; 0 is dry, 100 is condensing humid, 255 for error.
// TODO: detect low supply voltage with user reg, and make isAvailable() return false if too low to be reliable.
#define HumiditySensorSHT21_DEFINED
class HumiditySensorSHT21 final : public HumiditySensorBase
  { public: virtual uint8_t read(); };

// SHT21 sensor for ambient/room temperature in 1/16th of one degree Celsius.
// TODO: detect low supply voltage with user reg, and make isAvailable() return false if too low to be reliable.
#define RoomTemperatureC16_SHT21_DEFINED
class RoomTemperatureC16_SHT21 final : public OTV0P2BASE::TemperatureC16Base
  { public: virtual int16_t read(); };

#endif // ARDUINO_ARCH_AVR || __arm__


// Placeholder with dummy static status methods to reduce conditional-compilation complexity.
class DummyHumiditySensor final
  {
  public:
    constexpr static bool isAvailable() { return(false); } // Not available, so always returns false.
    constexpr static bool isRHHigh() { return(false); } // Unknown, so always false.
    constexpr static bool isRHHighWithHyst() { return(false); } // Unknown, so always false.
    constexpr static uint8_t get() { return(0); }
    constexpr static uint8_t read() { return(0); }
  };
// Previous name as used in V0p2_Main.
typedef DummyHumiditySensor DummyHumiditySensorSHT21;


}
#endif
