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

#ifndef ARDUINO_LIB_TEMPCONTROL_VALVEMODE_H
#define ARDUINO_LIB_TEMPCONTROL_VALVEMODE_H


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


#ifdef ARDUINO_ARCH_AVR
// Non-volatile (EEPROM) stored WARM threshold for some devices without physical controls, eg REV1.
// Typically selected if defined(ENABLE_SETTABLE_TARGET_TEMPERATURES)
#define TempControlSimpleEEPROMBacked_DEFINED
template <class valveControlParams = DEFAULT_ValveControlParameters>
class TempControlSimpleEEPROMBacked : public TempControlBase
  {
  public:
    virtual uint8_t getWARMTargetC()
      {
      // Get persisted value, if any.
      const uint8_t stored = eeprom_read_byte((uint8_t *)V0P2BASE_EE_START_WARM_C);
      // If out of bounds or no stored value then use default (or frost value if set and higher).
      if((stored < OTRadValve::MIN_TARGET_C) || (stored > OTRadValve::MAX_TARGET_C)) { return(OTV0P2BASE::fnmax(valveControlParams::WARM, getFROSTTargetC())); }
      // Return valid persisted value (or frost value if set and higher).
      return(OTV0P2BASE::fnmax(stored, getFROSTTargetC()));
      }

    virtual uint8_t getFROSTTargetC()
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

    // True if WARM temperature at/below halfway mark between eco and comfort levels.
    // Midpoint should be just in eco part to provide a system bias toward eco.
    virtual bool hasEcoBias() { return(getWARMTargetC() <= valveControlParams::TEMP_SCALE_MID); }
    // True if specified temperature is at or below 'eco' WARM target temperature, ie is eco-friendly.
    virtual bool isEcoTemperature(const uint8_t tempC) { return(tempC <= valveControlParams::WARM_ECO); }
    // True if specified temperature is at or above 'comfort' WARM target temperature.
    virtual bool isComfortTemperature(const uint8_t tempC) { return(tempC >= valveControlParams::WARM_COM); }
  };
#endif // ARDUINO_ARCH_AVR


// For REV2 and REV7 style devices with an analogue potentiometer temperature dial.
#ifdef SensorTemperaturePot_DEFINED
#define TempControlTempPot_DEFINED
template <const OTV0P2BASE::SensorTemperaturePot *const pot, class valveControlParams = DEFAULT_ValveControlParameters>
class TempControlTempPot : public TempControlBase
  {
  public:
    TempControlTempPot()
      { }
  };
#endif // SensorTemperaturePot_DEFINED

    }

#endif
