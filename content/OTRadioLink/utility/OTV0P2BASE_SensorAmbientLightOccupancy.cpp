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
    // Minimum steady time for detecting artificial light (ticks/minutes).
    static constexpr uint8_t steadyTicksMinForArtificialLight = 30;
    // Minimum steady time for detecting light on (ticks/minutes).
    // Should be short enough to notice someone going to make a cuppa.
    // Note that an interval <= TX interval may make it harder to validate
    // algorithms from routinely collected data,
    // eg <= 4 minutes with typical secure frame rate of 1 per ~4 minutes.
    static constexpr uint8_t steadyTicksMinBeforeLightOn = 3;
    // Minimum steady time after lights on to confirm 'probable' occupancy.
    // Intended to prevent  a brief flash of light,
    // or very quickly turning on lights in the night to find something,
    // from firing up the entire heating system.
    // This threshold may be applied conditionally, eg when previously v dark.
    // Not so long as to fail to respond to genuine occupancy.
    static constexpr uint8_t steadyTicksMinWithLightOn = 3;

    // If new light level lower than previous
    // then do not detect any level of occupancy and save some CPU time.
    if(newLightLevel < prevLightLevel)
        {
        // If a significant fall then levels are not steady,
        // and also clear any pending 'probable' occupancy indication.
        if((prevLightLevel - newLightLevel) >= epsilon)
            { steadyTicks = 0; probablePending = false; }
        else if(steadyTicks < 255)
            { ++steadyTicks; }
        prevLightLevel = newLightLevel;
        return(OCC_NONE);
        }

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
    // Assume maximum of full-scale minus the noise floor if none set.
    const uint8_t maxToUse = (0xff == longTermMaximumOrFF) ? (0xff-epsilon) : longTermMaximumOrFF;

    // Compute delta/rise.
    const uint8_t rise = newLightLevel - prevLightLevel;
    const bool steady = (rise < epsilon);

    // Reset 'steady' timer if significant (upward) delta.
    // (Rise does not clear pending probable.)
    const uint8_t oldSteadyTicks = steadyTicks;
    if(!steady) { steadyTicks = 0; }
    else if(steadyTicks < 255) { ++steadyTicks; }

    // Activate pending probable occupancy if steady long enough
    // ie without (much) light level fall.
    if(probablePending)
        {
        // This could get postponed indefinitely if light levels
        // continue to rise strongly;
        // eg with a slow warm-up CFL, or sunrise.
        if(steadyTicks >= steadyTicksMinWithLightOn)
            {
            // Lights have been on and stayed on and steady.
            occLevel = OCC_PROBABLE;
            probablePending = false;
            }
        }
    // Precondition for probable occupancy is a rising light level.
    // Any rise must be more than the fixed floor/noise threshold epsilon.
    // Also, IF a long-term mean for this time slot is available
    // and that mean is above the lower floor,
    // then the rise must also be more than a fraction of the mean's distance
    // above that floor.
    //
    // An alternative damper would be the rise starting at/below the mean.
    else if((!steady) && (oldSteadyTicks >= steadyTicksMinBeforeLightOn))
        {
        // Is the mean value for this slot usable?
        const bool usableMean = (0xff != meanNowOrFF) && (meanNowOrFF > minToUse);
        // Compute the minimum rise to trigger probable occupancy.
        // With no usable mean use a sensible default minimum rise
        // to improve initial stability while unit is learning typical levels.
        const uint8_t minRise = usableMean
          ? ((meanNowOrFF - minToUse) >> (1 /* sensitive ? 2 : 1 */ ))
          : (SensorAmbientLightBase::DEFAULT_LIGHT_THRESHOLD/2);
        if(rise >= minRise)
            {
            if((prevLightLevel > minToUse) || (oldSteadyTicks < 0xff))
                {
                // Room was NOT very dark,
                // or has not been steady (eg dark) for a long time.
                // Lights flicked on or curtains drawn maybe: room occupied.
                occLevel = OCC_PROBABLE;
                probablePending = false;
                }
            else
                {
                // Room was very dark; defer until light is left on.
                // Note weak occupancy in the interim,
                // which should not wake anything up.
                occLevel = OCC_WEAK;
                probablePending = true;
                }
            }
        }
    // Else if steady long enough look for weak occupancy indications.
    // Look for habitual use of artificial lighting at set times,
    // eg for TV watching or reading.
    // This must have a non-extreme sane mean for the current time of day,
    // and sane correctly-ordered min and max bounds.
    // and any rise must be small
    // and levels must be fairly steady for a while (> ~30 minutes)
    // eg to guard against (eg) sunlight-driven flicker.
    //
    // TODO: this could avoid providing weak occupancy signals indefinitely,
    // eg with an upper limit on steadyTicksMinForArtificialLight.
    //
    // See evening levels for trace 3l here for example:
    //     http://www.earth.org.uk/img/20161124-16WWal.png
    else if((steadyTicks >= steadyTicksMinForArtificialLight) &&
            (minToUse < meanNowOrFF) && (meanNowOrFF < maxToUse)) // Implicitly 0xff != meanNowOrFF.
        {
        // Previous and current light levels should ideally be
        // well away from maximum/minimum
        // (and asymmetrically much further below maximum,
        // ie a wider margin on the high side)
        // to avoid being triggered in continuously dark/lit areas,
        // and when daylit.
        // The level must also be close to the mean for the time of day.
        const uint8_t range = maxToUse - minToUse;
        if(range > 2*epsilon)
            {
            // This measure only makes sense if there is normally
            // a reasonably dynamic ambient light range
            // so that all the time lights levels are not trivially 'steady'.
            constexpr uint8_t marginWshift = 1;
            const uint8_t marginW = range >> (/*sensitive ? (1+marginWshift) : */ marginWshift);
            const uint8_t margin = uint8_t(marginW >> 2);
            const uint8_t thrL = minToUse + margin;
            const uint8_t thrH = maxToUse - marginW;
            const uint8_t maxDistanceFromMean = fnmin(meanNowOrFF-minToUse, maxToUse-meanNowOrFF) >> 1; // (sensitive ? 1 : 2);

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
        }

	prevLightLevel = newLightLevel;
    return(occLevel);
	}
}
