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
// to avoid spurious occupancy detection at power-up/restart.
//
// The class retains state in order to detect occupancy.
//
// A light level of 0 indicates dark.
//
// A light level of 254 (or over) indicates bright illumination.
//
// Light levels should be monotonic with lux.
//
// The more linear relationship between lux and the light level
// in the typical region of operation the better.
#define SensorAmbientLightOccupancyDetectorInterface_DEFINED
class SensorAmbientLightOccupancyDetectorInterface
  {
  public:
    // Call regularly with the current ambient light level [0,254].
    // Returns true if probable occupancy is detected.
    // Does not block.
    // Not thread-/ISR- safe.
    // Call regularly (~1/60s) with the current ambient light level [0,254].
    // Returns true if probable occupancy is detected.
    // Does not block.
    //   * newLightLevel in range [0,254]
    // Not thread-/ISR- safe.
    virtual bool update(uint8_t newLightLevel) = 0;

    // Set mean, min and max ambient light levels from recent stats, to allow auto adjustment to room; ~0/0xff means not known.
    // Mean value is for the current time of day.
    // Short term stats are typically over the last day,
    // longer term typically over the last week or so (eg rolling exponential decays).
    // Call regularly, roughly hourly, to drive other internal time-dependent adaptation.
    //   * meanNowOrFF  typical/mean light level around this time each 24h; 0xff if not known.
    //   * sensitive  if true then be more sensitive to possible occupancy changes, eg to improve comfort.
    // By default does nothing.
    // Not thread-/ISR- safe.
    virtual void setTypMinMax(uint8_t /*meanNowOrFF*/,
                      uint8_t /*longTermMinimumOrFF = 0xff*/, uint8_t /*longTermMaximumOrFF = 0xff*/,
                      bool /*sensitive = false*/) { }

    // True if the detector is in 'sensitive' mode.
    // Defaults to false.
    virtual bool isSensitive() { return(false); }
  };


// Simple reference implementation.
#define SensorAmbientLightOccupancyDetectorSimple_DEFINED
class SensorAmbientLightOccupancyDetectorSimple final : public SensorAmbientLightOccupancyDetectorInterface
  {
  public:
      // Minimum delta (rise) for occupancy to be detected; a simple noise floor.
      static const uint8_t epsilon = 4;

  private:
      // Previous ambient light level [0,254]; 0 means dark.
      // Starts at max so that no initial light level can imply occupancy.
      uint8_t prevLightLevel;

      // Parameters from setTypMinMax().
      uint8_t meanNowOrFF;
	  uint8_t longTermMinimumOrFF;
	  uint8_t longTermMaximumOrFF;
	  bool sensitive;

  public:
      SensorAmbientLightOccupancyDetectorSimple()
        : prevLightLevel(254),
		  meanNowOrFF(0xff), longTermMinimumOrFF(0xff), longTermMaximumOrFF(0xff), sensitive(false)
          { }

      // Call regularly (~1/60s) with the current ambient light level [0,254].
      // Returns true if probable occupancy is detected.
      // Does not block.
      //   * newLightLevel in range [0,254]
      // Not thread-/ISR- safe.
      virtual bool update(uint8_t newLightLevel);

      // Set mean, min and max ambient light levels from recent stats, to allow auto adjustment to room; ~0/0xff means not known.
      // Mean value is for the current time of day.
      // Short term stats are typically over the last day,
      // longer term typically over the last week or so (eg rolling exponential decays).
      // Call regularly, roughly hourly, to drive other internal time-dependent adaptation.
      //   * meanNowOrFF  typical/mean light level around this time each 24h; 0xff if not known.
      //   * sensitive  if true then be more sensitive to possible occupancy changes, eg to improve comfort.
      // Not thread-/ISR- safe.
      virtual void setTypMinMax(uint8_t meanNowOrFF,
                        uint8_t longTermMinimumOrFF = 0xff, uint8_t longTermMaximumOrFF = 0xff,
                        bool sensitive = false)
          {
          this->meanNowOrFF = meanNowOrFF;
          this->longTermMinimumOrFF = longTermMinimumOrFF;
          this->longTermMaximumOrFF = longTermMaximumOrFF;
          this->sensitive = sensitive;
          }

      // True if the detector is in 'sensitive' mode.
      virtual bool isSensitive() { return(sensitive); }
  };


}
#endif
