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
 * Notion of basic mode of OpenTRV thermostatic radiator valve: FROST, WARM or BAKE.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_VALVEMODE_H
#define ARDUINO_LIB_OTRADVALVE_VALVEMODE_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include "OTV0P2BASE_Concurrency.h"
#include "OTV0P2BASE_Sensor.h"
#include "OTRadValve_Parameters.h"


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


// Abstract class for motor drive.
// Supports abstract model plus remote (wireless) and local/direct implementations.
// Implementations may require read() called at a fixed rate,
// though should tolerate calls being skipped when time is tight for other operations,
// since read() may take substantial time (hundreds of milliseconds).
// Implementations must document when read() calls are critical,
// and/or expose alternative API for the time-critical elements.
class ValveMode : public OTV0P2BASE::SimpleTSUint8Sensor
  {
  private:
    // If true then is in WARM (or BAKE) mode; defaults to (starts as) false/FROST.
    // Should be only be set when 'debounced'.
    // Defaults to (starts as) false/FROST.
    // Marked volatile to allow atomic access from ISR without a lock.
    volatile bool isWarmMode = false;

    // Only relevant if isWarmMode is true.
    // Marked volatile to allow atomic access from ISR without a lock; decrements should lock out interrupts.
    volatile OTV0P2BASE::Atomic_UInt8T bakeCountdownM;

  public:
    // Modes.
    // Starts in VMODE_FROST.
    typedef enum { VMODE_FROST, VMODE_WARM, VMODE_BAKE } mode_t;

    // Construct an instance.
    ValveMode() : bakeCountdownM(0) { }

    // Returns true if the mode value passed is valid, ie in range [0,2].
    virtual bool isValid(const uint8_t value) const { return(value <= VMODE_BAKE); }

    // Set new mode value; if VMODE_BAKE then (re)starts the BAKE period.
    // Ignores invalid values.
    // If this returns true then the new target value was accepted.
    virtual bool set(const uint8_t newValue)
        {
        switch(newValue)
            {
            case VMODE_FROST: { setWarmModeDebounced(false); break; }
            case VMODE_WARM: { setWarmModeDebounced(true); break; }
            case VMODE_BAKE: { startBake(); break; }
            default: { return(false); } // Ignore bad values.
            }
        value = newValue;
        return(true); // Accept good value.
        }

    // Overload to allow set directly with enum value.
    bool set(const mode_t newValue) { return(set((uint8_t) newValue)); }

    // Compute state from underlying.
    uint8_t _get() const
        {
        if(!isWarmMode) { return(VMODE_FROST); }
        if(0 != bakeCountdownM.load()) { return(VMODE_BAKE); }
        return(VMODE_WARM);
        }

    // Call this nominally every minute to manage internal state (eg run down the BAKE timer).
    // Not thread-/ISR- safe.
    virtual uint8_t read()
      {
      OTV0P2BASE::safeDecIfNZWeak(bakeCountdownM);
//      ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
//        {
//        // Run down the BAKE mode timer if need be, one tick per minute.
//        if(bakeCountdownM > 0) { --bakeCountdownM; }
//        }
      // Recompute value from underlying.
      value = _get();
      return(value);
      }

    // Preferred poll interval (in seconds); should be called at constant rate, usually 1/60s.
    virtual uint8_t preferredPollInterval_s() const { return(60); }

    // Original V0p09/V0p2 API.
    // If true then the unit is in 'warm' (heating) mode, else 'frost' protection mode.
    bool inWarmMode() const { return(isWarmMode); }
    // Has the effect of forcing the warm mode to the specified state immediately.
    // Should be only be called once 'debounced' if coming from a button press for example.
    // If forcing to FROST mode then any pending BAKE time is cancelled.
    void setWarmModeDebounced(const bool warm)
      {
      isWarmMode = warm;
      if(!warm) { cancelBakeDebounced(); }
      }
    // If true then the unit is in 'BAKE' mode, a subset of 'WARM' mode which boosts the temperature target temporarily.
    // ISR-safe (though may yield stale answer if warm is set false concurrently).
    bool inBakeMode() const { return(isWarmMode && (0 != bakeCountdownM.load())); }
    // Should be only be called once 'debounced' if coming from a button press for example.
    // Cancel 'bake' mode if active; does not force to FROST mode.
    void cancelBakeDebounced() { bakeCountdownM.store(0); }
    // Start/restart 'BAKE' mode and timeout.
    // Should ideally be only be called once 'debounced' if coming from a button press for example.
    // Is thread-/ISR- safe (though may have no effect if warm is set false concurrently).
    void startBake() { isWarmMode = true; bakeCountdownM.store(DEFAULT_BAKE_MAX_M); }
  };


    }

#endif
