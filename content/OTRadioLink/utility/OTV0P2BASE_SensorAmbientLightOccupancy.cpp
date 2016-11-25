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
//
// Probable occupancy is detected by a rise in ambient light level in one tick/update:
//   * at least the hard-wired floor noise epsilon
//   * at least a fraction of the mean ambient light level expected in this interval
//
// Weak occupancy [will be!] detected by previous and current levels being:
//   * similar (ie not much change, and downward changes may be ignored to reduce processing and on principle)
//   * close-ish to expected mean for this interval
//   * significantly above long term minimum and below long term maximum (and not saturated/dark)
//     thus reflecting a deliberately-maintained light level other than max or dark,
//     and in particular not dark, saturated daylight nor completely constant lighting.
SensorAmbientLightOccupancyDetectorInterface::occType SensorAmbientLightOccupancyDetectorSimple::update(const uint8_t newLightLevel)
    {
    // Default to no occupancy detected.
    occType occLevel = OCC_NONE;

    // Only predict occupancy if no reason can be found NOT to.
    do  {
        // If new light level lower than previous
        // do not detect any level of occupancy and save some CPU time.
        if(newLightLevel < prevLightLevel) { break; }

        // Minimum/first condition for probable occupancy is a rising light level.
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

        occLevel = OCC_PROBABLE;
        } while(false);

	prevLightLevel = newLightLevel;
    return(occLevel);
	}

}
