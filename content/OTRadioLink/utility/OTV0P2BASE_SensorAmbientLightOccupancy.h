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

Author(s) / Copyright (s): Damon Hart-Davis 2016--2018
                           Deniz Erbilgin 2019
*/

/*
 Plug-in for ambient light sensor to provide occupancy detection.

 Provides an interface and a reference implementation.
 */

#ifndef OTV0P2BASE_SENSORAMBLIGHTOCCUPANCYDETECTION_H
#define OTV0P2BASE_SENSORAMBLIGHTOCCUPANCYDETECTION_H

#include "OTV0P2BASE_Util.h"
#include "OTV0P2BASE_SensorAmbientLightOccupancy_Tuneable.h"

namespace OTV0P2BASE
{


// Helper class to detect occupancy from ambient light levels.
//
// See research in this field such as:
//     "An intelligent agent for determining home occupancy using power monitors and light sensors"
//     https://www.researchgate.net/publication/225132826_An_Intelligent_Agent_for_Determining_Home_Occupancy_Using_Power_Monitors_and_Light_Sensors
//     Determining occupancy by ambient light sensors yielded an f-score
//     of 0.943 (precision score of 0.926, recall score of 0.962).
//     This did work very well. A more detailed examination found
//     there were 19 out of 219 days where the day’s precision or
//     recall scores were below 80% due to unusual light levels and sensors
//     not being optimally placed ...
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
// A light level of 254 (or over) indicates bright/full illumination.
//
// Light levels should be monotonic with lux.
//
// The more linear relationship between lux and the light level
// in the typical region of operation nominally the better,
// but in practice covering the range if illuminations in a typical
// home probably requires a fair amount of non-linearity,
// so a pure log response may also work well.
#define SensorAmbientLightOccupancyDetectorInterface_DEFINED
class SensorAmbientLightOccupancyDetectorInterface
  {
  public:
    // Occupancy detected from 0 (none) nominally rising to OCC_STRONG.
    // The OCC_STRONG level is (currently) beyond this detector's ability.
    enum occType : uint8_t
      {
        OCC_NONE = 0, // No occupancy detected.
        OCC_WEAK, // From constant habitual artificial lighting.
        OCC_PROBABLE, // From light flicked on.
        OCC_STRONG // Very strong confidence; NOT RETURNED BY THIS METHOD YET.
      };

    // Call regularly with the current ambient light level [0,254].
    // Should be called maybe once a minute
    // (or on whatever regular basis ambient light level is sampled).
    // Returns OCC_NONE if no occupancy is detected.
    // Returns OCC_WEAK if weak occupancy is detected,
    //     eg from TV watching.
    // Returns OCC_PROBABLE if probable occupancy is detected,
    //     eg from lights flicked on.
    // Does not block.
    //   * newLightLevel in range [0,254]
    // Not thread-/ISR- safe.
    virtual occType update(uint8_t newLightLevel) = 0;

    // Set mean, min and max ambient light levels from recent stats.
    // To allow auto adjustment to room; ~0/0xff means not known.
    // Mean value is for the current time of day.
    // Short term stats are typically over the last day,
    // longer term typically over the last week or so
    // (eg rolling exponential decays).
    // Call typically hourly with updated stats,
    // to set other internal time-dependent adaptation.
    //   * meanNowOrFF  typical/mean light level around this time
    //         each 24h; 0xff if not known.
    //   * sensitive  if true be more sensitive to possible
    //         occupancy changes,
    //         may mean more false positives and less energy saving
    // Not thread-/ISR- safe.
    virtual void setTypMinMax(uint8_t /*meanNowOrFF*/,
                              uint8_t /*longTermMinimumOrFF = 0xff*/,
                              uint8_t /*longTermMaximumOrFF = 0xff*/,
                              bool /*sensitive*/ = false) = 0;
  };


// Simple reference implementation.
#define SensorAmbientLightOccupancyDetectorSimple_DEFINED
class SensorAmbientLightOccupancyDetectorSimple final : public SensorAmbientLightOccupancyDetectorInterface
  {
  public:
      // Minimum delta (rise) for probable occupancy to be detected.
      // A simple noise floor.
      // This value cannot be greater than 127.
      static constexpr uint8_t epsilon = SENSORAMBIENTLIGHTOCCUPANCY_EPSILON;
      static_assert(epsilon <= 127, "epsilon must be less than or equal to 127.");

      // Min steady/grace time after lights on to confirm occupancy.
      // Intended to prevent a brief flash of light,
      // or quickly turning on lights in the night to find something,
      // from firing up the entire heating system.
      // This threshold may be applied conditionally,
      // eg when previously v dark.
      // Not so long as to fail to respond to genuine occupancy.
      //
      // This threshold may be useful elsewhere
      // to suppress over-hasty response
      // to a very brief lights-on, eg in the middle of the night.
      static constexpr uint8_t steadyTicksMinWithLightOn = SENSORAMBIENTLIGHTOCCUPANCY_steadyTicksMinWithLightOn;

  private:
      // Previous ambient light level [0,254]; 0 means dark.
      // Starts at max so that no initial light level
      // can imply occupancy.
      static constexpr uint8_t startingLL = 254;
      uint8_t prevLightLevel = startingLL;

      // Number of ticks (minutes) levels have been steady for.
      // Steady means a less-than-epsilon change per tick.
      uint8_t steadyTicks = 0;

      // Parameters from setTypMinMax().
      uint8_t meanNowOrFF = 0xff;
	  uint8_t longTermMinimumOrFF = 0xff;
	  uint8_t longTermMaximumOrFF = 0xff;

//	  // If true, the ambient light system has been requested to be
//	  // more sensitive to signs of occupancy.
//	  bool sensitive = false;

      // If true then a OCC_PROBABLE is pending
      // as long as light levels stay up/steady long enough.
      bool probablePending = false;

  public:
      constexpr SensorAmbientLightOccupancyDetectorSimple() { }

      // Reset to starting state; primarily for unit tests.
      void reset() { setTypMinMax(0xff, 0xff, 0xff, false); prevLightLevel = startingLL; steadyTicks = 0; probablePending = false; }

      // Call regularly (~1min) with current ambient light level [0,254].
      // Returns value > 0 if occupancy is detected.
      // Does not block.
      //   * newLightLevel in range [0,254]
      // Not thread-/ISR- safe.
      virtual occType update(uint8_t newLightLevel) override;

      // Set mean, min and max ambient light levels from recent stats.
      // To allow auto adjustment to room; ~0/0xff means not known.
      // Mean value is for the current time of day.
      // Short term stats are typically over the last day,
      // longer term typically over the last week or so
      // (eg rolling exponential decays).
      // Call regularly, roughly hourly, to drive
      // other internal time-dependent adaptation.
      //   * meanNowOrFF  typical/mean light level around this time
      //     each 24h; 0xff if not known.
      //   * sensitive  if true then be more sensitive to possible
      //     occupancy changes, eg to improve comfort.
      // Not thread-/ISR- safe.
      virtual void setTypMinMax(uint8_t meanNowOrFF,
                                uint8_t longTermMinimumOrFF = 0xff,
                                uint8_t longTermMaximumOrFF = 0xff,
                                bool /*sensitive*/ = false) override
          {
          this->meanNowOrFF = meanNowOrFF;
          this->longTermMinimumOrFF = longTermMinimumOrFF;
          this->longTermMaximumOrFF = longTermMaximumOrFF;
//          this->sensitive = sensitive;
          }

       // NOT OFFICAL API: expose steadyTicks for unit tests.
       uint8_t _getSteadyTicks() const { return(steadyTicks); }
  };


}
#endif
