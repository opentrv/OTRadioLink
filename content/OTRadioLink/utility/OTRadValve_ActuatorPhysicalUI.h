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
#include "OTV0P2BASE_SensorOccupancy.h"
#include "OTRadValve_ValveMode.h"
#include "OTRadValve_TempControl.h"
#include "OTRadValve_AbstractRadValve.h"

// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


// Base class for physical UI controls on V0p2 valve devices.
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
    virtual uint8_t read() = 0;

    // Preferred poll interval (in seconds); should be called at constant rate, usually 1/2s.
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
    const bool cycleMODE;

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
    volatile bool significantUIOp;

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
    void userOpFeedback(bool includeVisual = true) { } // FIXME

    // Valve mode; must not be NULL.
    ValveMode *const valveMode;

    // Temperature control for set-points; must not be NULL.
    const TempControlBase *const tempControl;

    // Read-only access to valve controller state.
    const AbstractRadValve *const valveController;

    // Occupancy tracker; must not be NULL.
    OTV0P2BASE::PseudoSensorOccupancyTracker *const occupancy;

    // Read-only acces to ambient light sensor; must not be NULL
    const OTV0P2BASE::SensorAmbientLight *const ambLight;

    // Temperature pot; may be NULL.
    // May have read() called to poll pot status and provoke occupancy callbacks.
    OTV0P2BASE::SensorTemperaturePot *const tempPotOpt;

    // If non-NULL, callback used to provide additional feedback to the user beyond UI.
    // For example, can cause the motor to wiggle for tactile reinforcement.
    void (*const userAdditionalFeedback)() = NULL; // FIXME

    // Callback used to provide UI-LED-on output, may not be thread-safe; never NULL.
    // Could be set to LED_HEATCALL_ON() or similar.
    void (*const LEDon)();
    // Callback used to provide UI-LED-off output, may not be thread-safe; never NULL.
    // Could be set to LED_HEATCALL_OFF() or similar.
    void (*const LEDoff)();
    // Callback used to provide ISR-safe instant UI-LED-on response; may be NULL if so such callback available.
    // Could be set to LED_HEATCALL_ON_ISR_SAFE() or similar.
    void (*const safeISRLEDonOpt)();

    // Called after handling main controls to handle other buttons and user controls.
    // Designed to be overridden by derived classes, eg to handle LEARN buttons.
    // Is passed 'true' if any status change has happened so far
    // to enforce any changes that may have been driven by other UI components (ie other than MODE button).
    // Eg adjustment of temp pot / eco bias changing scheduled state.
    // By default does nothing.
    virtual void handleOtherUserControls(bool /*statusChangeSoFar*/) { }

    // Called after handling main controls to handle other buttons and user controls.
    // This is also a suitable place to handle any simple schedules.
    // Designed to be overridden by derived classes, eg to handle LEARN buttons.
    // The UI LED is (possibly JUST) off by the time this is called.
    // By default does nothing.
    virtual void handleOtherUserControls() { }

    // Counts calls to read() (was tickUI()).
    uint8_t tickCount = 0;

  public:
    // Construct a default instance.
    // Most arguments must not be NULL.
    ModeButtonAndPotActuatorPhysicalUI(
      ValveMode *const _valveMode,
      const TempControlBase *const _tempControl,
      const AbstractRadValve *const _valveController,
      OTV0P2BASE::PseudoSensorOccupancyTracker *const _occupancy,
      const OTV0P2BASE::SensorAmbientLight *const _ambLight,
      OTV0P2BASE::SensorTemperaturePot *const _tempPotOpt,
      void (*const _LEDon)(), void (*const _LEDoff)(), void (*const _safeISRLEDonOpt)(),
      bool _cycleMODE = false)
      : cycleMODE(_cycleMODE),
        valveMode(_valveMode), tempControl(_tempControl), valveController(_valveController),
        occupancy(_occupancy), ambLight(_ambLight), tempPotOpt(_tempPotOpt),
        LEDon(_LEDon), LEDoff(_LEDoff), safeISRLEDonOpt(_safeISRLEDonOpt)
      {
//      // Abort constructor if any bad args...
//      if((NULL == _valveMode) ||
//         (NULL == _valveController) ||
//         (NULL == _occupancy) ||
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
    virtual uint8_t read() override;

    // Handle simple interrupt for from MODE button, edge triggered on button push.
    // Starts BAKE from manual UI interrupt; marks UI as used also.
    // Vetos switch to BAKE mode if a temp pot/dial is present and at the low end stop, ie in the FROST position.
    // Was startBakeFromInt().
    // Marked final to help the compiler optimise this time-critical routine.
    virtual bool handleInterruptSimple() override final
      {
      if(NULL != tempPotOpt)
        {
        const bool isLo = tempPotOpt->isAtLoEndStop(); // ISR-safe.
        if(isLo) { markUIControlUsed(); return(true); }
        }
      valveMode->startBake();
      markUIControlUsedSignificant();
      }
  };


// Supports two LEARN buttons, boost/MODE button, temperature pot, and a single HEATCALL LED.
class ModeAndLearnButtonsAndPotActuatorPhysicalUI : public ModeButtonAndPotActuatorPhysicalUI
  {
// TODO
  };

#endif // ARDUINO_ARCH_AVR


    }

#endif
