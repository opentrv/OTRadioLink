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
 Plug-in for ambient light sensor to provide occupancy detection.

 Provides an interface and a reference implementation.
 */

#ifndef OTV0P2BASE_SENSORAMBLIGHTOCCUPANCYDETECTION_H
#define OTV0P2BASE_SENSORAMBLIGHTOCCUPANCYDETECTION_H

#include "OTV0P2BASE_Util.h"


namespace OTV0P2BASE
{


// Helper class to detect occupancy from ambient light levels.
//
// The basic mode of operation is to call update() regularly
// (typically once per minute)
// with the current ambient light level.
//
// If occupancy is detected then update() returns true.
//
// Generally the initial call to update() should not return true,
// whatever the indicate current light level,
// to avoid spurious oocupancy detection at power-up/restart.
//
// The class retains state in order to detect occupancy.
//
// A light level of 0 indicates dark.
//
// A light level of 255 indicates bright illumination.
//
// Light levels should be monotonic with lux.
//
// The more linear relationship between lux and the light level
// in the typical region of operation the better.
#define SensorAmbientLightOccupancyDetectorInterface_DEFINED
class SensorAmbientLightOccupancyDetectorInterface
  {
  public:
    // Call regularly with the current ambient light level [0,255].
    // Returns true if probable occupancy is detected.
    // Does not block.
    // Not thread-/ISR- safe.
    virtual bool update(uint8_t newLightLevel) = 0;

    // Set mean, min and max ambient light levels from recent stats, to allow auto adjustment to room; ~0/0xff means not known.
    // Mean value is for the current time of day.
    // Short term stats are typically over the last day,
    // longer term typically over the last week or so (eg rolling exponential decays).
    // Call regularly, roughly hourly, to drive other internal time-dependent adaptation.
    //   * meanNowOrFF  typical/mean light level around this time; 0xff if not known.
    //   * sensitive  if true be more sensitive to possible occupancy changes, else less so.
    // By default does nothing.
    virtual void setTypMinMax(uint8_t meanNowOrFF,
                      uint8_t recentMinimumOrFF, uint8_t recentMaximumOrFF,
                      uint8_t longerTermMinimumOrFF = 0xff, uint8_t longerTermMaximumOrFF = 0xff,
                      bool sensitive = true)
      { }

  };


// Simple reference implementation.
#define SensorAmbientLightOccupancyDetectorSimple_DEFINED
class SensorAmbientLightOccupancyDetectorSimple : public SensorAmbientLightOccupancyDetectorInterface
  {
  public:
      // Minimum delta (rise) for occupancy to be detected; a simple noise floor.
      static const uint8_t epsilon = 8;

  private:
      // Previous ambient light level [0,255]; 0 means dark.
      // Starts at 255 so that no initial light level can imply occupancy.
      uint8_t prevLightLevel;

  public:
      SensorAmbientLightOccupancyDetectorSimple()
        : prevLightLevel(255)
          { }

      // Call regularly (~1/60s) with the current ambient light level [0,255].
      // Returns true if probable occupancy is detected.
      // Does not block.
      // Not thread-/ISR- safe.
      virtual bool update(uint8_t newLightLevel);
  };


}
#endif
