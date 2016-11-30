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

#ifndef OTV0P2BASE_SENSORAMBLIGHT_H
#define OTV0P2BASE_SENSORAMBLIGHT_H

#include "OTV0P2BASE_Util.h"
#include "OTV0P2BASE_Sensor.h"
#include "OTV0P2BASE_SensorAmbientLightOccupancy.h"


namespace OTV0P2BASE
{


// Sense (usually non-linearly) over full likely internal ambient lighting range of a (UK) home,
// down to levels too dark to be active in (and at which heating could be set back for example).
// This suggests a full scale of at least 50--100 lux, maybe as high as 300 lux, eg see:
// http://home.wlv.ac.uk/~in6840/Lightinglevels.htm
// http://www.engineeringtoolbox.com/light-level-rooms-d_708.html
// http://www.pocklington-trust.org.uk/Resources/Thomas%20Pocklington/Documents/PDF/Research%20Publications/GPG5.pdf
// http://www.vishay.com/docs/84154/appnotesensors.pdf

#define SensorAmbientLightBase_DEFINED
// Base class for ambient light sensors, including mocks.
class SensorAmbientLightBase : public SimpleTSUint8Sensor
  {
  protected:
    // True iff room appears lit well enough for activity.
    // Marked volatile for ISR-/thread- safe (simple) lock-free access.
    volatile bool isRoomLitFlag = false;

    // Set true if ambient light sensor may be unusable or unreliable.
    // This will be where (for example) there are historic values
    // but in a very narrow range which implies a broken sensor or shadowed location.
    bool unusable = false;

    // Number of minutes (read() calls) that the room has been continuously dark for [0,255].
    // Does not roll over from maximum value.
    // Reset to zero in light.
    uint8_t darkTicks = 0;

  public:
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

    // Returns true if this sensor is apparently unusable.
    virtual bool isAvailable() const final override { return(!unusable); }

    // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
    // The lifetime of the pointed-to text must be at least that of the Sensor instance.
    virtual Sensor_tag_t tag() const override { return(V0p2_SENSOR_TAG_F("L")); }

    // Returns true if room is probably lit enough for someone to be active, with some hysteresis.
    // False if unknown or sensor appears unusable.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    bool isRoomLit() const { return(isRoomLitFlag && !unusable); }

    // Returns true if room is probably too dark for someone to be active, with some hysteresis.
    // False if unknown or sensor appears unusable,
    // thus it is possible for both isRoomLit() and isRoomDark() to be false.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    bool isRoomDark() const { return(!isRoomLitFlag && !unusable); }

    // Returns true if room is probably pitch dark dark; no hysteresis.
    // Not all light sensors and thus devices may reliably get this low,
    // and some devices may be jammed down the back of a sofa in the pitch dark with this true,
    // thus this should only be treated as an extra hint when true.
    bool isRoomVeryDark() const { return(!unusable && (get() <= DEFAULT_PITCH_DARK_THRESHOLD)); }

    // Get number of minutes (read() calls) that the room has been continuously dark for [0,255].
    // Does not roll over from maximum value, ie stays at 255 until the room becomes light.
    // Reset to zero in light.
    // Stays at zero if the sensor decides that it is unusable.
    uint8_t getDarkMinutes() const { return(darkTicks); }
  };

// Accepts stats updates to adapt better to the location fitted.
// Also supports occupancy sensing and callbacks for reporting it.
class SensorAmbientLightAdaptive : public SensorAmbientLightBase
  {
  protected:
    // Minimum eg from rolling stats, to allow auto adjustment to dark; ~0/0xff means no min available.
    uint8_t rollingMin = 0xff;
    // Maximum eg from rolling stats, to allow auto adjustment to dark; ~0/0xff means no max available.
    uint8_t rollingMax = 0xff;

    // Dark/light thresholds (on [0,254] scale) incorporating hysteresis.
    // So lightThreshold is strictly greater than darkThreshold.
    uint8_t lightThreshold = DEFAULT_LIGHT_THRESHOLD;
    static constexpr uint8_t DEFAULT_upDelta = OTV0P2BASE::fnmax(1, DEFAULT_LIGHT_THRESHOLD >> 2); // Delta ~25% of light threshold.
    uint8_t darkThreshold = DEFAULT_LIGHT_THRESHOLD - DEFAULT_upDelta;

    // Embedded occupancy detection object.
    // May be moved out of here to stand alone,
    // or could be parameterised at compile time with a template,
    // or at run time by being constructed with a pointer to the base type.
    SensorAmbientLightOccupancyDetectorSimple occupancyDetector;

    // 'Possible occupancy' callback function (for moderate confidence of human presence).
    // If not NULL, is called when this sensor detects indications of occupancy.
    // A true argument indicates probable occupancy, false weak occupancy.
    void (*occCallbackOpt)(bool) = NULL;

    // Recomputes thresholds and 'unusable' based on current state.
    //   * meanNowOrFF  mean ambient light level in this time interval; 0xff if none.
    //   * sensitive  if true be more sensitive to possible occupancy changes, else less so.
    void recomputeThresholds(uint8_t meanNowOrFF, bool sensitive);

  public:
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
    // Call regularly, at least roughly hourly, to drive other internal time-dependent adaptation.
    //   * meanNowOrFF  typical/mean light level around this time each 24h; 0xff if not known.
    //   * sensitive  if true be more sensitive to possible occupancy changes, else less so.
    void setTypMinMax(uint8_t meanNowOrFF,
                   uint8_t longerTermMinimumOrFF = 0xff, uint8_t longerTermMaximumOrFF = 0xff,
                   bool sensitive = true);

    // Updates other values based on what is in value.
    // Derived classes may wish to set value first, then call this.
    virtual uint8_t read() override;
  };


// Class primarily to support simple mocking for unit tests.
// Also allows testing of common algorithms in the base classes.
// Set desired raw light value with set() then call read().
class SensorAmbientLightAdaptiveMock : public SensorAmbientLightAdaptive
  {
  public:
    // Set new value.
    virtual bool set(const uint8_t newValue) { value = newValue; return(true); }
    // Set new non-dependent values immediately.
    virtual bool set(const uint8_t newValue, const uint8_t newDarkTicks, const bool isUnusable = false)
        { value = newValue; unusable = isUnusable; darkTicks = newDarkTicks; return(true); }
  };


#ifdef ARDUINO_ARCH_AVR
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
class SensorAmbientLight final : public SensorAmbientLightAdaptive
  {
  private:
  public:
    constexpr SensorAmbientLight() { }

    // Force a read/poll of the ambient light level and return the value sensed [0,255] (dark to light).
    // Potentially expensive/slow.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    // If possible turn off all local light sources (eg UI LEDs) before calling.
    // If possible turn off all heavy current drains on supply before calling.
    virtual uint8_t read();
  };
#endif // ARDUINO_ARCH_AVR


// Dummy placeholder AmbientLight sensor class with always-false dummy static status methods.
// These methods should be fully optimised away by the compiler in many/most cases.
// Can be to reduce code complexity, by eliminating some need for pre-processing.
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
    constexpr static uint8_t getDarkMinutes() { return(0); }
  };


}
#endif
