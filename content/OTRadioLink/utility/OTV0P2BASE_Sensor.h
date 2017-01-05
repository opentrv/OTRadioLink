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

Author(s) / Copyright (s): Damon Hart-Davis 2014--2017
*/

/*
 Base sensor type for simple sensors returning scalar values.

 Most sensors should derive from this.

 May also be used for pseudo-sensors
 such as synthesised from multiple sensors combined.
 */

#ifndef OTV0P2BASE_SENSOR_H
#define OTV0P2BASE_SENSOR_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "OTV0P2BASE_ArduinoCompat.h"
#endif

#include <stdint.h>
#include <stddef.h>


namespace OTV0P2BASE
{


// Type used for Sensor tags items.
// Generally const char * but may be special type to be placed in MCU code space eg on AVR.
#if defined(ARDUINO)
#define V0p2_SENSOR_TAG_NOT_SIMPLECHARPTR
#define V0p2_SENSOR_TAG_IS_FlashStringHelper
#define V0p2_SENSOR_TAG_F(literal) (F(literal))
typedef const __FlashStringHelper *Sensor_tag_t;
#else
#define V0p2_SENSOR_TAG_F(literal) (literal)
typedef const char *Sensor_tag_t;
#endif


// Minimal lightweight sensor subset.
// Contains just enough to check availability and to name and get the latest value.
template <class T>
class SensorCore
  {
  public:
    // Type of sensed data.
    typedef T data_t;

    // Return last value fetched by read(); undefined before first read().
    // Usually fast.
    // Often likely to be thread-safe or usable within ISRs (Interrupt Service Routines),
    // BUT READ IMPLEMENTATION DOCUMENTATION BEFORE TREATING AS thread/ISR-safe.
    virtual T get() const = 0;

    // Returns true if this sensor is currently available.
    // True by default unless implementation overrides.
    // For those sensors that need starting this will be false before begin().
    virtual bool isAvailable() const { return(true); }

    // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
    // The lifetime of the pointed-to text must be at least that of the Sensor instance.
    virtual Sensor_tag_t tag() const { return(NULL); }

#if 0 // Defining the virtual destructor uses ~800+ bytes of Flash by forcing use of malloc()/free().
    // Ensure safe instance destruction when derived from.
    // by default attempts to shut down the sensor and otherwise free resources when done.
    // This uses ~800+ bytes of Flash by forcing use of malloc()/free().
    virtual ~SensorCore() { end(); }
#else
#define SENSORCORE_NO_VIRT_DEST // Beware, no virtual destructor so be careful of use via base pointers.
#endif
  };

// Base sensor type.
// Templated on sensor value type, typically uint8_t or uint16_t or int16_t.
template <class T>
class Sensor : public SensorCore<T>
  {
  public:
    // Force a read/poll of this sensor and return the value sensed.
    // May be expensive/slow.
    // For many implementations read() should be called at a reasonably steady rate,
    // see preferredPollInterval_s().
    // Unlikely to be thread-safe or usable within ISRs (Interrupt Service Routines).
    // Individual implementations can document alternative behaviour.
    virtual T read() = 0;

    // Returns true if this sensor reading value passed is potentially valid, eg in-range.
    // Default is to always return true, ie all values potentially valid.
    virtual bool isValid(T /*value*/) const { return(true); }

    // Returns non-zero if this implementation requires a regular call to read() to operate correctly.
    // Preferred poll interval (in seconds) or 0 if no regular poll() call required.
    // Default returns 0 indicating regular call to read() not required,
    // only as required to fetch new values from the underlying sensor.
    virtual uint8_t preferredPollInterval_s() const { return(0); }

//    // Returns a suggested privacy/sensitivity level of the data from this sensor.
//    // The default sensitivity is set to just forbid transmission at default (255) leaf settings.
//    virtual uint8_t sensitivity() const { return(254 /* stTXsecOnly */ ); }

    // Handle simple interrupt for this sensor.
    // Must be fast and ISR (Interrupt Service Routine) safe.
    // Returns true if interrupt was successfully handled and cleared
    // else another interrupt handler in the chain may be called
    // to attempt to clear the interrupt.
    // By default does nothing (and returns false).
    virtual bool handleInterruptSimple() { return(false); }

#if 0 // Defining the virtual destructor uses ~800+ bytes of Flash by forcing use of malloc()/free().
    // Ensure safe instance destruction when derived from.
    // by default attempts to shut down the sensor and otherwise free resources when done.
    // This uses ~800+ bytes of Flash by forcing use of malloc()/free().
    virtual ~Sensor() { end(); }
#else
#define SENSOR_NO_VIRT_DEST // Beware, no virtual destructor so be careful of use via base pointers.
#endif
  };


// Simple mainly thread-safe uint8_t-valued sensor.
// Made thread-safe in get() by marking the value volatile
// providing that read() is careful to do any compound operations on value
// (eg more than simple read or simple write, or involving unwanted intermediate states)
// under a proper lock, eg excluding interrupts.
class SimpleTSUint8Sensor : public Sensor<uint8_t>
  {
  protected:
    // The current sensor value, as fetched/computed by read().
    volatile uint8_t value;

    // By default initialise the value to zero.
    constexpr SimpleTSUint8Sensor() : value(0) { }
    // Can initialise to a chosen value.
    constexpr SimpleTSUint8Sensor(const uint8_t v) : value(v) { }

  public:
    // Return last value fetched by read(); undefined before first read().
    // Usually fast.
    // Likely to be thread-safe or usable within ISRs (Interrupt Service Routines),
    // BUT READ IMPLEMENTATION DOCUMENTATION BEFORE TREATING AS thread/ISR-safe.
    virtual uint8_t get() const override { return(value); }
  };


// Sub-sensor / facade.
// This sub-sensor's value is derived from another sensor value,
// and so can be considered low priority by default.
template <class T, bool lowPri = true>
class SubSensor : public SensorCore<T>
  {
  public:
    // True if this stat is to be treated as low priority / low information by default
    static constexpr bool lowPriority = lowPri;
  };

// Sub-sensor / facade wrapping direct reference to the underlying (non-volatile) variables.
// This should be efficient and simple but is not always usable based on parent sensor implementation.
// This holds the tag directly.
// This version does not override isAvailable() and so it returns true.
template <class T, bool lowPri = true>
class SubSensorSimpleRef final : public SubSensor<T, lowPri>
  {
  private:
    const T &v;
    const Sensor_tag_t t;
  public:
    constexpr SubSensorSimpleRef(const T &valueByRef, const Sensor_tag_t tag) : v(valueByRef), t(tag) { }
    virtual T get() const override { return(v); }
    virtual Sensor_tag_t tag() const override { return(t); }
  };

// Sub-sensor / facade wrapping calls for the key methods in the specified parent class.
//   * T  sensor data type
//   * P  parent Sensor object type
// Constructor parameters:
//   * tagFn  pointer to member function of P to get the tag value; never NULL
//   * getFn  pointer to member function of P to get the sensor value; never NULL
//   * isAvailableFnOpt  pointer to member function of P to get the isAvailable() value; if NULL then isAvailable() always returns true
//
// NOTE: DO NOT USE: g++ 4.9.x seems to generate hideously inefficient code; >100 bytes for a single call via member function pointer.
template <class P, class T, bool lowPri = true>
class SubSensorByCallback final : public SubSensor<T, lowPri>
  {
  private:
      const P *const p;
      Sensor_tag_t (P::*const t)() const;
      T (P::*const g)() const;
      bool (P::*const a)() const;
  public:
    constexpr SubSensorByCallback
      (
      const P *parent,
      Sensor_tag_t (P::*const tagFn)() const,
      T (P::*const getFn)() const,
      bool (P::*const isAvailableFnOpt)() const = NULL
      )
      : p(parent), t(tagFn), g(getFn), a(isAvailableFnOpt) { }
    virtual T get() const override { return((p->*g)()); }
    virtual Sensor_tag_t tag() const override { return((p->*t)()); }
    virtual bool isAvailable() const override { if(NULL == a) { return(true); } return((p->*a)()); }
  };


}

#endif
