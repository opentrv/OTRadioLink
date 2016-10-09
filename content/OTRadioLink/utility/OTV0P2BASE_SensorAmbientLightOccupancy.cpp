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

#include "OTV0P2BASE_SensorAmbientLightOccupancy.h"


namespace OTV0P2BASE
{


// Call regularly (~1/60s) with the current ambient light level [0,254].
// Returns true if probable occupancy is detected.
// Does not block.
// Not thread-/ISR- safe.
//   * newLightLevel in range [0,254]
bool SensorAmbientLightOccupancyDetectorSimple::update(const uint8_t newLightLevel)
    {
    const bool result = (newLightLevel > prevLightLevel) && // Needs further constraints.
        (((int)newLightLevel) > (epsilon + (int)prevLightLevel));
	prevLightLevel = newLightLevel;
    return(result);
	}

// Set mean, min and max ambient light levels from recent stats, to allow auto adjustment to room; ~0/0xff means not known.
// Mean value is for the current time of day.
// Short term stats are typically over the last day,
// longer term typically over the last week or so (eg rolling exponential decays).
// Call regularly, roughly hourly, to drive other internal time-dependent adaptation.
//   * meanNowOrFF  typical/mean light level around this time each 24h; 0xff if not known.
//   * sensitive  if true then be more sensitive to possible occupancy changes, eg to improve comfort.
void SensorAmbientLightOccupancyDetectorSimple::setTypMinMax(const uint8_t meanNowOrFF,
                  const uint8_t longTermMinimumOrFF, const uint8_t longTermMaximumOrFF,
                  const bool sensitive)
    {
    this->meanNowOrFF = meanNowOrFF;
    this->longTermMinimumOrFF = longTermMinimumOrFF;
    this->longTermMaximumOrFF = longTermMaximumOrFF;
    this->sensitive = sensitive;
    }


}
