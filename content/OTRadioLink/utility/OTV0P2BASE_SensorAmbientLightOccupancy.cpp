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
// Returns > 0 if occupancy is detected.
// Does not block.
//   * newLightLevel in range [0,254]
// Not thread-/ISR- safe.
SensorAmbientLightOccupancyDetectorInterface::occType SensorAmbientLightOccupancyDetectorSimple::update(const uint8_t newLightLevel)
    {
    bool probableOccupancy = false;
    // Only predict occupancy if no reason can be found NOT to.
    do  {
        // Minimum/first condition for occupancy is a rising light level.
        if(newLightLevel <= prevLightLevel) { break; }
        const uint8_t rise = newLightLevel - prevLightLevel;
        // Any rise must be more than the fixed floor/noise threshold epsilon.
        if(rise < epsilon) { break; }

        // Any rise must be a decent fraction of min to mean (or min to max) distance.
        // Amount to right-shift mean (-min) and max (-min) to generate thresholds.
        const uint8_t meanShift = sensitive ? 2 : 1;
        // Assume minimum of 0 if none set.
        const uint8_t minToUse = (0xff == longTermMinimumOrFF) ? 0 : longTermMinimumOrFF;
        // If a typical/mean value is available then screen current rise against it.
        if((0xff != meanNowOrFF) && (meanNowOrFF >= minToUse))
			{
            const uint8_t meanRiseThreshold = (meanNowOrFF - minToUse) >> meanShift;
            if(rise < meanRiseThreshold) { break; }
			}
//        else if((0xff != longTermMaximumOrFF) && (longTermMaximumOrFF >= minToUse))
//            {
//            const int maxShift = meanShift + 1;
//            const uint8_t maxRiseThreshold = (longTermMaximumOrFF - minToUse) >> maxShift;
//            if(rise < maxRiseThreshold) { break; }
//            }

        probableOccupancy = true;
        } while(false);
	prevLightLevel = newLightLevel;
    return(probableOccupancy ? OCC_PROBABLE : OCC_NONE);
	}

}
