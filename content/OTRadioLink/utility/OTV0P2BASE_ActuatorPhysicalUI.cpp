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
 * OpenTRV radiator valve physical UI controls and output as an actuator.
 *
 * A base class, a null class, and one or more implementations are provided
 * for different stock behaviour with different hardware.
 *
 * A mixture of template and constructor parameters
 * is used to configure the different types.
 */

#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include <OTV0P2BASE_Actuator.h>

#include "OTV0P2BASE_ActuatorPhysicalUI.h"


// Use namespaces to help avoid collisions.
namespace OTV0P2BASE
    {


#if defined(ModeButtonAndPotActuatorPhysicalUI_DEFINED) && 0
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

// Record local manual operation of a physical UI control, eg not remote or via CLI.
// Marks room as occupied amongst other things.
// To be thread-/ISR- safe, everything that this touches or calls must be.
// Thread-safe.
void ModeButtonAndPotActuatorPhysicalUI::markUIControlUsed()
    {
    statusChange = true; // Note user interaction with the system.
    uiTimeoutM = UI_DEFAULT_RECENT_USE_TIMEOUT_M; // Ensure that UI controls are kept 'warm' for a little while.
// FIXME
//  #if defined(ENABLE_UI_WAKES_CLI)
//    // Make CLI active for a while (at some slight possibly-significant energy cost).
//    resetCLIActiveTimer(); // Thread-safe.
//  #endif
    // User operation of physical controls is strong indication of presence.
    occupancy->markAsOccupied(); // Thread-safe.
    }

// Record significant local manual operation of a physical UI control, eg not remote or via CLI.
// Marks room as occupied amongst other things.
// As markUIControlUsed() but likely to generate some feedback to the user, ASAP.
// Thread-safe.
void ModeButtonAndPotActuatorPhysicalUI::markUIControlUsedSignificant()
    {
    // Provide some instant visual feedback if possible.
    //  LED_HEATCALL_ON_ISR_SAFE();
    void (*safeISRLEDon_FN)() = safeISRLEDon;
    if(NULL != safeISRLEDon_FN) { safeISRLEDon_FN(); }
    // Flag up need for feedback.
    significantUIOp = true;
    // Do main UI-touched work.
    markUIControlUsed();
    }

// Call this nominally on even numbered seconds to allow the UI to operate.
// In practice call early once per 2s major cycle.
// Should never be skipped, so as to allow the UI to remain responsive.
// Runs in 350ms or less; usually takes only a few milliseconds or microseconds.
// Returns a non-zero value iff the user interacted with the system, and maybe caused a status change.
// NOTE: since this is on the minimum idle-loop code path, minimise CPU cycles, esp in frost mode.
// Replaces: bool tickUI(uint_fast8_t sec).
uint8_t ModeButtonAndPotActuatorPhysicalUI::read()
    {
    // True on every 4th tick/call, ie every ~8 seconds.
    const bool forthTick = !((++tickCount) & 3); // True on every 4th tick.

    // Perform any once-per-minute-ish operations, every 32 ticks.
    const bool sec0 = (0 == (tickCount & 0x1f));
    if(sec0)
      {
      ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
        // Run down UI interaction timer if need be, one tick per minute(ish).
        if(uiTimeoutM > 0) { --uiTimeoutM; }
        }
      }

//    const bool reportedRecently = occupancy->reportedRecently();

    // Provide enhanced feedback when the has been very recent interaction with the UI,
    // since the user is still quite likely to be continuing.
    const bool enhancedUIFeedback = veryRecentUIControlUse();

    if(NULL != tempPotOpt)
        {
        // Force relatively-frequent re-read of temp pot UI device
        // and if there has been recent UI manual controls activity,
        // to keep the valve UI responsive.
    //  #if !defined(ENABLE_FAST_TEMP_POT_SAMPLING) || !defined(ENABLE_OCCUPANCY_SUPPORT)
    //    if(enhancedUIFeedback || forthTick)
    //  #else
    //    // Even more responsive at some possible energy cost...
        // Polls pot on every tick unless the room has been vacant for a day or two or is in FROST mode.
        if(enhancedUIFeedback || forthTick || (inWarmMode() && !occupancy->longLongVacant()))
    //  #endif
          {
          tempPotOpt->read();
      #if 0 && defined(DEBUG)
            DEBUG_SERIAL_PRINT_FLASHSTRING("TP");
            DEBUG_SERIAL_PRINT(TempPot.getRaw());
            DEBUG_SERIAL_PRINTLN();
      #endif
          // Force to FROST mode (and cancel any erroneous BAKE, etc) when at FROST end of dial.
          const bool isLo = tempPotOpt->isAtLoEndStop();
          if(isLo) { setWarmModeDebounced(false); }
          // Feed back significant change in pot position, ie at temperature boundaries.
          // Synthesise a 'warm' target temp that distinguishes end stops...
          const uint8_t nominalWarmTarget = isLo ? 1 :
              (tempPotOpt->isAtHiEndStop() ? 99 :
              getWARMTargetC());
          // Record of 'last' nominalWarmTarget; initially 0.
          static uint8_t lastNominalWarmTarget;
          if(nominalWarmTarget != lastNominalWarmTarget)
            {
            // Note if a boundary was crossed, ignoring any false 'start-up' transient.
            if(0 != lastNominalWarmTarget) { significantUIOp = true; }
      #if 1 && defined(DEBUG)
            DEBUG_SERIAL_PRINT_FLASHSTRING("WT");
            DEBUG_SERIAL_PRINT(nominalWarmTarget);
            DEBUG_SERIAL_PRINTLN();
      #endif
            lastNominalWarmTarget = nominalWarmTarget;
            }
          }
      }

    // Provide extra user feedback for significant UI actions...
    if(significantUIOp) { userOpFeedback(); }

//  #if !defined(ENABLE_SIMPLIFIED_MODE_BAKE) // if(cycleMODE)
//    // Full MODE button behaviour:
//    //   * cycle through FROST/WARM/BAKE while held down
//    //   * switch to selected mode on release
//    //
//    // If true then is in WARM (or BAKE) mode; defaults to (starts as) false/FROST.
//    // Should be only be set when 'debounced'.
//    // Defaults to (starts as) false/FROST.
//    static bool isWarmModePutative;
//    static bool isBakeModePutative;
//
//    static bool modeButtonWasPressed;
//    if(fastDigitalRead(BUTTON_MODE_L) == LOW)
//      {
//      if(!modeButtonWasPressed)
//        {
//        // Capture real mode variable as button is pressed.
//        isWarmModePutative = inWarmMode();
//        isBakeModePutative = inBakeMode();
//        modeButtonWasPressed = true;
//        }
//
//      // User is pressing the mode button: cycle through FROST | WARM [ | BAKE ].
//      // Mark controls used and room as currently occupied given button press.
//      markUIControlUsed();
//      // LED on...
//      LEDon();
//      tinyPause(); // Leading tiny pause...
//      if(!isWarmModePutative) // Was in FROST mode; moving to WARM mode.
//        {
//        isWarmModePutative = true;
//        isBakeModePutative = false;
//        // 2 x flash 'heat call' to indicate now in WARM mode.
//        LEDoff();
//        offPause();
//        LEDon();
//        tinyPause();
//        }
//      else if(!isBakeModePutative) // Was in WARM mode, move to BAKE (with full timeout to run).
//        {
//        isBakeModePutative = true;
//        // 2 x flash + one longer flash 'heat call' to indicate now in BAKE mode.
//        LEDoff();
//        offPause();
//        LEDon();
//        tinyPause();
//        LEDoff();
//        mediumPause(); // Note different flash on/off duty cycle to try to distinguish this last flash.
//        LEDon();
//        mediumPause();
//        }
//      else // Was in BAKE (if supported, else was in WARM), move to FROST.
//        {
//        isWarmModePutative = false;
//        isBakeModePutative = false;
//        // 1 x flash 'heat call' to indicate now in FROST mode.
//        }
//      }
//    else
//  #endif // !defined(ENABLE_SIMPLIFIED_MODE_BAKE)
      {
//  #if !defined(ENABLE_SIMPLIFIED_MODE_BAKE) // if(cycleMODE)
//      // Update real control variables for mode when button is released.
//      if(modeButtonWasPressed)
//        {
//        // Don't update the debounced WARM mode while button held down.
//        // Will also capture programmatic changes to isWarmMode, eg from schedules.
//        const bool isWarmModeDebounced = isWarmModePutative;
//        setWarmModeDebounced(isWarmModeDebounced);
//        if(isBakeModePutative) { startBake(); } else { cancelBakeDebounced(); }
//
//        markUIControlUsed(); // Note activity on release of MODE button...
//        modeButtonWasPressed = false;
//        }
//  #endif // !defined(ENABLE_SIMPLIFIED_MODE_BAKE)

      // Keep reporting UI status if the user has just touched the unit in some way or UI feedback is enhanced.
      const bool justTouched = statusChange || enhancedUIFeedback;

      // Mode button not pressed: indicate current mode with flash(es); more flashes if actually calling for heat.
      // Force display while UI controls are being used, eg to indicate temp pot position.
      if(justTouched || inWarmMode()) // Generate flash(es) if in WARM mode or fiddling with UI other than Mode button.
        {
        // DHD20131223: only flash if the room not known to be dark so as to save energy and avoid disturbing sleep, etc.
        // In this case force resample of light level frequently in case user turns light on eg to operate unit.
        // Do show LED flash if user has recently operated controls (other than mode button) manually.
        // Flash infrequently if no recently operated controls and not in BAKE mode and not actually calling for heat;
        // this is to conserve batteries for those people who leave the valves in WARM mode all the time.
        if(justTouched ||
           ((forthTick
  #if defined(ENABLE_NOMINAL_RAD_VALVE) && defined(ENABLE_LOCAL_TRV)
               || NominalRadValve.isCallingForHeat()
  #endif
               || inBakeMode()) && !ambLight->isRoomDark()))
          {
          // First flash to indicate WARM mode (or pot being twiddled).
          LEDon();
          // LED on stepwise proportional to temp pot setting.
          // Small number of steps (3) should help make positioning more obvious.
          const uint8_t wt = getWARMTargetC();
          // Makes vtiny|tiny|medium flash for cool|OK|warm temperature target.
          // Stick to minimum length flashes to save energy unless just touched.
          if(!justTouched || isEcoTemperature(wt)) { veryTinyPause(); }
          else if(!isComfortTemperature(wt)) { tinyPause(); }
          else { mediumPause(); }

  #if defined(ENABLE_NOMINAL_RAD_VALVE) && defined(ENABLE_LOCAL_TRV)
          // Second flash to indicate actually calling for heat,
          // or likely to be calling for heat while interacting with the controls, to give fast user feedback (TODO-695).
          if((enhancedUIFeedback && NominalRadValve.isUnderTarget()) ||
              NominalRadValve.isCallingForHeat() ||
              inBakeMode())
            {
            LEDoff();
            offPause(); // V0.09 was mediumPause().
            LEDon(); // flash
            // Stick to minimum length flashes to save energy unless just touched.
            if(!justTouched || isEcoTemperature(wt)) { veryTinyPause(); }
            else if(!isComfortTemperature(wt)) { OTV0P2BASE::sleepLowPowerMs((VERYTINY_PAUSE_MS + TINY_PAUSE_MS) / 2); }
            else { tinyPause(); }

            if(inBakeMode())
              {
              // Third (lengthened) flash to indicate BAKE mode.
              LEDoff();
              mediumPause(); // Note different flash off time to try to distinguish this last flash.
              LEDon();
              // Makes tiny|small|medium flash for eco|OK|comfort temperature target.
              // Stick to minimum length flashes to save energy unless just touched.
              if(!justTouched || isEcoTemperature(wt)) { veryTinyPause(); }
              else if(!isComfortTemperature(wt)) { smallPause(); }
              else { mediumPause(); }
              }
            }
  #endif
          }
        }

  #if defined(ENABLE_NOMINAL_RAD_VALVE) && defined(ENABLE_LOCAL_TRV)
      // Even in FROST mode, and if actually calling for heat (eg opening the rad valve significantly, etc)
      // then emit a tiny double flash on every 4th tick.
      // This call for heat may be frost protection or pre-warming / anticipating demand.
      // DHD20130528: new 4th-tick flash in FROST mode...
      // DHD20131223: only flash if the room is not dark (ie or if no working light sensor) so as to save energy and avoid disturbing sleep, etc.
      else if(forthTick &&
              !ambLight->isRoomDark() &&
              NominalRadValve.isCallingForHeat() /* &&
              NominalRadValve.isControlledValveReallyOpen() */ )
        {
        // Double flash every 4th tick indicates call for heat while in FROST MODE (matches call for heat in WARM mode).
        LEDon(); // flash
        veryTinyPause();
        LEDoff();
        offPause();
        LEDon(); // flash
        veryTinyPause();
        }
  #endif

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

      }

    // Ensure LED forced off unconditionally at least once each cycle.
    LEDoff();

    // Handle LEARN buttons (etc) in derived classes.
    handleOtherUserControls();
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

    const bool statusChanged = statusChange;
    statusChange = false; // Potential race.
    return(statusChanged);
    }

#endif // ModeButtonAndPotActuatorPhysicalUI_DEFINED


    }
