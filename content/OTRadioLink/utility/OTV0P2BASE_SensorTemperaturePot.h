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

 V0p2/AVR specific for now.
 */

#ifndef OTV0P2BASE_SENSORTEMPERATUREPOT_H
#define OTV0P2BASE_SENSORTEMPERATUREPOT_H

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "OTV0P2BASE_Util.h"
#include "OTV0P2BASE_Sensor.h"


namespace OTV0P2BASE
{


#ifdef ARDUINO_ARCH_AVR
// Sensor for temperature potentiometer/dial; 0 is coldest, 255 is hottest.
// Note that if the callbacks are enabled, the following are implemented:
//   * Any operation of the pot calls the occupancy/"UI used" callback.
//   * Force FROST mode when dial turned right down to bottom.
//   * Start BAKE mode when dial turned right up to top.
//   * Cancel BAKE mode when dial/temperature turned down.
//   * Force WARM mode when dial/temperature turned up.
#define SensorTemperaturePot_DEFINED
class SensorTemperaturePot : public OTV0P2BASE::SimpleTSUint8Sensor
  {
  public:
    // Maximum 'raw' temperature pot/dial value.
    static const uint16_t TEMP_POT_RAW_MAX = 1023;

    // Minimum change (hysteresis) enforced in normalised/8-bit 'reduced noise' range value; strictly positive.
    // Aim to provide reasonable noise immunity, even from an ageing carbon-track pot.
    // Allow reasonable remaining granularity of response, at least 10s of distinct positions (>=5 bits).
    // This is in terms of steps on the non-raw [0,255] nominal output scale.
    // Note that some applications may only see a fraction of full scale movement (eg ~25% for DORM1),
    // so allowing for reasonable end stops and tolerances that further constrains this value from above.
    // DHD20160212: observed manual precision with base REV10 pot ~8--16 raw, so RN_HYST >= 2 is reasonable.
    static const uint8_t RN_HYST = 1;

    // Bottom and top parts of normalised/8-bit reduced noise range reserved for end-stops (forcing FROST or BAKE).
    // Should be big enough to hit easily (and should be larger than RN_HYST)
    // but not so big as to really constrain the temperature range or cause confusion.
    // This is in terms of steps on the non-raw [0,255] nominal output scale.
    // Note that some applications may only see a fraction of full scale movement (eg ~25% for DORM1),
    // so allowing for reasonable end stops and tolerances that further constrains this value from above.
    // Note that absolute skew of pot in different devices units may be much larger than unit self-precision.
    static const uint8_t RN_FRBO = fnmax(2*RN_HYST, 8);

  private:
    // Raw pot value [0,1023] if extra precision is required.
    uint16_t raw;

    // Occupancy callback function (for good confidence of human presence); NULL if not used.
    // Also indicates that the manual UI has been used.
    // If not NULL, is called when this sensor detects indications of occupancy.
    void (*occCallback)();

    // WARM/FROST and BAKE start/cancel callbacks.
    // If not NULL, are called when the pot is adjusted appropriately.
    // Typically at most one of these callbacks would be made on any appropriate pot adjustment.
    void (*warmModeCallback)(bool);
    void (*bakeStartCallback)(bool);

    // Compute the real scaled minimum, allowing for reversals.
    inline uint8_t _computeRealMinScaled(uint16_t minExpected_, uint16_t maxExpected_)
      { return(fnmin(minExpected_ >> 2, maxExpected_ >> 2)); }
    // Compute the real scaled maximum, allowing for reversals.
    inline uint8_t _computeRealMaxScaled(uint16_t minExpected_, uint16_t maxExpected_)
      { return(fnmax(minExpected_ >> 2, maxExpected_ >> 2)); }
    // Compute the loEndStop.
    inline uint8_t _computeLoEndStop(uint16_t minExpected_, uint16_t maxExpected_)
      {
      const uint8_t realMinScaled = _computeRealMinScaled(minExpected_, maxExpected_);
      return((realMinScaled >= 255 - RN_FRBO) ? realMinScaled : (realMinScaled + RN_FRBO));
      }
    // Compute the hiEndStop.
    inline uint8_t _computeHiEndStop(uint16_t minExpected_, uint16_t maxExpected_)
      {
      const uint8_t realMaxScaled = _computeRealMaxScaled(minExpected_, maxExpected_);
      return((realMaxScaled < RN_FRBO) ? realMaxScaled : (realMaxScaled - RN_FRBO));
      }

  public:
    // Initialise raw to distinct/special value and all pointers to NULL.
    SensorTemperaturePot(const uint16_t minExpected_ = 0, const uint16_t maxExpected_ = TEMP_POT_RAW_MAX)
      : raw((uint16_t) ~0U),
        occCallback(NULL), warmModeCallback(NULL), bakeStartCallback(NULL),
        minExpected(minExpected_), maxExpected(maxExpected_),
        loEndStop(_computeLoEndStop(minExpected_, maxExpected_)), hiEndStop(_computeHiEndStop(minExpected_, maxExpected_))
      { }

    // Lower and upper bounds of expected pot movement/output each in range [0,TEMP_POT_RAW_MAX].
    // The values must be different and further apart at least than the noise threshold (~8).
    // Max is lower than min if the pot value is to be reversed.
    // Conservative values (ie with actual travel outside the specified range) should be specified
    // if end-stop detection is to work (callbacks on hitting the extremes).
    // The output is not rebased based on these values, though is reversed if necessary;
    // whatever uses the pot output should map to the desired values.
    // These read-only values are exposed to assist with any such mapping.
    const uint16_t minExpected, maxExpected;
    // Returns true if the pot output is to be reversed from the natural direction.
    inline bool isReversed() const { return(minExpected > maxExpected); }

    // A (scaled) value below this is deemed to be at the low end stop region (allowing for reversed movement).
    const uint8_t loEndStop;
    // Returns true if at the low end stop: ISR safe.
    inline bool isAtLoEndStop() const { return(value < loEndStop); }
    // A (scaled) value above this is deemed to be at the high end stop region (allowing for reversed movement).
    const uint8_t hiEndStop;
    // Returns true if at the high end stop: ISR safe.
    inline bool isAtHiEndStop() const { return(value > hiEndStop); }

    // Force a read/poll of the temperature pot and return the value sensed [0,255] (cold to hot).
    // Potentially expensive/slow.
    // This value has some hysteresis applied to reduce noise.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    virtual uint8_t read();

    // Set occupancy callback function (for good confidence of human presence); NULL for no callback.
    // Also indicates that the manual UI has been used.
    void setOccCallback(void (*occCallback_)()) { occCallback = occCallback_; }

    // Set WARM/FROST and BAKE start/cancel callbacks.
    // If not NULL, are called when the pot is adjusted appropriately.
    // Typically at most one of these callbacks would be made on any approriate pot adjustment.
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
