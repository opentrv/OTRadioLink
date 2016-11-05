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
 Temperature potentiometer (pot) dial sensor with UI / occupancy outputs.
 */

#ifndef OTV0P2BASE_SENSORTEMPERATUREPOT_H
#define OTV0P2BASE_SENSORTEMPERATUREPOT_H

//#ifdef ARDUINO
//#include <Arduino.h>
//#endif

#include "OTV0P2BASE_ADC.h"
#include "OTV0P2BASE_Entropy.h"
#include "OTV0P2BASE_Sensor.h"
#include "OTV0P2BASE_PowerManagement.h"
#include "OTV0P2BASE_SensorOccupancy.h"


namespace OTV0P2BASE
{


// Base class for sensor for temperature potentiometer/dial; 0 is coldest, 255 is hottest.
// Note that if the callbacks are enabled, the following are implemented:
//   * Any operation of the pot calls the occupancy/"UI used" callback.
//   * Force FROST mode when dial turned right down to bottom.
//   * Start BAKE mode when dial turned right up to top.
//   * Cancel BAKE mode when dial/temperature turned down.
//   * Force WARM mode when dial/temperature turned up.
#define SensorTemperaturePotBase_DEFINED
class SensorTemperaturePotBase : public OTV0P2BASE::SimpleTSUint8Sensor
  {
  public:
    // Minimum change (hysteresis) enforced in normalised/8-bit 'reduced noise' range value; strictly positive.
    // Aim to provide reasonable noise immunity, even from an ageing carbon-track pot.
    // Allow reasonable remaining granularity of response, at least 10s of distinct positions (>=5 bits).
    // This is in terms of steps on the non-raw [0,255] nominal output scale.
    // Note that some applications may only see a fraction of full scale movement (eg ~25% for DORM1),
    // so allowing for reasonable end stops and tolerances that further constrains this value from above.
    // DHD20160212: observed manual precision with base REV10 pot ~8--16 raw, so RN_HYST >= 2 is reasonable.
    static constexpr uint8_t RN_HYST = 1;

    // Bottom and top parts of normalised/8-bit reduced noise range reserved for end-stops (forcing FROST or BAKE).
    // Should be big enough to hit easily (and should be larger than RN_HYST)
    // but not so big as to really constrain the temperature range or cause confusion.
    // This is in terms of steps on the non-raw [0,255] nominal output scale.
    // Note that some applications may only see a fraction of full scale movement (eg ~25% for DORM1),
    // so allowing for reasonable end stops and tolerances that further constrains this value from above.
    // Note that absolute skew of pot in different devices units may be much larger than unit self-precision.
    static constexpr uint8_t RN_FRBO = fnmax(2*RN_HYST, 8);

    // A (scaled) value below this is deemed to be at the low end stop region (allowing for reversed movement).
    const uint8_t loEndStop;
    // Returns true if at the low end stop: ISR safe.
    inline bool isAtLoEndStop() const { return(value < loEndStop); }
    // A (scaled) value above this is deemed to be at the high end stop region (allowing for reversed movement).
    const uint8_t hiEndStop;
    // Returns true if at the high end stop: ISR safe.
    inline bool isAtHiEndStop() const { return(value > hiEndStop); }

    // Construct an instance.
    SensorTemperaturePotBase(const uint8_t _loEndStop, const uint8_t _hiEndStop)
      : loEndStop(_loEndStop), hiEndStop(_hiEndStop)
      { }
  };


#ifdef ARDUINO_ARCH_AVR
// Sensor for temperature potentiometer/dial; 0 is coldest, 255 is hottest.
// Note that if the callbacks are enabled, the following are implemented:
//   * Any operation of the pot calls the occupancy/"UI used" callback.
//   * Force FROST mode when dial turned right down to bottom.
//   * Start BAKE mode when dial turned right up to top.
//   * Cancel BAKE mode when dial/temperature turned down.
//   * Force WARM mode when dial/temperature turned up.
//
// Parameters:
//   * occupancy  if non-NULL, occupancy tracker to report manual interaction to
//   * neededPeriphEnable  if true (the default), bring up IO_POWER_UP line for talking the sample.
//   * minExpected, maxExpected
//        Lower and upper bounds of expected pot movement/output each in range [0,TEMP_POT_RAW_MAX].
//        The values must be different and further apart at least than the noise threshold (~8).
//        Max is lower than min if the pot value is to be reversed.
//        Conservative values (ie with actual travel outside the specified range) should be specified
//        if end-stop detection is to work (callbacks on hitting the extremes).
//        The output is not rebased based on these values, though is reversed if necessary;
//        whatever uses the pot output should map to the desired values.
//
//        DORM1 / REV7 initial unit range ~[45,293] DHD20160211 (seen <45 to >325).
//        Thus could be ~30 points per item on scale: * 16 17 18 >19< 20 21 22 BOOST
//        Actual precision/reproducability of pot is circa +/- 4.
        //static const uint16_t REV7_pot_low = 48;
        //static const uint16_t REV7_pot_high = 296;
#define SensorTemperaturePot_DEFINED
template
    <
    class occ_t /*= OTV0P2BASE::PseudoSensorOccupancyTracker*/, occ_t *const occupancy = NULL,
    const uint16_t minExpected = 0, const uint16_t maxExpected = 1023 /* TEMP_POT_RAW_MAX */,
    bool needsPeriphEnable = true
    >
class SensorTemperaturePot final : public SensorTemperaturePotBase
  {
  public:
    // Maximum 'raw' temperature pot/dial value.
    static const uint16_t TEMP_POT_RAW_MAX = 1023;

  private:
    // Raw pot value [0,1023] if extra precision is required.
    uint16_t raw;

    // WARM/FROST and BAKE start/cancel callbacks.
    // If not NULL, are called when the pot is adjusted appropriately.
    // Typically at most one of these callbacks would be made on any appropriate pot adjustment.
    void (*warmModeCallback)(bool);
    void (*bakeStartCallback)(bool);

    // Compute the real scaled minimum, allowing for reversals.
    inline constexpr uint8_t _computeRealMinScaled(uint16_t minExpected_, uint16_t maxExpected_)
      { return(fnmin(minExpected_ >> 2, maxExpected_ >> 2)); }
    // Compute the real scaled maximum, allowing for reversals.
    inline constexpr uint8_t _computeRealMaxScaled(uint16_t minExpected_, uint16_t maxExpected_)
      { return(fnmax(minExpected_ >> 2, maxExpected_ >> 2)); }
    // Compute the loEndStop.
    inline constexpr uint8_t _computeLoEndStop(uint16_t minExpected_, uint16_t maxExpected_)
      {
      const uint8_t realMinScaled = _computeRealMinScaled(minExpected_, maxExpected_);
      return((realMinScaled >= 255 - SensorTemperaturePotBase::RN_FRBO) ? realMinScaled : (realMinScaled + SensorTemperaturePotBase::RN_FRBO));
      }
    // Compute the hiEndStop.
    inline constexpr uint8_t _computeHiEndStop(uint16_t minExpected_, uint16_t maxExpected_)
      {
      const uint8_t realMaxScaled = _computeRealMaxScaled(minExpected_, maxExpected_);
      return((realMaxScaled < SensorTemperaturePotBase::RN_FRBO) ? realMaxScaled : (realMaxScaled - SensorTemperaturePotBase::RN_FRBO));
      }

    // Returns true if the pot output is to be reversed from the natural direction.
    inline bool isReversed() const { return(minExpected > maxExpected); }

  public:
    // Initialise raw to distinct/special value and all pointers to NULL.
    SensorTemperaturePot(/*const uint16_t minExpected_ = 0, const uint16_t maxExpected_ = TEMP_POT_RAW_MAX*/)
      : raw((uint16_t) ~0U),
        warmModeCallback(NULL), bakeStartCallback(NULL),
        SensorTemperaturePotBase(_computeLoEndStop(minExpected, maxExpected), _computeHiEndStop(minExpected, maxExpected))
      { }

    // Force a read/poll of the temperature pot and return the value sensed [0,255] (cold to hot).
    // Potentially expensive/slow.
    // This value has some hysteresis applied to reduce noise.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    // Force a read/poll of the temperature pot and return the value sensed [0,255] (cold to hot).
    // Potentially expensive/slow.
    // This value has some hysteresis applied to reduce noise.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    virtual uint8_t read() override
      {
      // Capture the old raw value early.
      const uint16_t oldRaw = raw;

      // No need to wait for voltage to stabilise as pot top end directly driven by IO_POWER_UP.
      if(needsPeriphEnable) { OTV0P2BASE::power_intermittent_peripherals_enable(false); }
      const uint16_t tpRaw = OTV0P2BASE::analogueNoiseReducedRead(V0p2_PIN_TEMP_POT_AIN, DEFAULT); // Vcc reference.
      if(needsPeriphEnable) { OTV0P2BASE::power_intermittent_peripherals_disable(); }

      const bool reverse = isReversed();
      const uint16_t newRaw = reverse ? (TEMP_POT_RAW_MAX - tpRaw) : tpRaw;

      // Capture entropy from changed LS bits.
      if((uint8_t)newRaw != (uint8_t)oldRaw) { ::OTV0P2BASE::addEntropyToPool((uint8_t)newRaw, 0); } // Claim zero entropy as may be forced by Eve.

      // Capture reduced-noise value with a little hysteresis.
      // Only update the value if changed significantly so as to reduce noise.
      // Too much hysteresis may make the dial difficult to use,
      // especially if the rotation is physically constrained.
      const uint8_t oldValue = this->value;
      const uint8_t potentialNewValue = newRaw >> 2;
      if(((potentialNewValue > oldValue) && (newRaw - oldRaw >= (SensorTemperaturePotBase::RN_HYST<<2))) ||
         ((potentialNewValue < oldValue) && (oldRaw - newRaw >= (SensorTemperaturePotBase::RN_HYST<<2))))
        {
        // Use this potential new value as a reduced-noise new value.
        const uint8_t rn = (uint8_t) potentialNewValue;
        // Atomically store the reduced-noise normalised value.
        this->value = rn;

        // Smart responses to adjustment/movement of temperature pot.
        // Possible to get reasonable functionality without using MODE button.
        //
        // Ignore first reading which might otherwise cause spurious mode change, etc.
        if((uint16_t)~0U != (uint16_t)raw) // Ignore if raw not yet set for the first time.
          {
          // Force FROST mode when dial turned right down to bottom.
          if(rn < this->loEndStop) { if(NULL != warmModeCallback) { warmModeCallback(false); } }
          // Start BAKE mode when dial turned right up to top.
          else if(rn > this->hiEndStop) { if(NULL != bakeStartCallback) { bakeStartCallback(true); } }
          // Cancel BAKE mode when dial/temperature turned down.
          else if(rn < oldValue) { if(NULL != bakeStartCallback) { bakeStartCallback(false); } }
          // Force WARM mode when dial/temperature turned up.
          else if(rn > oldValue) { if(NULL != warmModeCallback) { warmModeCallback(true); } }

          // Report that the user operated the pot, ie part of the manual UI.
          // Do this regardless of whether a specific mode change was invoked.
          if(NULL != occupancy) { occupancy->markAsOccupied(); }
          }
        }

    #if 0 && defined(DEBUG)
      DEBUG_SERIAL_PRINT_FLASHSTRING("Temp pot: ");
      DEBUG_SERIAL_PRINT(tp);
      DEBUG_SERIAL_PRINT_FLASHSTRING(", rn: ");
      DEBUG_SERIAL_PRINT(tempPotReducedNoise);
      DEBUG_SERIAL_PRINTLN();
    #endif

      // Store new raw value last.
      raw = newRaw;
      // Return noise-reduced value.
      return(this->value);
      }

    // Set WARM/FROST and BAKE start/cancel callbacks.
    // If not NULL, are called when the pot is adjusted appropriately.
    // Typically at most one of these callbacks would be made on any appropriate pot adjustment.
    void setWFBCallbacks(void (*warmModeCallback_)(bool), void (*bakeStartCallback_)(bool))
      { warmModeCallback = warmModeCallback_; bakeStartCallback = bakeStartCallback_; }

    // Return last value fetched by read(); undefined before first read()).
    // Fast.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    uint16_t getRaw() const { return(raw); }
  };

#endif // ARDUINO_ARCH_AVR


}
#endif
