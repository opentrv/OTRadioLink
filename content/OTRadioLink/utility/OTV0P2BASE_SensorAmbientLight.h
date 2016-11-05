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
    // Default value for (default)LightThreshold.
    // For REV2 LDR and REV7 phototransistor.
    static const uint8_t DEFAULT_LIGHT_THRESHOLD = 16;

    // Returns true if this sensor is apparently unusable.
    virtual bool isAvailable() const final override { return(!unusable); }

    // Returns true if room is probably lit enough for someone to be active, with some hysteresis.
    // False if unknown or sensor appears unusable.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    bool isRoomLit() const { return(isRoomLitFlag && !unusable); }

    // Returns true if room is probably too dark for someone to be active, with some hysteresis.
    // False if unknown or sensor appears unusable,
    // thus it is possible for both isRoomLit() and isRoomDark() to be false.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    bool isRoomDark() const { return(!isRoomLitFlag && !unusable); }

    // Get number of minutes (read() calls) that the room has been continuously dark for [0,255].
    // Does not roll over from maximum value, ie stays at 255 until the room becomes light.
    // Reset to zero in light.
    // Stays at zero if the sensor decides that it is unusable.
    uint8_t getDarkMinutes() const { return(darkTicks); }
  };


// Class primarily to support mocking and unit tests.
class SensorAmbientLightMock : public SensorAmbientLightBase
  {
  public:
    // Set new value.
    virtual bool set(const uint8_t newValue) { value = newValue; return(true); }
    // Set new non-dependent values immediately.
    virtual bool set(const uint8_t newValue, const uint8_t newDarkTicks, const bool isUnusable = false)
        { value = newValue; unusable = isUnusable; darkTicks = newDarkTicks; return(true); }

    // Returns the existing value: use set() to set a new one.
    // Simplistically updates other flags and outputs based on current value.
    uint8_t read() override
        {
        const bool light = (value >= DEFAULT_LIGHT_THRESHOLD);
        isRoomLitFlag = light;
        if(light) { darkTicks = 0; } else if(darkTicks < 255) { ++darkTicks; }
        return(value);
        }
  };


#ifdef ARDUINO_ARCH_AVR
// Sensor for ambient light level; 0 is dark, 255 is bright.
//
// The REV7 implementation expects a phototransitor TEPT4400 (50nA dark current, nominal 200uA@100lx@Vce=50V) from IO_POWER_UP to LDR_SENSOR_AIN and 220k to ground.
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
class SensorAmbientLight final : public SensorAmbientLightBase
  {
  private:
    // Raw ambient light value [0,1023] dark--light, possibly companded.
    uint16_t rawValue;

    // Minimum eg from recent stats, to allow auto adjustment to dark; ~0/0xff means no min available.
    uint8_t recentMin;
    // Maximum eg from recent stats, to allow auto adjustment to dark; ~0/0xff means no max available.
    uint8_t recentMax;

    // Upwards delta used for "lights switched on" occupancy hint; strictly positive.
    uint8_t upDelta;

    // Dark/light thresholds (on [0,254] scale) incorporating hysteresis.
    // So lightThreshold is strictly greater than darkThreshold.
    uint8_t darkThreshold, lightThreshold;

    // Default light threshold (from which dark will be deduced as ~25% lower).
    // Set in constructor.
    // This is used if setMinMax() is not used.
    const uint8_t defaultLightThreshold;

    // 'Possible occupancy' callback function (for moderate confidence of human presence).
    // If not NULL, is called when this sensor detects indications of occupancy.
    void (*possOccCallback)();

    // Recomputes thresholds and 'unusable' based on current state.
    // WARNING: called from (static) constructors so do not attempt (eg) use of Serial.
    //   * sensitive  if true be more sensitive to possible occupancy changes, else less so.
    void _recomputeThresholds(bool sensitive = true);

    // Embedded occupancy detection object.
    // May be moved out of here to stand alone,
    // or could be parameterised at compile time with a template,
    // or at run time by being constructed with a pointer to the base type.
    SensorAmbientLightOccupancyDetectorSimple occupancyDetector;

  public:
    SensorAmbientLight(const uint8_t defaultLightThreshold_ = DEFAULT_LIGHT_THRESHOLD)
      : rawValue((uint16_t) ~0U), // Initial value is distinct.
        recentMin(~0), recentMax(~0),
        defaultLightThreshold(fnmin((uint8_t)254, fnmax((uint8_t)1, defaultLightThreshold_))),
        possOccCallback(NULL)
      { _recomputeThresholds(); }

    // Force a read/poll of the ambient light level and return the value sensed [0,255] (dark to light).
    // Potentially expensive/slow.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    // If possible turn off all local light sources (eg UI LEDs) before calling.
    // If possible turn off all heavy current drains on supply before calling.
    virtual uint8_t read();

    // Preferred poll interval (in seconds); should be called at constant rate, usually 1/60s.
    virtual uint8_t preferredPollInterval_s() const override { return(60); }

    // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
    // The lifetime of the pointed-to text must be at least that of the Sensor instance.
    virtual const char *tag() const override { return("L"); }

    // Get raw ambient light value in range [0,1023].
    // Undefined until first read().
    uint16_t getRaw() const { return(rawValue); }

    // Set 'possible occupancy' callback function (for moderate confidence of human presence); NULL for no callback.
    void setPossOccCallback(void (*possOccCallback_)()) { possOccCallback = possOccCallback_; }

    // Get light threshold, above which room is considered light enough for activity [1,254].
    uint8_t getLightThreshold() const { return(lightThreshold); }

    // Set recent min and max ambient light levels from recent stats, to allow auto adjustment to dark; ~0/0xff means no min/max available.
    // Short term stats are typically over the last day,
    // longer term typically over the last week or so (eg rolling exponential decays).
    // Call regularly, at least roughly hourly, to drive other internal time-dependent adaptation.
    //   * sensitive  if true be more sensitive to possible occupancy changes, else less so.
    // DEPRECATED: use setTypMinMax() with the extra typical/mean parameter.
    void setMinMax(uint8_t recentMinimumOrFF, uint8_t recentMaximumOrFF,
                   uint8_t longerTermMinimumOrFF = 0xff, uint8_t longerTermMaximumOrFF = 0xff,
                   bool sensitive = true)
        { setTypMinMax(0xff, recentMinimumOrFF, recentMaximumOrFF, longerTermMinimumOrFF,longerTermMaximumOrFF, sensitive); }

    // Set recent min and max ambient light levels from recent stats, to allow auto adjustment to dark; ~0/0xff means no min/max available.
    // Short term stats are typically over the last day,
    // longer term typically over the last week or so (eg rolling exponential decays).
    // Call regularly, at least roughly hourly, to drive other internal time-dependent adaptation.
    //   * meanNowOrFF  typical/mean light level around this time each 24h; 0xff if not known.
    //   * sensitive  if true be more sensitive to possible occupancy changes, else less so.
    void setTypMinMax(uint8_t meanNowOrFF,
                   uint8_t recentMinimumOrFF, uint8_t recentMaximumOrFF,
                   uint8_t longerTermMinimumOrFF = 0xff, uint8_t longerTermMaximumOrFF = 0xff,
                   bool sensitive = true);
  };
#endif // ARDUINO_ARCH_AVR


// Dummy placeholder AmbientLight sensor class with always-false dummy static status methods.
// These methods should be fully optimised away by the compiler in many/most cases.
// Can be to reduce code complexity, by eliminating some need for pre-processing.
class DummySensorAmbientLight
  {
  public:
    // Not available, so always a 'dark' value.
    static uint8_t get() { return(0); }

    // Not available, so always returns false.
    static bool isAvailable() { return(false); }

    // Unknown, so always false.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    static bool isRoomLit() { return(false); }

    // Unknown, so always false.
    // Thread-safe and usable within ISRs (Interrupt Service Routines).
    static bool isRoomDark() { return(false); }

    // No sensor, so always zero.
    static uint8_t getDarkMinutes() { return(0); }
  };


}
#endif
