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
 * OpenTRV radiator valve physical UI controls and output(s) as an actuator.
 *
 * A base class, a null class, and one or more implementations are provided
 * for different stock behaviour with different hardware.
 *
 * A mixture of template and constructor parameters
 * is used to configure the different types.
 */

#ifndef ARDUINO_LIB_ACTUATOR_PHYSICAL_UI_H
#define ARDUINO_LIB_ACTUATOR_PHYSICAL_UI_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include "OTV0P2BASE_Actuator.h"


// Use namespaces to help avoid collisions.
namespace OTV0P2BASE
    {


// Base class for physical UI controls on V0p2 devices.
class ActuatorPhysicalUIBase : public OTV0P2BASE::SimpleTSUint8Actuator
  {
  protected:
    // Prevent direct creation of naked instance of this base/abstract class.
    ActuatorPhysicalUIBase() { }

  public:
    // Set a new target output indication, eg mode.
    // If this returns true then the new target value was accepted.
    virtual bool set(const uint8_t /*newValue*/) { return(false); }

    // Call this nominally on even numbered seconds to allow the UI to operate.
    // In practice call early once per 2s major cycle.
    // Should never be skipped, so as to allow the UI to remain responsive.
    // Runs in 350ms or less; usually takes only a few milliseconds or microseconds.
    // Returns a non-zero value iff the user interacted with the system, and maybe caused a status change.
    // NOTE: since this is on the minimum idle-loop code path, minimise CPU cycles, esp in frost mode.
    // Replaces: bool tickUI(uint_fast8_t sec);
    virtual uint8_t read();

    // Preferred poll interval (in seconds); should be called at constant rate, usually 1/60s.
    virtual uint8_t preferredPollInterval_s() const { return(2); }
  };


// Null UI: always returns false for read() and does nothing with set().
// Has no physical interactions with devices.
class NullActuatorPhysicalUI : public ActuatorPhysicalUIBase
  {
  public:
    // Does nothing and forces 'sensor' value to 0 and returns 0.
    virtual uint8_t read() { value = 0; return(value); }
  };



// TODO: MODE button vs BAKE button.


// Supports boost/MODE button, temperature pot, and a single HEATCALL LED.
// This does not support LEARN buttons; a derived class does.
class ModeButtonAndPotActuatorPhysicalUI : public ActuatorPhysicalUIBase
  {
  protected:
    // If true, implements older MODE behaviour hold to cycle through FROST/WARM/BAKE.
    // If false, button is press to BAKE, and should be interrupt-driven.
    const bool cycleMODE = false;

    // Record local manual operation of a physical UI control, eg not remote or via CLI.
    // Marks room as occupied amongst other things.
    // Thread-safe.
    void markUIControlUsed();

    // Record significant local manual operation of a physical UI control, eg not remote or via CLI.
    // Marks room as occupied amongst other things.
    // As markUIControlUsed() but likely to generate some feedback to the user, ASAP.
    // Thread-safe.
    void markUIControlUsedSignificant();

    // UI feedback.
    // Provide low-key visual / audio / tactile feedback on a significant user action.
    // May take hundreds of milliseconds and noticeable energy.
    // By default includes visual feedback,
    // but that can be prevented if other visual feedback already in progress.
    // Marks the UI as used.
    // Not thread-/ISR- safe.
    void userOpFeedback(bool includeVisual = true);

    // If non-NULL, callback used to provide additional feedback to the user beyond UI.
    // For example, can cause the motor to wiggle for tactile reinforcement.
    const void (*userAdditionalFeedback)() = NULL; // FIXME

    // Occupancy callback function (for good confidence of human presence); NULL if not used.
    // Also indicates that the manual UI has been used.
    // If not NULL, is called when this sensor detects indications of occupancy.
    void (*occCallback)() = NULL; // FIXME

    // WARM/FROST and BAKE start/cancel callbacks.
    // If not NULL, are called when the pot is adjusted appropriately.
    // Typically at most one of these callbacks would be made on any appropriate pot adjustment.
    void (*warmModeCallback)(bool) = NULL; // FIXME
    void (*bakeStartCallback)(bool) = NULL; // FIXME

  public:
    // True if a manual UI control has been very recently (minutes ago) operated.
    // The user may still be interacting with the control and the UI etc should probably be extra responsive.
    // Thread-safe.
    bool veryRecentUIControlUse();

    // True if a manual UI control has been recently (tens of minutes ago) operated.
    // If true then local manual settings should 'win' in any conflict with programmed or remote ones.
    // For example, remote requests to override settings may be ignored while this is true.
    // Thread-safe.
    bool recentUIControlUse();

    // Does nothing and forces 'sensor' value to 0 and returns 0.
    virtual uint8_t read();

    // Handle simple interrupt for this UI sensor.
    // Should be wired to the MODE button, edge triggered.
    // By default does nothing (and returns false).
    //virtual bool handleInterruptSimple() { return(false); }

  };


// Supports two LEARN buttons, boost/MODE button, temperature pot, and a single HEATCALL LED.
class ModeAndLearnButtonsAndPotActuatorPhysicalUI : public ModeButtonAndPotActuatorPhysicalUI
  {
// TODO
  };

    }

#endif
