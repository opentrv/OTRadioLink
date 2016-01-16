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

#include "OTV0P2BASE_Util.h"
#include "OTV0P2BASE_Sensor.h"


namespace OTV0P2BASE
{


// Sensor for temperature potentiometer/dial; 0 is coldest, 255 is hottest.
// Note that if the callbacks are enabled, the following are implemented:
//   * Any operation of the pot calls the occupancy/"UI used" callback.
//   * Force FROST mode when dial turned right down to bottom.
//   * Start BAKE mode when dial turned right up to top.
//   * Cancel BAKE mode when dial/temperature turned down.
//   * Force WARM mode when dial/temperature turned up.
class SensorTemperaturePot : public OTV0P2BASE::SimpleTSUint8Sensor
  {
  public:
    // Maximum 'raw' temperature pot/dial value.
    static const uint16_t TEMP_POT_RAW_MAX = 1023;

  private:
    // Raw pot value [0,1023] if extra precision is required.
    uint16_t raw;

    // Occupancy callback function (for good confidence of human presence); NULL if not used.
    // Also indicates that the manual UI has been used.
    // If not NULL, is called when this sensor detects indications of occupancy.
    void (*occCallback)();

    // WARM/FROST and BAKE start/cancel callbacks.
    // If not NULL, are called when the pot is adjusted appropriately.
    // Typically at most one of these callbacks would be made on any approriate pot adjustment.
    void (*warmModeCallback)(bool);
    void (*bakeStartCallback)(bool);

  public:
    // Initialise raw to distinct/special value and all pointers to NULL.
    SensorTemperaturePot(uint16_t minExpected_ = 0, uint16_t maxExpected_ = TEMP_POT_RAW_MAX)
      : raw(~0U),
        occCallback(NULL), warmModeCallback(NULL), bakeStartCallback(NULL),
        minExpected(minExpected_), maxExpected(maxExpected_)
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
    inline bool isReversed() { return(minExpected > maxExpected); }

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


}
#endif
