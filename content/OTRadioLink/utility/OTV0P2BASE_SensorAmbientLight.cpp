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
  // Power on to top of LDR/phototransistor.
//  power_intermittent_peripherals_enable(false); // No need to wait for anything to stablise as direct of IO_POWER_UP.
  OTV0P2BASE::power_intermittent_peripherals_enable(false); // Will take a nap() below to allow supply to quieten.
  OTV0P2BASE::nap(WDTO_30MS); // Give supply a moment to settle, eg from heavy current draw elsewhere.
  // Photosensor vs Vsupply [0,1023].  // May allow against Vbandgap again for some variants.
  const uint16_t al0 = OTV0P2BASE::analogueNoiseReducedRead(V0p2_PIN_LDR_SENSOR_AIN, DEFAULT); // ALREFERENCE);
  const uint16_t al = al0; // Use raw value as-is.
//#if !defined(EXTEND_OPTO_SENSOR_RANGE)
//  const uint16_t al = al0; // Use raw value as-is.
//#else // defined(EXTEND_OPTO_SENSOR_RANGE)
//  uint16_t al; // Ambient light.
//  // Default shift of raw value to extend effective scale.
//  al = al0 >> shiftExtendedToRawScale;
//  // If simple reading against bandgap at full scale then compute extended range.
//  // Two extra ADC measurements take extra time and introduce noise.
//  if(al0 >= rawScale-1)
//    {
//    // Photosensor vs Vsupply reference, [0,1023].
//    const uint16_t al1 = OTV0P2BASE::analogueNoiseReducedRead(LDR_SENSOR_AIN, DEFAULT);
//    const uint16_t vcc = Supply_cV.read();
//    const uint16_t vbg = Supply_cV.getRawInv(); // Vbandgap wrt Vsupply, [0,1023].
//    // Compute value in extended range up to ~1024 * Vsupply/Vbandgap.
//    // Faster overflow-free uint16_t-only approximation to (uint16_t)((al1 * 1024L) / vbg)).
//    const uint16_t ale = fnmin(4095U, ((al1 << 6) / vbg)) << 4;
//    if(ale > al0) // Keep output scale monotonic...
//      { al = fnmin(1023U, ale >> shiftExtendedToRawScale); }
//#if 1 && defined(DEBUG)
//    DEBUG_SERIAL_PRINT_FLASHSTRING("Ambient raw: ");
//    DEBUG_SERIAL_PRINT(al0);
//    DEBUG_SERIAL_PRINT_FLASHSTRING(", against Vcc: ");
//    DEBUG_SERIAL_PRINT(al1);
//    DEBUG_SERIAL_PRINT_FLASHSTRING(", extended scale value: ");
//    DEBUG_SERIAL_PRINT(ale);
//    DEBUG_SERIAL_PRINT_FLASHSTRING(", Vref against Vcc: ");
//    DEBUG_SERIAL_PRINT(vbg);
//    DEBUG_SERIAL_PRINT_FLASHSTRING(", Vcc: ");
//    DEBUG_SERIAL_PRINT(vcc);
////    DEBUG_SERIAL_PRINT_FLASHSTRING(", es threshold: ");
////    DEBUG_SERIAL_PRINT(aleThreshold);
//    DEBUG_SERIAL_PRINT_FLASHSTRING(", compressed: ");
//    DEBUG_SERIAL_PRINT(al);
//    DEBUG_SERIAL_PRINTLN();
//#endif
//    }
//#endif // defined(EXTEND_OPTO_SENSOR_RANGE)
  // Power off to top of LDR/phototransistor.
  OTV0P2BASE::power_intermittent_peripherals_disable();

  // Capture entropy from changed LS bits.
  if((uint8_t)al != (uint8_t)rawValue) { ::OTV0P2BASE::addEntropyToPool((uint8_t)al, 0); } // Claim zero entropy as may be forced by Eve.

  // Hold the existing/old value for comparison.
  const uint8_t oldValue = value;
  // Compute the new normalised value.
  const uint8_t newValue = (uint8_t)(al >> shiftRawScaleTo8Bit);

  // Adjust room-lit flag, with hysteresis.
  // Should be able to detect dark when darkThreshold is zero and newValue is zero.
  if(newValue <= darkThreshold)
    {
    isRoomLitFlag = false;
    // If dark enough to set isRoomLitFlag false then increment counter.
    // Do not do increment the count if the sensor seems to be unusable / dubiously usable.
    if(!unusable && (darkTicks < 255)) { ++darkTicks; }
    }
  else if(newValue > lightThreshold)
    {
    isRoomLitFlag = true;
    // If light enough to set isRoomLitFlag true then reset darkTicks counter.
    darkTicks = 0;
    }

  // If a callback is set
  // then treat a sharp brightening as a possible/weak indication of occupancy, eg light flicked on.
  if((NULL != possOccCallback) && (newValue > oldValue))
    {
    // Ignore false trigger at start-up,
    // and only respond to a large-enough jump in light levels.
    if((((uint16_t)~0U) != rawValue) && ((newValue - oldValue) >= upDelta))
      {
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINT_FLASHSTRING("  UP: ambient light rise/upDelta/newval/dt/lt: ");
DEBUG_SERIAL_PRINT((newValue - value));
DEBUG_SERIAL_PRINT(' ');
DEBUG_SERIAL_PRINT(upDelta);
DEBUG_SERIAL_PRINT(' ');
DEBUG_SERIAL_PRINT(newValue);
DEBUG_SERIAL_PRINT(' ');
DEBUG_SERIAL_PRINT(darkThreshold);
DEBUG_SERIAL_PRINT(' ');
DEBUG_SERIAL_PRINT(lightThreshold);
DEBUG_SERIAL_PRINTLN();
#endif
      possOccCallback(); // Ping the callback!
      }
    }

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

  // Store new value, in its various forms.
  rawValue = al;
  value = newValue;
  return(value);
  }

// Maximum value in the uint8_t range.
static const uint8_t MAX_AMBLIGHT_VALUE_UINT8 = 254;
// Minimum viable range (on [0,254] scale) to be usable.
static const uint8_t ABS_MIN_AMBLIGHT_RANGE_UINT8 = 3;
// Minimum hysteresis (on [0,254] scale) to be usable and avoid noise triggers.
static const uint8_t ABS_MIN_AMBLIGHT_HYST_UINT8 = 2;

// Recomputes thresholds and 'unusable' based on current state.
// WARNING: called from (static) constructors so do not attempt (eg) use of Serial.
//   * sensitive  if true be more sensitive to possible occupancy changes, else less so.
void SensorAmbientLight::_recomputeThresholds(const bool sensitive)
  {
  // If either recent max or min is unset then assume device usable by default.
  // Use default threshold(s).
  if((0xff == recentMin) || (0xff == recentMax))
    {
    // Use the supplied default light threshold and derive the rest from it.
    lightThreshold = defaultLightThreshold;
    upDelta = OTV0P2BASE::fnmax(1, lightThreshold >> 2); // Delta ~25% of light threshold.
    darkThreshold = lightThreshold - upDelta;
    // Assume OK for now.
    unusable = false;
    return;
    }

  // If the range between recent max and min too narrow then assume unusable.
  if((recentMin >= MAX_AMBLIGHT_VALUE_UINT8 - ABS_MIN_AMBLIGHT_RANGE_UINT8) ||
     (recentMax <= recentMin) ||
     (recentMax - recentMin <= ABS_MIN_AMBLIGHT_RANGE_UINT8))
    {
    // Use the supplied default light threshold and derive the rest from it.
    lightThreshold = defaultLightThreshold;
    upDelta = OTV0P2BASE::fnmax(1, lightThreshold >> 2); // Delta ~25% of light threshold.
    darkThreshold = lightThreshold - upDelta;
    // Assume unusable.
    darkTicks = 0; // Scrub any previous possibly-misleading value.
    unusable = true;
    return;
    }

  // Compute thresholds to fit within the observed sensed value range.
  //
  // TODO: a more sophisticated notion of distribution of values may be needed, esp for non-linear scale.
  // TODO: possibly allow a small adjustment on top of this to allow at least one trigger-free hour each day.
  // Some areas may have background flicker eg from trees moving or cars passing, so units there may need desensitising.
  // Could (say) increment an additional margin (up to ~25%) on each non-zero-trigger last hour, else decrement.
  //
  // Take upwards delta indicative of lights on, and hysteresis, as ~12.5% of FSD if 'sensitive'/default,
  // else 25% if less sensitive.
  //
  // TODO: possibly allow a small adjustment on top of this to allow >= 1 each trigger and trigger-free hours each day.
  // Some areas may have background flicker eg from trees moving or cars passing, so units there may need desensitising.
  // Could (say) increment an additional margin (up to half) on each non-zero-trigger last hour, else decrement.
  upDelta = OTV0P2BASE::fnmax((uint8_t)((recentMax - recentMin) >> (sensitive ? 3 : 2)), ABS_MIN_AMBLIGHT_HYST_UINT8);
  // Provide some noise elbow-room above the observed minimum.
  // Set the hysteresis values to be the same as the upDelta.
  darkThreshold = (uint8_t) OTV0P2BASE::fnmin(254, recentMin+1 + (upDelta>>1));
  lightThreshold = (uint8_t) OTV0P2BASE::fnmin(recentMax-1, darkThreshold + upDelta);

  // All seems OK.
  unusable = false;
  }

// Set recent min and max ambient light levels from recent stats, to allow auto adjustment to dark; ~0/0xff means no min/max available.
// Short term stats are typically over the last day,
// longer term typically over the last week or so (eg rolling exponential decays).
// Call regularly, roughly hourly, to drive other internal time-dependent adaptation.
//   * sensitive if true be more sensitive to possible occupancy changes, else less so.
void SensorAmbientLight::setMinMax(const uint8_t recentMinimumOrFF, const uint8_t recentMaximumOrFF,
                             const uint8_t longerTermMinimumOrFF, const uint8_t longerTermMaximumOrFF,
                             const bool sensitive)
  {
  // Simple approach: will ignore an 'unset'/0xff value if the other is good.
  recentMin = OTV0P2BASE::fnmin(recentMinimumOrFF, longerTermMinimumOrFF);

  if(0xff == recentMaximumOrFF) { recentMin = longerTermMaximumOrFF; }
  else if(0xff == longerTermMaximumOrFF) { recentMin = recentMaximumOrFF; }
  else
    {
    // Both values available; weight towards the more recent one for quick adaptation.
    recentMax = (uint8_t) (((3*(uint16_t)recentMaximumOrFF) + (uint16_t)longerTermMaximumOrFF) >> 2);
    }

  _recomputeThresholds(sensitive);

#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("Ambient recent min/max: ");
  DEBUG_SERIAL_PRINT(recentMin);
  DEBUG_SERIAL_PRINT(' ');
  DEBUG_SERIAL_PRINT(recentMax);
  if(unusable) { DEBUG_SERIAL_PRINT_FLASHSTRING(" UNUSABLE"); }
  DEBUG_SERIAL_PRINTLN();
#endif
  }

#endif // SensorAmbientLight_DEFINED


}
