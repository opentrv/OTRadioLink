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

#include "OTV0P2BASE_Serial_IO.h"

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
    // If new light level lower than previous
    // then do not detect any level of occupancy and save some CPU time.
    if(newLightLevel < prevLightLevel) { prevLightLevel = newLightLevel; return(OCC_NONE); }

#if 0 && !defined(ARDUINO)
    serialPrintlnAndFlush("update(>=)");
#endif

    // Default to no occupancy detected.
    occType occLevel = OCC_NONE;

    // For probable occupancy, and rise must be
    // a decent fraction of min to mean (or min to max) distance.
    // For weak occupancy being within a small distance of mean is a big clue.

    // Assume minimum of the noise floor if none set.
    const uint8_t minToUse = (0xff == longTermMinimumOrFF) ? epsilon : longTermMinimumOrFF;
    // Assume maximum of max dropped by the noise floor if none set.
    const uint8_t maxToUse = (0xff == longTermMaximumOrFF) ? (0xff-epsilon) : longTermMaximumOrFF;

    const uint8_t rise = newLightLevel - prevLightLevel;

    // Precondition for probable occupancy is a rising light level.
    // Any rise must be more than the fixed floor/noise threshold epsilon.
    // Also, IF a long-term mean for this time slot is available and above the lower floor,
    // then the rise must also be more than a fraction of the mean's distance above that floor.
    if((rise >= epsilon) &&
        (((0xff == meanNowOrFF) || (meanNowOrFF <= minToUse)) ||
            (rise >= ((meanNowOrFF - minToUse) >> (sensitive ? 2 : 1)))))
        {
        // Lights flicked on or curtains drawn maybe: room occupied.
        occLevel = OCC_PROBABLE;
        }
    // Else look for weak occupancy indications.
    // Look for habitual use of artificial lighting at set times, eg for TV watching or reading.
    // This must have a long-term non-extreme sane mean for the current time of day available,
    // and sane correctly-ordered min and max bounds.
    // and any rise must be small eg to guard against (eg) sunlight-driven flicker.
    //
    // See evening levels for trace 3l here for example:
    //     http://www.earth.org.uk/img/20161124-16WWal.png
    else if((rise < epsilon) && (meanNowOrFF > minToUse) && (meanNowOrFF < maxToUse)) // Implicitly 0xff != meanNowOrFF.
        {
        // Previous and current light levels should ideally be well away from maximum/minimum
        // (and asymmetrically much further below maximum, ie a wider margin on the high side)
        // to avoid being triggered in continuously dark/lit areas, and when daylit.
        // The levels must also be close to the mean for the time of day.
        const uint8_t range = maxToUse - minToUse;
        constexpr uint8_t marginWshift = 1;
        const uint8_t marginW = range >> (/*sensitive ? (1+marginWshift) :*/ marginWshift);
        const uint8_t margin = uint8_t(marginW >> 2);
        const uint8_t thrL = minToUse + margin;
        const uint8_t thrH = maxToUse - marginW;
        const uint8_t maxDistanceFromMean = fnmin(meanNowOrFF-minToUse, maxToUse-meanNowOrFF) >> (sensitive ? 2 : 3);

#if 0 && !defined(ARDUINO)
        serialPrintAndFlush("  newLightLevel=");
        serialPrintAndFlush(newLightLevel);
        serialPrintlnAndFlush();
        serialPrintAndFlush("  prevLightLevel=");
        serialPrintAndFlush(prevLightLevel);
        serialPrintlnAndFlush();
        serialPrintAndFlush("  meanNowOrFF=");
        serialPrintAndFlush(meanNowOrFF);
        serialPrintlnAndFlush();
        serialPrintAndFlush("  range=");
        serialPrintAndFlush(range);
        serialPrintlnAndFlush();
        serialPrintAndFlush("  margin=");
        serialPrintAndFlush(margin);
        serialPrintlnAndFlush();
        serialPrintAndFlush("  thrL=");
        serialPrintAndFlush(thrL);
        serialPrintlnAndFlush();
        serialPrintAndFlush("  thrH=");
        serialPrintAndFlush(thrH);
        serialPrintlnAndFlush();
        serialPrintAndFlush("  maxDistanceFromMean=");
        serialPrintAndFlush(maxDistanceFromMean);
        serialPrintlnAndFlush();
#endif

        if((newLightLevel > thrL) && (newLightLevel < thrH) &&
           (fnabsdiff(newLightLevel, meanNowOrFF) <= maxDistanceFromMean))
            {
            // Steady artificial lighting now near usual levels for this time of day.
            occLevel = OCC_WEAK;
            }
        }

	prevLightLevel = newLightLevel;
    return(occLevel);
	}
}
