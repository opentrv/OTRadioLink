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

#include "OTV0P2BASE_SensorAmbientLight.h"
#include "OTV0P2BASE_SensorTemperaturePot.h"


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
    // Replaces: bool tickUI(uint_fast8_t sec).
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


#ifdef ARDUINO_ARCH_AVR

// Supports boost/MODE button, temperature pot, and a single HEATCALL LED.
// This does not support LEARN buttons; a derived class does.
#define ModeButtonAndPotActuatorPhysicalUI_DEFINED
class ModeButtonAndPotActuatorPhysicalUI : public ActuatorPhysicalUIBase
  {
  protected:
    // If true, implements older MODE behaviour hold to cycle through FROST/WARM/BAKE
    // as selected by ENABLE_SIMPLIFIED_MODE_BAKE.
    // If false, button is press to BAKE, and should be interrupt-driven.
    const bool cycleMODE = false;

    // Marked true if the physical UI controls are being used.
    // Cleared at end of read().
    // Marked volatile for thread-safe lock-free non-read-modify-write access to byte-wide value.
    volatile bool statusChange;

    // Minutes that freshly-touched controls are regarded as 'recently' used.
    static const uint8_t UI_DEFAULT_RECENT_USE_TIMEOUT_M = 31;
    // Minutes that freshly-touched controls are regarded as 'very recently' used.
    static const uint8_t UI_DEFAULT_VERY_RECENT_USE_TIMEOUT_M = 2;
    // If non-zero then UI controls have been recently manually/locally operated; counts down to zero.
    // Marked volatile for thread-safe lock-free non-read-modify-write access to byte-wide value.
    // Compound operations on this value must block interrupts.
    volatile uint8_t uiTimeoutM;

    // Set true on significant local UI operation.
    // Should be cleared when feedback has been given.
    // Marked volatile for thread-safe lockless access;
    static volatile bool significantUIOp;

    // Record local manual operation of a physical UI control, eg not remote or via CLI.
    // Marks room as occupied amongst other things.
    // To be thread-/ISR- safe, everything that this touches or calls must be.
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

    // Occupancy tracker; must not be NULL.
    PseudoSensorOccupancyTracker *const occupancy;

    // Ambient light sensor; must not be NULL
    const SensorAmbientLight *const ambLight;

    // Temperature pot; may be NULL.
    // May have read() called to poll pot status and provoke occupancy callbacks.
    SensorTemperaturePot *const tempPotOpt;

    // If non-NULL, callback used to provide additional feedback to the user beyond UI.
    // For example, can cause the motor to wiggle for tactile reinforcement.
    void (*const userAdditionalFeedback)() = NULL; // FIXME

    // Callback used to provide UI-LED-on output, may not be thread-safe; never NULL.
    // Could be set to LED_HEATCALL_ON() or similar.
    void (*const LEDon)();
    // Callback used to provide UI-LED-off output, may not be thread-safe; never NULL.
    // Could be set to LED_HEATCALL_OFF() or similar.
    void (*const LEDoff)();
    // If non-NULL, callback used to provide ISR-safe instant UI-LED-on response.
    // Could be set to LED_HEATCALL_ON_ISR_SAFE() or similar.
    void (*const safeISRLEDon)();


    // WARM/FROST and BAKE start/cancel callbacks.
    // If not NULL, are called when the pot is adjusted appropriately.
    // Typically at most one of these callbacks would be made on any appropriate pot adjustment.
    void (*warmModeCallback)(bool) = NULL; // FIXME
    void (*bakeStartCallback)(bool) = NULL; // FIXME

    // Called after handling main controls to handle other buttons and user controls.
    // Designed to be overridden by derived classes, eg to handle LEARN buttons.
    // By default does nothing.
    virtual void handleOtherUserControls() { }

    // Counts calls to read() (was tickUI()).
    uint8_t tickCount = 0;

  public:
    // Construct a default instance.
    ModeButtonAndPotActuatorPhysicalUI(
      PseudoSensorOccupancyTracker *const _occupancy,
      const SensorAmbientLight *const _ambLight,
      SensorTemperaturePot *const _tempPotOpt,
      void (*const _LEDon)(), void (*const _LEDoff)(), void (*const _safeISRLEDon)(),
        bool _cycleMODE = false)
      : cycleMODE(_cycleMODE),
        occupancy(_occupancy), ambLight(_ambLight), tempPotOpt(_tempPotOpt),
        LEDon(_LEDon),  LEDoff(_LEDoff),  safeISRLEDon(_safeISRLEDon)
      {
//      // Abort constructor if any bad args...
//      if((NULL == _occupancy) ||
//         (NULL == _ambLight) ||
//         (NULL == _LEDon) ||
//         (NULL == _LEDoff)) { panic(); }
      }

    // True if a manual UI control has been very recently (minutes ago) operated.
    // The user may still be interacting with the control and the UI etc should probably be extra responsive.
    // Thread-safe.
    bool veryRecentUIControlUse() { return(uiTimeoutM >= (UI_DEFAULT_RECENT_USE_TIMEOUT_M - UI_DEFAULT_VERY_RECENT_USE_TIMEOUT_M)); }

    // True if a manual UI control has been recently (tens of minutes ago) operated.
    // If true then local manual settings should 'win' in any conflict with programmed or remote ones.
    // For example, remote requests to override settings may be ignored while this is true.
    // Thread-safe.
    bool recentUIControlUse() { return(0 != uiTimeoutM); }

    // Call this nominally on even numbered seconds to allow the UI to operate.
    // In practice call early once per 2s major cycle.
    // Should never be skipped, so as to allow the UI to remain responsive.
    // Runs in 350ms or less; usually takes only a few milliseconds or microseconds.
    // Returns a non-zero value iff the user interacted with the system, and maybe caused a status change.
    // NOTE: since this is on the minimum idle-loop code path, minimise CPU cycles, esp in frost mode.
    // Replaces: bool tickUI(uint_fast8_t sec).
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

#endif // ARDUINO_ARCH_AVR


    }

#endif
