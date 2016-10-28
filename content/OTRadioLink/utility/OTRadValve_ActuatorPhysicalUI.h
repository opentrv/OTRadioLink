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
  public:
    // Use WDT-based timer for xxxPause() routines.
    // Very tiny low-power sleep.
    static const uint8_t VERYTINY_PAUSE_MS = 5;
    static void inline veryTinyPause() { OTV0P2BASE::sleepLowPowerMs(VERYTINY_PAUSE_MS); }
    // Tiny low-power sleep to approximately match the PICAXE V0.09 routine of the same name.
    static const uint8_t TINY_PAUSE_MS = 15;
    static void inline tinyPause() { OTV0P2BASE::nap(WDTO_15MS); } // 15ms vs 18ms nominal for PICAXE V0.09 impl.
    // Small low-power sleep.
    static const uint8_t SMALL_PAUSE_MS = 30;
    static void inline smallPause() { OTV0P2BASE::nap(WDTO_30MS); }
    // Medium low-power sleep to approximately match the PICAXE V0.09 routine of the same name.
    // Premature wakeups MAY be allowed to avoid blocking I/O polling for too long.
    static const uint8_t MEDIUM_PAUSE_MS = 60;
    static void inline mediumPause() { OTV0P2BASE::nap(WDTO_60MS); } // 60ms vs 144ms nominal for PICAXE V0.09 impl.
    // Big low-power sleep to approximately match the PICAXE V0.09 routine of the same name.
    // Premature wakeups MAY be allowed to avoid blocking I/O polling for too long.
    static const uint8_t BIG_PAUSE_MS = 120;
    static void inline bigPause() { OTV0P2BASE::nap(WDTO_120MS); } // 120ms vs 288ms nominal for PICAXE V0.09 impl.

    // Pause between flashes to allow them to be distinguished (>100ms); was mediumPause() for PICAXE V0.09 impl.
    static void inline offPause()
      {
      bigPause(); // 120ms, was V0.09 144ms mediumPause() for PICAXE V0.09 impl.
      //  pollIO(); // Slip in an I/O poll.
      }

  protected:
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

    // UI feedback.
    // Provide low-key visual / audio / tactile feedback on a significant user action.
    // May take hundreds of milliseconds and noticeable energy.
    // By default includes visual feedback,
    // but that can be prevented if other visual feedback already in progress.
    // Marks the UI as used.
    // Not thread-/ISR- safe.
    void userOpFeedback(const bool includeVisual = true)
      {
      if(includeVisual) { LEDon(); }
      markUIControlUsed();
      // Sound and tactile feedback with local valve, like mobile phone vibrate mode.
      // Only do this if in a normal state, eg not calibrating nor in error.
      if(valveController->isInNormalRunState()) { valveController->wiggle(); }
      // Were wiggle cannot be used pause briefly to let LED on be seen.
      else { if(includeVisual) { smallPause(); } }
      if(includeVisual) { LEDoff(); }
      // Note that feedback for significant UI action has been given.
      significantUIOp = false;
      }

    // Valve mode; must not be NULL.
    ValveMode *const valveMode;

    // Temperature control for set-points; must not be NULL.
    const TempControlBase *const tempControl;

    // Read-only access to valve controller state.
    const AbstractRadValve *const valveController;

    // Occupancy tracker; must not be NULL.
    OTV0P2BASE::PseudoSensorOccupancyTracker *const occupancy;

    // Read-only access to ambient light sensor; must not be NULL
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

    // Poll the MODE button to support cycling through modes with the button held down; true if button active.
    // If this returns true then avoid other LED UI output.
    // This was the older style of interface (eg for REV1/REV2).
    // The newer interface only uses the MODE button to trigger bake, interrupt-driven.
    // This is called after polling the temp pot if present,
    // and after providing feedback for any significant accrued UI interactions.
    // By default does nothing and returns false.
    virtual bool pollMODEButton() { return(false); }

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
      void (*const _LEDon)(), void (*const _LEDoff)(), void (*const _safeISRLEDonOpt)())
      : valveMode(_valveMode), tempControl(_tempControl), valveController(_valveController),
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

    // Start/cancel WARM mode in one call, driven by manual UI input.
    void setWarmModeFromManualUI(const bool warm)
      {
      // Give feedback when changing WARM mode.
      if(warm != valveMode->inWarmMode()) { this->markUIControlUsedSignificant(); }
      // Now set/cancel WARM.
      valveMode->setWarmModeDebounced(warm);
      }

    // Start/cancel BAKE mode in one call, driven by manual UI input.
    void setBakeModeFromManualUI(const bool start)
      {
      // Give feedback when changing BAKE mode.
      if(valveMode->inBakeMode() != start) { this->markUIControlUsedSignificant(); }
      // Now set/cancel BAKE.
      if(start) { valveMode->startBake(); } else { valveMode->cancelBakeDebounced(); }
      }

    // Handle simple interrupt for from MODE button, edge triggered on button push.
    // Starts BAKE from manual UI interrupt; marks UI as used also.
    // Vetoes switch to BAKE mode if a temp pot/dial is present and at the low end stop, ie in the FROST position.
    // Marked inline to try encourage the compiler to produce the best possible code.
    // Calling this from the ISR entry-point is likely much faster than calling handleInterruptSimple().
    // ISR-safe.
    inline void startBakeFromInt()
      {
      if(NULL != tempPotOpt)
        {
        const bool isLo = tempPotOpt->isAtLoEndStop(); // ISR-safe.
        if(isLo) { markUIControlUsed(); return; }
        }
      valveMode->startBake();
      markUIControlUsedSignificant();
      }

    // Handle simple interrupt for from MODE button, edge triggered on button push.
    // Starts BAKE from manual UI interrupt if not in cycle mode; marks UI as used in any case.
    // ISR-safe.
    virtual bool handleInterruptSimple() override { startBakeFromInt(); return(true); }
  };


// Supports two LEARN buttons, boost/MODE button, temperature pot, and a single HEATCALL LED.
// Uses the MODE button to cycle though modes as per older (and pot-less) UI such as REV1.
//
// Implementation of minimal UI using single LED and one or two momentary push-buttons.
//
// ; UI DESCRIPTION (derived from V0.09 PICAXE code)
// ; Button causes cycling through 'off'/'frost' target of 5C, 'warm' target of ~18C,
// ; and an optional 'bake' mode that raises the target temperature to up to ~24C
// ; for up to ~30 minutes or until the target is hit then reverts to 'warm' automatically.
// ; (Button may have to be held down for up to a few seconds to get the unit's attention.)
// ; As of 2013/12/15 acknowledgment single/double/triple flash in new mode.
// ;; Up to 2013/12/14, acknowledgment was medium/long/double flash in new mode
// ;; (medium is frost, long is 'warm', long + second flash is 'bake').
//
// ; Without the button pressed,
// ; the unit generates one to three short flashes on a two-second cycle if in heat mode.
// ; A first flash indicates "warm mode".  (V0.2: every 4th set of flashes will be dim or omitted if a schedule is set.)
// ; A second flash if present indicates "calling for heat".
// ; A third flash if present indicates "bake mode" (which is automatically cancelled after a short time, or if the high target is hit).
//
// ; This may optionally support an interactive CLI over the serial connection,
// ; with reprogramming initiation permitted (instead of CLI) while the UI button is held down.
//
// ; If target is not being met then aim to turn TRV on/up and call for heat from the boiler too,
// ; else if target is being met then turn TRV off/down and stop calling for heat from the boiler.
// ; Has a small amount of hysteresis to reduce short-cycling of the boiler.
// ; Does some proportional TRV control as target temperature is neared to reduce overshoot.
//
// ; This can use a simple setback (drops the 'warm' target a little to save energy)
// ; eg using an LDR, ie reasonable ambient light, as a proxy for occupancy.
//
#define CycleModeAndLearnButtonsAndPotActuatorPhysicalUI_DEFINED
template <int BUTTON_MODE_L_pin>
class CycleModeAndLearnButtonsAndPotActuatorPhysicalUI : public ModeButtonAndPotActuatorPhysicalUI
  {
  private:
    // If true then is in WARM (or BAKE) mode; defaults to (starts as) false/FROST.
    // Should be only be set when 'debounced'.
    // Defaults to (starts as) false/FROST.
    bool isWarmModePutative;
    bool isBakeModePutative;
    bool modeButtonWasPressed;

  protected:
    // Poll the MODE button to support cycling through modes with the button held down; true if button active.
    // If this returns true then avoid other LED UI output.
    // This was the older style of interface (eg for REV1/REV2).
    // The newer interface only uses the MODE button to trigger bake, interrupt-driven.
    // This is called after polling the temp pot if present,
    // and after providing feedback for any significant accrued UI interactions.
    virtual bool pollMODEButton() override
      {
      // Full MODE button behaviour:
      //   * cycle through FROST/WARM/BAKE while held down showing 1/2/3 flashes as appropriate
      //   * switch to selected mode on button release
      const bool modeButtonIsPressed = (LOW == fastDigitalRead(BUTTON_MODE_L_pin));
      if(modeButtonIsPressed)
        {
        if(!modeButtonWasPressed)
          {
          // Capture real mode variable as button is pressed.
          isWarmModePutative = valveMode->inWarmMode();
          isBakeModePutative = valveMode->inBakeMode();
          modeButtonWasPressed = true;
          }

        // User is pressing the mode button: cycle through FROST | WARM [ | BAKE ].
        // Mark controls used and room as currently occupied given button press.
        markUIControlUsed();
        // LED on...
        this->LEDon();
        tinyPause(); // Leading tiny pause...
        if(!isWarmModePutative) // Was in FROST mode; moving to WARM mode.
          {
          isWarmModePutative = true;
          isBakeModePutative = false;
          // 2 x flash 'heat call' to indicate now in WARM mode.
          this->LEDoff();
          offPause();
          this->LEDon();
          tinyPause();
          }
        else if(!isBakeModePutative) // Was in WARM mode, move to BAKE (with full timeout to run).
          {
          isBakeModePutative = true;
          // 2 x flash + one longer flash 'heat call' to indicate now in BAKE mode.
          this->LEDoff();
          offPause();
          this->LEDon();
          tinyPause();
          this->LEDoff();
          mediumPause(); // Note different flash on/off duty cycle to try to distinguish this last flash.
          this->LEDon();
          mediumPause();
          }
        else // Was in BAKE (if supported, else was in WARM), move to FROST.
          {
          isWarmModePutative = false;
          isBakeModePutative = false;
          // 1 x flash 'heat call' to indicate now in FROST mode.
          }
        }
      else
        {
        // Update real control variables for mode when the button is released.
        if(modeButtonWasPressed)
          {
          // Don't update the debounced WARM mode while button held down.
          // Will also capture programmatic changes to isWarmMode, eg from schedules.
          const bool isWarmModeDebounced = isWarmModePutative;
          valveMode->setWarmModeDebounced(isWarmModeDebounced);
          if(isBakeModePutative) { valveMode->startBake(); } else { valveMode->cancelBakeDebounced(); }
          this->markUIControlUsed(); // Note activity on release of MODE button...
          modeButtonWasPressed = false;
          return(false);
          }
        }
      return(modeButtonIsPressed);
      }

    // Called after handling main controls to handle other buttons and user controls.
    // This is also a suitable place to handle any simple schedules.
    // Designed to be overridden by derived classes, eg to handle LEARN buttons.
    // The UI LED is (possibly JUST) off by the time this is called.
    // By default does nothing.
    virtual void handleOtherUserControls() override
      {
      // FIXME
      //      // Enforce any changes that may have been driven by other UI components (ie other than MODE button).
      //      // Eg adjustment of temp pot / eco bias changing scheduled state.
      //      if(statusChange)
      //        {
      //        static bool prevScheduleStatus;
      //        const bool currentScheduleStatus = Scheduler.isAnyScheduleOnWARMNow();
      //        if(currentScheduleStatus != prevScheduleStatus)
      //          {
      //          prevScheduleStatus = currentScheduleStatus;
      //          setWarmModeDebounced(currentScheduleStatus);
      //          }
      //        }

      // FIXME
      //  #ifdef ENABLE_LEARN_BUTTON
      //    // Handle learn button if supported and if is currently pressed.
      //    if(fastDigitalRead(BUTTON_LEARN_L) == LOW)
      //      {
      //      handleLEARN(0);
      //      userOpFeedback(false); // Mark controls used and room as currently occupied given button press.
      //      LEDon(); // Leave heatcall LED on while learn button held down.
      //      }
      //
      //  #if defined(BUTTON_LEARN2_L)
      //    // Handle second learn button if supported and currently pressed and primary learn button not pressed.
      //    else if(fastDigitalRead(BUTTON_LEARN2_L) == LOW)
      //      {
      //      handleLEARN(1);
      //      userOpFeedback(false); // Mark controls used and room as currently occupied given button press.
      //      LEDon(); // Leave heatcall LED on while learn button held down.
      //      }
      //  #endif
      //  #endif
      }

  public:
    // Inherit the base class constructor.
    using ModeButtonAndPotActuatorPhysicalUI::ModeButtonAndPotActuatorPhysicalUI;

    // Handle simple interrupt for from MODE button, edge triggered on button push.
    // Marks UI as used.
    // ISR-safe.
    virtual bool handleInterruptSimple() override final { markUIControlUsed();  return(true); }

    //// Handle learn button(s).
    //// First/primary button is 0, second is 1, etc.
    //// In simple mode: if in frost mode clear simple schedule else set repeat for every 24h from now.
    //// May be called from pushbutton or CLI UI components.
    //static void handleLEARN(const uint8_t which)
    //  {
    //  // Set simple schedule starting every 24h from a little before now and running for an hour or so.
    //  if(valveMode.inWarmMode()) { Scheduler.setSimpleSchedule(OTV0P2BASE::getMinutesSinceMidnightLT(), which); }
    //  // Clear simple schedule.
    //  else { Scheduler.clearSimpleSchedule(which); }
    //  }
  };

#endif // ARDUINO_ARCH_AVR


    }

#endif
