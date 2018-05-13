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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2018
*/

/*
 Ambient light sensor with occupancy detection.

 Specific to V0p2/AVR for now.
 */

#ifndef OTV0P2BASE_SENSORAMBLIGHT_H
#define OTV0P2BASE_SENSORAMBLIGHT_H

#include "OTV0P2BASE_Util.h"
#include "OTV0P2BASE_Sensor.h"
#include "OTV0P2BASE_SensorAmbientLightOccupancy.h"
#include "OTV0P2BASE_ADC.h"
#include "OTV0P2BASE_Entropy.h"


namespace OTV0P2BASE
{


// Sense ambient indoor home lighting levels.
// Sense (usually non-linearly) over full likely internal
// ambient lighting range of a (UK) home,
// down to levels too dark to be active in
// (and at which heating could be set back for example).
// This suggests a full scale of at least 50--100 lux,
// maybe as high as 300 lux, eg see:
// http://home.wlv.ac.uk/~in6840/Lightinglevels.htm
// http://www.engineeringtoolbox.com/light-level-rooms-d_708.html
// http://www.pocklington-trust.org.uk/Resources/Thomas%20Pocklington/Documents/PDF/Research%20Publications/GPG5.pdf
// http://www.vishay.com/docs/84154/appnotesensors.pdf
// https://academic.oup.com/aje/article-abstract/187/3/427/4056592

#define SensorAmbientLightBase_DEFINED
// Base class for ambient light sensors, including mocks.
class SensorAmbientLightBase : public SimpleTSUint8Sensor
  {
  protected:
    // True iff room appears lit well enough for activity.
    // Marked volatile for ISR-/thread- safe (simple) lock-free access.
    volatile bool isRoomLitFlag = false;

    // Set true if ambient light sensor range may be too small to use.
    // This will be where (for example) there are historic values
    // but in a very narrow range which implies a broken sensor or
    // shadowed location.
    // This does not mark the entire sensor/device as unavailable,
    // eg so that stats can go on being collected in case things improve,
    // but it does disable all the assertions about dark/light/ticks.
    bool rangeTooNarrow = false;

    // read() calls / mins that the room has been continuously dark for.
    // Does not roll over from maximum value.
    // Reset to zero in light.
    // Stays at zero if the sensor decides that its range is too narrow.
    // May not count up while in hysteresis range.
    uint16_t darkTicks = 0;

  public:
    // Reset to starting state; primarily for unit tests.
    void reset() { value = 0; isRoomLitFlag = false; rangeTooNarrow = false; darkTicks = 0; }

    // Default value for lightThreshold; a dimly light room at night may be brighter.
    // For REV2 LDR and REV7 phototransistor.
    static const uint8_t DEFAULT_LIGHT_THRESHOLD = 16;

    // Default 'very dark' threshold; at or below this a room is pitch black.
    // Not all light sensors and thus devices may reliably get this low,
    // though many may get down to 1 or even 0.
    // Some *very* poorly-lit locations may get this low even when occupied.
    // Some locations may be prevented from getting this dark by night-lights.
    // For REV2 LDR and REV7 phototransistor.
    static const uint8_t DEFAULT_PITCH_DARK_THRESHOLD = 4;

    // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
    // The lifetime of the pointed-to text must be at least that
    // of the Sensor instance.
    virtual Sensor_tag_t tag() const override { return(V0p2_SENSOR_TAG_F("L")); }

    // Returns true if room is probably lit enough for someone to be active, with some hysteresis.
    // False if unknown or sensor range appears too narrow.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    bool isRoomLit() const { return(isRoomLitFlag && !rangeTooNarrow); }

    // Returns true if room is probably too dark for someone to be active, with some hysteresis.
    // False if unknown or sensor range appears too narrow.
    // thus it is possible for both isRoomLit() and isRoomDark()
    // to be false.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    bool isRoomDark() const { return(!isRoomLitFlag && !rangeTooNarrow); }

    // Returns true if room is probably pitch dark dark; no hysteresis.
    // Not all light sensors and thus devices may reliably get this low,
    // and some devices may be jammed down the back of a sofa
    // in the pitch dark with this almost permanently true,
    // thus this should only be treated as an extra hint when true.
    bool isRoomVeryDark() const { return((get() <= DEFAULT_PITCH_DARK_THRESHOLD) && !rangeTooNarrow); }

    // Get number of minutes (read() calls) that the room has been continuously dark for.
    // Does not roll over from maximum value.
    // Reset to zero in light.
    // Stays at zero if the sensor decides that its range is too narrow.
    // May not count up while in hysteresis range.
    uint16_t getDarkMinutes() const { return(darkTicks); }

    // Returns true if ambient light range seems to be too narrow to be reliable.
    bool isRangeTooNarrow() const { return(rangeTooNarrow); }
  };

// Accepts stats updates to adapt better to the location fitted.
// Also supports occupancy sensing and callbacks for reporting it.
// Parameterise with any SensorAmbientLightOccupancyDetectorInterface.
template <class occupancyDetector_t = SensorAmbientLightOccupancyDetectorSimple>
class SensorAmbientLightAdaptiveTBase : public SensorAmbientLightBase
  {
  public:
    // Minimum hysteresis; a simple noise floor.
    static constexpr uint8_t epsilon = 4;

  protected:
    // Minimum eg from rolling stats, to allow auto adjustment to dark; ~0/0xff means no min available.
    uint8_t rollingMin = 0xff;
    // Maximum eg from rolling stats, to allow auto adjustment to dark; ~0/0xff means no max available.
    uint8_t rollingMax = 0xff;

    // Dark/light thresholds (on [0,254] scale) incorporating hysteresis.
    // So lightThreshold is strictly greater than darkThreshold.
    uint8_t lightThreshold = SensorAmbientLightBase::DEFAULT_LIGHT_THRESHOLD;
    static constexpr uint8_t DEFAULT_upDelta = OTV0P2BASE::fnmax(1, SensorAmbientLightBase::DEFAULT_LIGHT_THRESHOLD >> 2); // Delta ~25% of light threshold.
    uint8_t darkThreshold = SensorAmbientLightBase::DEFAULT_LIGHT_THRESHOLD - DEFAULT_upDelta;

    // Embedded occupancy detection object.
    // Instance of SensorAmbientLightOccupancyDetectorInterface.
    occupancyDetector_t occupancyDetector;

    // 'Possible occupancy' callback function (for moderate confidence of human presence).
    // If not NULL, is called when this sensor detects indications
    // of occupancy.
    // A true argument indicates probable occupancy,
    // false weak occupancy.
    void (*occCallbackOpt)(bool) = NULL;

    // Recomputes thresholds and 'rangeTooNarrow' based on current state.
    //   * meanNowOrFF  typical/mean light level around this time
    //     each 24h; 0xff if not known.
    //   * sensitive  if true be more sensitive to possible
    //     occupancy changes, else less so.
    void recomputeThresholds(
            const uint8_t meanNowOrFF, const bool sensitive)
      {
      // Maximum value in the uint8_t range.
      static constexpr uint8_t MAX_AMBLIGHT_VALUE_UINT8 = 254;

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


  public:
    // Reset to starting state; primarily for unit tests.
    void resetAdaptive() { reset(); occCallbackOpt = NULL; setTypMinMax(0xff, 0xff, 0xff, false); occupancyDetector.reset(); }

    // Get light threshold, above which the room is considered light enough for activity [1,254].
    uint8_t getLightThreshold() const { return(lightThreshold); }

    // Get dark threshold, at or below which the room is considered too dark for activity [0,253].
    uint8_t getDarkThreshold() const { return(darkThreshold); }

    // Preferred poll interval (in seconds); should be called at constant rate, usually 1/60s.
    virtual uint8_t preferredPollInterval_s() const override { return(60); }

    // Set 'possible'/weak occupancy callback function; NULL for no callback.
    void setOccCallbackOpt(void (*occCallbackOpt_)(bool)) { occCallbackOpt = occCallbackOpt_; }

    // Set recent min and max ambient light levels from stats, to allow auto adjustment to dark; ~0/0xff means no min/max available.
    // Longer term typically over the last week or so (eg rolling exponential decay).
    // Call typically hourly with updated stats,
    // to set other internal time-dependent adaptation.
    //   * meanNowOrFF  typical/mean light level around this time each 24h;
    //         0xff if not known.
    //   * sensitive  if true be more sensitive to possible occupancy changes,
    //         which may mean more false positives and less energy saving
    void setTypMinMax(const uint8_t meanNowOrFF,
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

    // Updates other values based on what is in value.
    // Derived classes may wish to set value first, then call this.
    virtual uint8_t read() override
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
  };

// Use default detector implementation.
class SensorAmbientLightAdaptive : public SensorAmbientLightAdaptiveTBase<>
{ } ;


// Class primarily to support simple mocking for unit tests.
// Also allows testing of common algorithms in the base classes.
// Set desired raw light value with set() then call read().
class SensorAmbientLightAdaptiveMock : public SensorAmbientLightAdaptive
  {
  public:
    // Set new value.
    virtual bool set(const uint8_t newValue) { value = newValue; return(true); }
    // Set new non-dependent values immediately.
    virtual bool set(const uint8_t newValue, const uint16_t newDarkTicks, const bool isRangeTooNarrow = false)
        { value = newValue; rangeTooNarrow = isRangeTooNarrow; darkTicks = newDarkTicks; return(true); }

    // Expose the occupancy detector read-only for tests.
    const SensorAmbientLightOccupancyDetectorSimple &_occDet = occupancyDetector;
  };


#ifdef ARDUINO_ARCH_AVR
template <uint8_t lightSensorADCChannel>
class SensorAmbientLightConfigurable final : public SensorAmbientLightAdaptive
{
private:
public:
    constexpr SensorAmbientLightConfigurable() { }

    // Normal raw scale internally is 10 bits [0,1023].
    static constexpr uint16_t rawScale = 1024;
    // Normal 2 bit shift between raw and externally-presented values.
    static constexpr uint8_t shiftRawScaleTo8Bit = 2;

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
    uint8_t read()
    {
        // Power on to top of LDR/phototransistor, directly connected to IO_POWER_UP.
        OTV0P2BASE::power_intermittent_peripherals_enable(false);
        // Give supply a moment to settle, eg from heavy current draw elsewhere.
        OTV0P2BASE::nap(WDTO_30MS);
        // Photosensor vs Vsupply [0,1023].  // May allow against Vbandgap again for some variants.
        const uint16_t al0 = OTV0P2BASE::analogueNoiseReducedRead(lightSensorADCChannel, DEFAULT);
        const uint16_t al = al0; // Use raw value as-is.
        // Power off to top of LDR/phototransistor.
        OTV0P2BASE::power_intermittent_peripherals_disable();

        // Compute the new normalised value.
        const uint8_t newValue = (uint8_t)(al >> shiftRawScaleTo8Bit);

        // Capture entropy from changed ls bits.
        // Claim zero entropy as value may be partly directly forced by Eve.
        if(newValue != value) { addEntropyToPool((uint8_t)al, 0); }

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
};

// Sensor for ambient light level; 0 is dark, 255 is bright.
//
// The REV7 implementation expects a phototransistor TEPT4400 (50nA dark current, nominal 200uA@100lx@Vce=50V) from IO_POWER_UP to LDR_SENSOR_AIN and 220k to ground.
// Measurement should be taken wrt to internal fixed 1.1V bandgap reference, since light indication is current flow across a fixed resistor.
// Aiming for maximum reading at or above 100--300lx, ie decent domestic internal lighting.
// Note that phototransistor is likely far more directionally-sensitive than REV2's LDR and its response nominally nearly linear.
// This extends the dynamic range and switches to measurement vs supply when full-scale against bandgap ref, then scales by Vss/Vbandgap and compresses to fit.
// http://home.wlv.ac.uk/~in6840/Lightinglevels.htm
// http://www.engineeringtoolbox.com/light-level-rooms-d_708.html
// http://www.pocklington-trust.org.uk/Resources/Thomas%20Pocklington/Documents/PDF/Research%20Publications/GPG5.pdf
// http://www.vishay.com/docs/84154/appnotesensors.pdf
//
// The REV2 implementation expects an LDR (1M dark resistance) from IO_POWER_UP to LDR_SENSOR_AIN and 100k to ground.
// Measurement should be taken wrt to supply voltage, since light indication is a fraction of that.
// Values below from PICAXE V0.09 impl approx multiplied by 4+ to allow for scale change.
#define SensorAmbientLight_DEFINED
using SensorAmbientLight = SensorAmbientLightConfigurable<V0p2_PIN_LDR_SENSOR_AIN>;
#endif // ARDUINO_ARCH_AVR


// Dummy placeholder AmbientLight sensor class with always-false dummy static status methods.
// These methods should be fully optimised away by the compiler
// in many/most cases.
// Can be to reduce code complexity,
// by eliminating some need for pre-processing.
class DummySensorAmbientLight
  {
  public:
    // Not available, so always a 'dark' value.
    constexpr static uint8_t get() { return(0); }

    // Not available, so always returns false.
    constexpr static bool isAvailable() { return(false); }

    // Unknown, so always false.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    constexpr static bool isRoomLit() { return(false); }

    // Unknown, so always false.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    constexpr static bool isRoomDark() { return(false); }

    // No sensor, so always zero.
    constexpr static uint16_t getDarkMinutes() { return(0); }
  };


}
#endif
