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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2016
*/

/*
 * Temperature control/setting for OpenTRV thermostatic radiator valve.
 *
 * May be fixed or using a supplied potentiometer, for example.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_TEMPCONTROL_H
#define ARDUINO_LIB_OTRADVALVE_TEMPCONTROL_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include "OTRadValve_Parameters.h"


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


// Base class for temperature control.
// Default as provided by this base is a single fixed safe room temperature.
// Derived classes support such items as non-volatile CLI-configurable temperatures (eg REV1)
// and analogue temperature potentiometers (such as the REV2 and REV7/DORM1/TRV1).
class TempControlBase
  {
  public:
    // Get (possibly dynamically-set) thresholds/parameters.
    // Get 'FROST' protection target in C; no higher than getWARMTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Depends dynamically on current (last-read) temp-pot setting.
    virtual uint8_t getFROSTTargetC() const { return(MIN_TARGET_C); }
    // Get 'WARM' target in C; no lower than getFROSTTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Depends dynamically on current (last-read) temp-pot setting.
    virtual uint8_t getWARMTargetC() const { return(SAFE_ROOM_TEMPERATURE); }

    // Some systems allow FROST and WARM targets to be set and stored, eg REV1.
    // Set (non-volatile) 'FROST' protection target in C; no higher than getWARMTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Can also be used, even when a temperature pot is present, to set a floor setback temperature.
    // Returns false if not set, eg because outside range [MIN_TARGET_C,MAX_TARGET_C], else returns true.
    virtual bool setFROSTTargetC(uint8_t /*tempC*/) { return(false); } // Does nothing by default.
    // Set 'WARM' target in C; no lower than getFROSTTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Returns false if not set, eg because below FROST setting or outside range [MIN_TARGET_C,MAX_TARGET_C], else returns true.
    virtual bool setWARMTargetC(uint8_t /*tempC*/) { return(false); }  // Does nothing by default.

    // If true (the default) then the system has an 'Eco' energy-saving bias, else it has a 'comfort' bias.
    // Several system parameters are adjusted depending on the bias,
    // with 'eco' slanted toward saving energy, eg with lower target temperatures and shorter on-times.
    // This is determined from user-settable temperature values.
    virtual bool hasEcoBias() const { return(true); }
    // True if specified temperature is at or below 'eco' WARM target temperature, ie is eco-friendly.
    virtual bool isEcoTemperature(uint8_t tempC) const { return(tempC < SAFE_ROOM_TEMPERATURE); } // ((tempC) <= PARAMS::WARM_ECO)
    // True if specified temperature is at or above 'comfort' WARM target temperature.
    virtual bool isComfortTemperature(uint8_t tempC) const { return(tempC > SAFE_ROOM_TEMPERATURE); } // ((tempC) >= PARAMS::WARM_COM)
  };


// Intermediate templated abstract class that deals with some of the valve control parameters.
#define TempControlSimpleVCP_DEFINED
template <class valveControlParams = DEFAULT_ValveControlParameters>
class TempControlSimpleVCP : public TempControlBase
  {
  public:
    // Get (possibly dynamically-set) thresholds/parameters.
    // Get 'FROST' protection target in C; no higher than getWARMTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Depends dynamically on current (last-read) temp-pot setting.
    virtual uint8_t getFROSTTargetC() const override { return(valveControlParams::FROST); }
    // Get 'WARM' target in C; no lower than getFROSTTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Depends dynamically on current (last-read) temp-pot setting.
    virtual uint8_t getWARMTargetC() const override { return(valveControlParams::WARM); }
    // True if WARM temperature at/below halfway mark between eco and comfort levels.
    // Midpoint should be just in eco part to provide a system bias toward eco.
    virtual bool hasEcoBias() const override { return(getWARMTargetC() <= valveControlParams::TEMP_SCALE_MID); }
    // True if specified temperature is at or below 'eco' WARM target temperature, ie is eco-friendly.
    virtual bool isEcoTemperature(const uint8_t tempC) const override { return(tempC <= valveControlParams::WARM_ECO); }
    // True if specified temperature is at or above 'comfort' WARM target temperature.
    virtual bool isComfortTemperature(const uint8_t tempC) const override { return(tempC >= valveControlParams::WARM_COM); }
  };

#ifdef ARDUINO_ARCH_AVR
// Non-volatile (EEPROM) stored WARM threshold for some devices without physical controls, eg REV1.
// Typically selected if defined(ENABLE_SETTABLE_TARGET_TEMPERATURES)
#define TempControlSimpleEEPROMBacked_DEFINED
template <class valveControlParams = DEFAULT_ValveControlParameters>
class TempControlSimpleEEPROMBacked final : public TempControlSimpleVCP<valveControlParams>
  {
  public:
    virtual uint8_t getWARMTargetC() const
      {
      // Get persisted value, if any.
      const uint8_t stored = eeprom_read_byte((uint8_t *)V0P2BASE_EE_START_WARM_C);
      // If out of bounds or no stored value then use default (or frost value if set and higher).
      if((stored < OTRadValve::MIN_TARGET_C) || (stored > OTRadValve::MAX_TARGET_C)) { return(OTV0P2BASE::fnmax(valveControlParams::WARM, getFROSTTargetC())); }
      // Return valid persisted value (or frost value if set and higher).
      return(OTV0P2BASE::fnmax(stored, getFROSTTargetC()));
      }

    virtual uint8_t getFROSTTargetC() const
      {
      // Get persisted value, if any.
      const uint8_t stored = eeprom_read_byte((uint8_t *)V0P2BASE_EE_START_FROST_C);
      // If out of bounds or no stored value then use default.
      if((stored < OTRadValve::MIN_TARGET_C) || (stored > OTRadValve::MAX_TARGET_C)) { return(valveControlParams::FROST); }
      // TODO-403: cannot use hasEcoBias() with RH% as that would cause infinite recursion!
      // Return valid persisted value.
      return(stored);
      }

    // Set (non-volatile) 'FROST' protection target in C; no higher than getWARMTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Can also be used, even when a temperature pot is present, to set a floor setback temperature.
    // Returns false if not set, eg because outside range [MIN_TARGET_C,MAX_TARGET_C], else returns true.
    bool setFROSTTargetC(const uint8_t tempC)
      {
      if((tempC < OTRadValve::MIN_TARGET_C) || (tempC > OTRadValve::MAX_TARGET_C)) { return(false); } // Invalid temperature.
      if(tempC > getWARMTargetC()) { return(false); } // Cannot set above WARM target.
      OTV0P2BASE::eeprom_smart_update_byte((uint8_t *)V0P2BASE_EE_START_FROST_C, tempC); // Update in EEPROM if necessary.
      return(true); // Assume value correctly written.
      }

    // Set 'WARM' target in C; no lower than getFROSTTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Returns false if not set, eg because below FROST setting or outside range [MIN_TARGET_C,MAX_TARGET_C], else returns true.
    bool setWARMTargetC(const uint8_t tempC)
      {
      if((tempC < OTRadValve::MIN_TARGET_C) || (tempC > OTRadValve::MAX_TARGET_C)) { return(false); } // Invalid temperature.
      if(tempC < getFROSTTargetC()) { return(false); } // Cannot set below FROST target.
      OTV0P2BASE::eeprom_smart_update_byte((uint8_t *)V0P2BASE_EE_START_WARM_C, tempC); // Update in EEPROM if necessary.
      return(true); // Assume value correctly written.
      }
  };
#endif // ARDUINO_ARCH_AVR


// For REV2 and REV7 style devices with an analogue potentiometer temperature dial.
// This can also adjust the temperature thresholds based on relative humidity if a sensor is available.
//
// Expose calculation of WARM target based on user physical control for unit testing.
// Derived from temperature pot position, 0 for coldest (most eco), 255 for hottest (comfort).
// Temp ranges from eco-1C to comfort+1C levels across full (reduced jitter) [0,255] pot range.
// Everything beyond the lo/hi end-stop thresholds is forced to the appropriate end temperature.
template <class valveControlParams = DEFAULT_ValveControlParameters>
uint8_t TempControlTempPot_computeWARMTargetC(const uint8_t pot, const uint8_t loEndStop, const uint8_t hiEndStop)
  {
  // Everything in the end-stop regions is assigned to the appropriate end temperature.
  // As a tiny optimisation note that the in-scale end points must be the end temperatures also.
  if(pot <= loEndStop) { return(valveControlParams::TEMP_SCALE_MIN); } // At/near bottom...
  if(pot >= hiEndStop) { return(valveControlParams::TEMP_SCALE_MAX); } // At/near top...
  // Allow actual full temp range between low and high end points,
  // plus possibly a little more wiggle-room / manufacturing tolerance.
  // Range is number of actual distinct temperatures on scale between end-stop regions.
  const uint8_t usefulScale = hiEndStop - loEndStop + 1;
  constexpr uint8_t DIAL_TEMPS = valveControlParams::TEMP_SCALE_MAX - valveControlParams::TEMP_SCALE_MIN + 1;
  constexpr uint8_t range = DIAL_TEMPS;
  constexpr bool rangeIs1BelowPowerOfTwo = (0 != range) && (0 == (range & (range + 1)));
  // Special-case some ranges to be able to use fast shifts rather than devisions.
  // This also in particular supports REV7/DORM1/TRV1 shimmed for extra space at scale ends.
  // (REV7/DORM1 case is DIAL_TEMPS==7 and usefulScale ~ 47 as of 20160212, eg on first sample unit.)
  // The shim allows for a little more mechanical tolerance as well as CPU efficiency.
  constexpr bool doShim = rangeIs1BelowPowerOfTwo;
  constexpr uint8_t rangeUsed = doShim ? (range+1) : range;
  // Width of band for each degree C...
  const uint8_t band = ((usefulScale+(rangeUsed/2)) / rangeUsed);
  // Adjust for actual bottom of useful range...
  const uint8_t ppotBasic = pot - loEndStop;
  const uint8_t shimWidth = (band >> 1);
  if(doShim && (ppotBasic <= shimWidth)) { return(valveControlParams::TEMP_SCALE_MIN); }
  const uint8_t ppot = (!doShim) ? ppotBasic : (ppotBasic - shimWidth);
  // If there is a relatively small number of distinct temperature values
  // then compute the result iteratively...
  if(DIAL_TEMPS < 10)
    {
    uint8_t result = valveControlParams::TEMP_SCALE_MIN;
    uint8_t bottomOfNextBand = band;
    while((ppot >= bottomOfNextBand) && (result < valveControlParams::TEMP_SCALE_MAX))
      {
      ++result;
      bottomOfNextBand += band;
      }
    return(result);
    }
  // ...else do it in one step with an (expensive) division.
  return((ppot / band) + valveControlParams::TEMP_SCALE_MIN);
  }

#ifdef SensorTemperaturePot_DEFINED
#define TempControlTempPot_DEFINED
// All template parameters must be non-NULL except the humidity sensor.
//   * rh  (const pointer to) relative humidity sensor; NULL if none.
template <const OTV0P2BASE::SensorTemperaturePot *const tempPot, class valveControlParams = DEFAULT_ValveControlParameters, class rh_t = OTV0P2BASE::HumiditySensorBase, const rh_t *rh = (const rh_t *)NULL>
class TempControlTempPot final : public TempControlSimpleVCP<valveControlParams>
  {
  private:
    // Cached input and result values for getWARMTargetC(); initially zero.
    mutable uint8_t potLast = 0;
    mutable uint8_t resultLast = 0;

  public:
    virtual uint8_t getFROSTTargetC() const
      {
      // Prevent falling to lowest frost temperature if relative humidity is high (eg to avoid mould).
      const uint8_t result = (!this->hasEcoBias() || ((NULL != rh) && rh->isAvailable() && rh->isRHHighWithHyst())) ? valveControlParams::FROST_COM : valveControlParams::FROST_ECO;
//    #if defined(ENABLE_SETTABLE_TARGET_TEMPERATURES)
      const uint8_t stored = eeprom_read_byte((uint8_t *)V0P2BASE_EE_START_FROST_C);
      // If stored value is set and in bounds and higher than computed value then use stored value instead.
      if((stored >= OTRadValve::MIN_TARGET_C) && (stored <= OTRadValve::MAX_TARGET_C) && (stored > result)) { return(stored); }
//    #endif
      return(result);
      }

    virtual uint8_t getWARMTargetC() const
      {
      const uint8_t pot = tempPot->get();
      // Force recomputation if pot value changed
      // or apparently no calc done yet (unlikely/impossible zero cached result).
      if((potLast != pot) || (0 == resultLast))
        {
        const uint8_t result = TempControlTempPot_computeWARMTargetC<valveControlParams>(pot, tempPot->loEndStop, tempPot->hiEndStop);
        // Cache input/result.
        resultLast = result;
        potLast = pot;
        return(result);
        }
      // Return cached result.
      return(resultLast);
      }
  };
#endif // SensorTemperaturePot_DEFINED

    }

#endif
