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
#include "OTV0P2BASE_Sensor.h"
#include "OTRadValve_Parameters.h"


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


// Base class for temperature control.
// Default as provided by this base is a single fixed safe room temperature.
// Derived classes support such items as non-volatile CLI-configurable temperatures (eg REV1)
// and analogue temperature potentiometers (such as the REV2 and REV7/DORM1/TRV1).
class TempControlBase // : public OTV0P2BASE::SimpleTSUint8Sensor
  {
  public:
    // If true (the default) then the system has an 'Eco' energy-saving bias, else it has a 'comfort' bias.
    // Several system parameters are adjusted depending on the bias,
    // with 'eco' slanted toward saving energy, eg with lower target temperatures and shorter on-times.
    // This is determined from user-settable temperature values.
    virtual bool hasEcoBias() const { return(true); }

    // Get (possibly dynamically-set) thresholds/parameters.
    // Get 'FROST' protection target in C; no higher than getWARMTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Depends dynamically on current (last-read) temp-pot setting.
    virtual uint8_t getFROSTTargetC() const { return(MIN_TARGET_C); }
    // Get 'WARM' target in C; no lower than getFROSTTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Depends dynamically on current (last-read) temp-pot setting.
    virtual uint8_t getWARMTargetC() const { return(SAFE_ROOM_TEMPERATURE); }

//    #if defined(TEMP_POT_AVAILABLE)
//    // Expose internal calculation of WARM target based on user physical control for unit testing.
//    // Derived from temperature pot position, 0 for coldest (most eco), 255 for hotest (comfort).
//    // Temp ranges from eco-1C to comfort+1C levels across full (reduced jitter) [0,255] pot range.
//    // Everything beyond the lo/hi end-stop thresholds is forced to the appropriate end temperature.
//    uint8_t computeWARMTargetC(uint8_t pot, uint8_t loEndStop, uint8_t hiEndStop);
//    #endif

    // Some systems allow FROST and WARM targets to be set and stored, eg REV1.
    // Set (non-volatile) 'FROST' protection target in C; no higher than getWARMTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Can also be used, even when a temperature pot is present, to set a floor setback temperature.
    // Returns false if not set, eg because outside range [MIN_TARGET_C,MAX_TARGET_C], else returns true.
    virtual bool setFROSTTargetC(uint8_t /*tempC*/) { return(false); } // Does nothing by default.
    // Set 'WARM' target in C; no lower than getFROSTTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
    // Returns false if not set, eg because below FROST setting or outside range [MIN_TARGET_C,MAX_TARGET_C], else returns true.
    virtual bool setWARMTargetC(uint8_t /*tempC*/) { return(false); }  // Does nothing by default.

    // True if specified temperature is at or below 'eco' WARM target temperature, ie is eco-friendly.
    virtual bool isEcoTemperature(uint8_t tempC) const { return(tempC < getWARMTargetC()); } // ((tempC) <= PARAMS::WARM_ECO)
    // True if specified temperature is at or above 'comfort' WARM target temperature.
    virtual bool isComfortTemperature(uint8_t tempC) const { return(tempC > getWARMTargetC()); } // ((tempC) >= PARAMS::WARM_COM)
  };


#ifdef ARDUINO_ARCH_AVR
// Non-volatile (EEPROM) stored WARM threshold for some devices without physical controls, eg REV1.
class TempControlSimpleEEPROMBacked : public TempControlBase
  {

  };
#endif // ARDUINO_ARCH_AVR


    }

#endif
