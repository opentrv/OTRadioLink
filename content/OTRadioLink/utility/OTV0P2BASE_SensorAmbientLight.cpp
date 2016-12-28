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
 Ambient light sensor with occupancy detection.

 Specific to V0p2/AVR for now.
 */


#include "OTV0P2BASE_SensorAmbientLight.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "OTV0P2BASE_ADC.h"
#include "OTV0P2BASE_BasicPinAssignments.h"
#include "OTV0P2BASE_Entropy.h"
#include "OTV0P2BASE_PowerManagement.h"
#include "OTV0P2BASE_Sleep.h"


namespace OTV0P2BASE
{


uint8_t SensorAmbientLightAdaptive::read()
    {
    // Adjust room-lit flag, with hysteresis.
    // Should be able to detect dark when darkThreshold is zero and newValue is zero.
    const bool definitelyLit = (value > lightThreshold);
    if(definitelyLit)
        {
        isRoomLitFlag = true;
        // If light enough to set isRoomLitFlag true then reset darkTicks counter.
        darkTicks = 0;
        }
    else if(value <= darkThreshold)
        {
        isRoomLitFlag = false;
        // If dark enough to set isRoomLitFlag false then increment counter
        // (but don't let it wrap around back to zero).
        // Do not increment the count if the sensor seems only dubiously usable.
        if(!rangeTooNarrow && (darkTicks < uint16_t(~0U)))
            { ++darkTicks; }
        }

    // If a callback is set then use the occupancy detector.
    // Suppress WEAK callbacks if the room is not definitely lit.
    if(NULL != occCallbackOpt)
        {
        const OTV0P2BASE::SensorAmbientLightOccupancyDetectorInterface::occType occ = occupancyDetector.update(value);
        // Ping the callback!
        if(occ >= OTV0P2BASE::SensorAmbientLightOccupancyDetectorInterface::OCC_PROBABLE)
            { occCallbackOpt(true); }
        else if(definitelyLit && (occ >= OTV0P2BASE::SensorAmbientLightOccupancyDetectorInterface::OCC_WEAK))
            { occCallbackOpt(false); }
        }

    return(value);
    }


// Maximum value in the uint8_t range.
static constexpr uint8_t MAX_AMBLIGHT_VALUE_UINT8 = 254;

// Recomputes thresholds and 'rangeTooNarrow' based on current state.
//   * meanNowOrFF  typical/mean light level around this time each 24h; 0xff if not known.
//   * sensitive  if true be more sensitive to possible occupancy changes, else less so.
void SensorAmbientLightAdaptive::recomputeThresholds(
        const uint8_t meanNowOrFF, const bool sensitive)
  {
  // If either recent max or min is unset then assume device usable.
  // Use default threshold(s).
  if((0xff == rollingMin) || (0xff == rollingMax))
    {
    // Use the supplied default light threshold and derive the rest from it.
    lightThreshold = DEFAULT_LIGHT_THRESHOLD;
    darkThreshold = DEFAULT_LIGHT_THRESHOLD - DEFAULT_upDelta;
    // Assume OK for now.
    rangeTooNarrow = false;
    return;
    }

  // If the range between recent max and min too narrow then maybe unusable
  // but that may prevent the stats mechanism collecting further values.
  if((rollingMin >= MAX_AMBLIGHT_VALUE_UINT8 - epsilon) ||
     (rollingMax <= rollingMin) ||
     (rollingMax - rollingMin <= epsilon))
    {
    // Use the supplied default light threshold and derive the rest from it.
    lightThreshold = DEFAULT_LIGHT_THRESHOLD;
    darkThreshold = DEFAULT_LIGHT_THRESHOLD - DEFAULT_upDelta;
    // Assume unusable.
    darkTicks = 0; // Scrub any previous possibly-misleading value.
    rangeTooNarrow = true;
    return;
    }

  // Compute thresholds to fit within the observed sensed value range.
  //
  // TODO: a more sophisticated notion of distribution of values may be needed, esp for non-linear scale.
  // TODO: possibly allow a small adjustment on top of this to allow at least one trigger-free hour each day.
  // Some areas may have background flicker eg from trees moving or cars passing, so units there may need desensitising.
  // Could (say) increment an additional margin (up to ~25%) on each non-zero-trigger last hour, else decrement.
  //
  // Default upwards delta indicative of lights on, and hysteresis, is ~12.5% of FSD if default,
  // else half that if sensitive.

  // If current mean is low compared to max then become extra sensitive
  // to try to be able to detect (eg) artificial illumination.
  const bool isLow = meanNowOrFF < (rollingMax>>1);

  // Compute hysteresis.
  const uint8_t e = epsilon;
  const uint8_t upDelta = OTV0P2BASE::fnmax((uint8_t)((rollingMax - rollingMin) >> ((sensitive||isLow) ? 4 : 3)), e);
  // Provide some noise elbow-room above the observed minimum.
  darkThreshold = (uint8_t) OTV0P2BASE::fnmin(254, rollingMin + OTV0P2BASE::fnmax(1, (upDelta>>1)));
  lightThreshold = (uint8_t) OTV0P2BASE::fnmin(rollingMax-1, darkThreshold + upDelta);

  // All seems OK.
  rangeTooNarrow = false;
  }

// Set recent min and max ambient light levels from recent stats, to allow auto adjustment to dark; ~0/0xff means no min/max available.
// Short term stats are typically over the last day,
// longer term typically over the last week or so (eg rolling exponential decays).
// Call regularly, roughly hourly, to drive other internal time-dependent adaptation.
//   * meanNowOrFF  typical/mean light level around this time each 24h; 0xff if not known.
//   * sensitive  if true be more sensitive to possible occupancy changes, else less so.
void SensorAmbientLightAdaptive::setTypMinMax(const uint8_t meanNowOrFF,
                             const uint8_t longerTermMinimumOrFF, const uint8_t longerTermMaximumOrFF,
                             const bool sensitive)
  {
  rollingMin = longerTermMinimumOrFF;
  rollingMax = longerTermMaximumOrFF;

  recomputeThresholds(meanNowOrFF, sensitive);

  // Pass on appropriate properties to the occupancy detector.
  occupancyDetector.setTypMinMax(meanNowOrFF,
          longerTermMinimumOrFF, longerTermMaximumOrFF,
          sensitive);
  }


#ifdef SensorAmbientLight_DEFINED

// Normal raw scale internally is 10 bits [0,1023].
static const uint16_t rawScale = 1024;
// Normal 2 bit shift between raw and externally-presented values.
static const uint8_t shiftRawScaleTo8Bit = 2;

////// If defined, then allow adaptive compression of top part of range when would otherwise max out.
////// This may be somewhat supply-voltage dependent, eg capped by the supply voltage.
////// Supply voltage is expected to be 2--3 times the bandgap reference, typically.
//////#define ADAPTIVE_THRESHOLD 896U // (1024-128) Top ~10%, companding by 8x.
//////#define ADAPTIVE_THRESHOLD 683U // (1024-683) Top ~33%, companding by 4x.
//////static const uint16_t scaleFactor = (extendedScale - ADAPTIVE_THRESHOLD) / (normalScale - ADAPTIVE_THRESHOLD);
////// Assuming typical V supply of 2--3 times Vbandgap,
////// compress above threshold to extend top of range by a factor of two.
////// Ensure that scale stays monotonic in face of calculation lumpiness, etc...
////// Scale all remaining space above threshold to new top value into remaining space.
////// Ensure scaleFactor is a power of two for speed.

// Measure/store/return the current room ambient light levels in range [0,255].
// This may consume significant power and time.
// Probably no need to do this more than (say) once per minute,
// but at a regular rate to catch such events as lights being switched on.
// This implementation expects LDR (1M dark resistance) from IO_POWER_UP to LDR_SENSOR_AIN and 100k to ground,
// or a phototransistor TEPT4400 in place of the LDR.
// (Not intended to be called from ISR.)
// If possible turn off all local light sources (eg UI LEDs) before calling.
// If possible turn off all heavy current drains on supply before calling.
uint8_t SensorAmbientLight::read()
  {
  // Power on to top of LDR/phototransistor, directly connected to IO_POWER_UP.
  OTV0P2BASE::power_intermittent_peripherals_enable(false);
  // Give supply a moment to settle, eg from heavy current draw elsewhere.
  OTV0P2BASE::nap(WDTO_30MS);
  // Photosensor vs Vsupply [0,1023].  // May allow against Vbandgap again for some variants.
  const uint16_t al0 = OTV0P2BASE::analogueNoiseReducedRead(V0p2_PIN_LDR_SENSOR_AIN, DEFAULT); // ALREFERENCE);
  const uint16_t al = al0; // Use raw value as-is.
  // Power off to top of LDR/phototransistor.
  OTV0P2BASE::power_intermittent_peripherals_disable();

  // Compute the new normalised value.
  const uint8_t newValue = (uint8_t)(al >> shiftRawScaleTo8Bit);

  // Capture entropy from changed LS bits.
  if(newValue != value) { ::OTV0P2BASE::addEntropyToPool((uint8_t)al, 0); } // Claim zero entropy as may be forced by Eve.

  // Store new value.
  value = newValue;

  // Have base class update other/derived values.
  SensorAmbientLightAdaptive::read();

#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("Ambient light (/1023): ");
  DEBUG_SERIAL_PRINT(al);
  DEBUG_SERIAL_PRINTLN();
#endif

#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("Ambient light val/dt/lt: ");
  DEBUG_SERIAL_PRINT(value);
  DEBUG_SERIAL_PRINT(' ');
  DEBUG_SERIAL_PRINT(darkThreshold);
  DEBUG_SERIAL_PRINT(' ');
  DEBUG_SERIAL_PRINT(lightThreshold);
  DEBUG_SERIAL_PRINTLN();
#endif

#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("isRoomLit: ");
  DEBUG_SERIAL_PRINT(isRoomLitFlag);
  DEBUG_SERIAL_PRINTLN();
#endif

   return(value);
  }

// DHD20161104: impl removed from main app.
//
//static const uint8_t shiftRawScaleTo8Bit = 2;
//#ifdef ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
//// This implementation expects a phototransitor TEPT4400 (50nA dark current, nominal 200uA@100lx@Vce=50V) from IO_POWER_UP to LDR_SENSOR_AIN and 220k to ground.
//// Measurement should be taken wrt to internal fixed 1.1V bandgap reference, since light indication is current flow across a fixed resistor.
//// Aiming for maximum reading at or above 100--300lx, ie decent domestic internal lighting.
//// Note that phototransistor is likely far more directionally-sensitive than LDR and its response nearly linear.
//// This extends the dynamic range and switches to measurement vs supply when full-scale against bandgap ref, then scales by Vss/Vbandgap and compresses to fit.
//// http://home.wlv.ac.uk/~in6840/Lightinglevels.htm
//// http://www.engineeringtoolbox.com/light-level-rooms-d_708.html
//// http://www.pocklington-trust.org.uk/Resources/Thomas%20Pocklington/Documents/PDF/Research%20Publications/GPG5.pdf
//// http://www.vishay.com/docs/84154/appnotesensors.pdf
//#if (7 == V0p2_REV) // REV7 initial board run especially uses different phototransistor (not TEPT4400).
//// Note that some REV7s from initial batch were fitted with wrong device entirely,
//// an IR device, with very low effective sensitivity (FSD ~ 20 rather than 1023).
//static const int LDR_THR_LOW = 180U;
//static const int LDR_THR_HIGH = 250U;
//#else // REV4 default values.
//static const int LDR_THR_LOW = 270U;
//static const int LDR_THR_HIGH = 400U;
//#endif
//#else // LDR (!defined(ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400))
//// This implementation expects an LDR (1M dark resistance) from IO_POWER_UP to LDR_SENSOR_AIN and 100k to ground.
//// Measurement should be taken wrt to supply voltage, since light indication is a fraction of that.
//// Values below from PICAXE V0.09 impl approx multiplied by 4+ to allow for scale change.
//#ifdef ENABLE_AMBLIGHT_EXTRA_SENSITIVE // Define if LDR not exposed to much light, eg for REV2 cut4 sideways-pointing LDR (TODO-209).
//static const int LDR_THR_LOW = 50U;
//static const int LDR_THR_HIGH = 70U;
//#else // Normal settings.
//static const int LDR_THR_LOW = 160U; // Was 30.
//static const int LDR_THR_HIGH = 200U; // Was 35.
//#endif // ENABLE_AMBLIGHT_EXTRA_SENSITIVE
//#endif // ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
//AmbientLight AmbLight(LDR_THR_HIGH >> shiftRawScaleTo8Bit);

#endif // SensorAmbientLight_DEFINED


}
