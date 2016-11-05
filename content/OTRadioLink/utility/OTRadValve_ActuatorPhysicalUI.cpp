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

#include <OTRadValve_ActuatorPhysicalUI.h>


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


#if defined(ModeButtonAndPotActuatorPhysicalUI_DEFINED)

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
    if(NULL != safeISRLEDonOpt) { safeISRLEDonOpt(); }
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
      OTV0P2BASE::safeDecIfNZWeak(uiTimeoutM);

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
        if(enhancedUIFeedback || forthTick || (valveMode->inWarmMode() && !occupancy->longLongVacant()))
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
          if(isLo) { valveMode->setWarmModeDebounced(false); }
          // Feed back significant change in pot position, ie at temperature boundaries.
          // Synthesise a 'hot' target temperature that distinguishes the end stops...
          const uint8_t nominalWarmTarget = isLo ? 1 :
              (tempPotOpt->isAtHiEndStop() ? 99 :
              tempControl->getWARMTargetC());
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

    // Support cycling through modes polling the MODE button.
    // If MODE button is active skip normal LED UI activity.
    if(!pollMODEButton())
      {
      // Keep reporting UI status if the user has just touched the unit in some way or UI feedback is enhanced.
      const bool justTouched = statusChange || enhancedUIFeedback;

      // Capture battery state if available.
      const bool batteryLow = (NULL != lowBattOpt) && lowBattOpt->isSupplyVoltageLow();

      // Minimise LED on duration unless UI just touched, or if battery low (TODO-963).
      const bool minimiseOnTime = (!justTouched) || batteryLow;

      // Mode button not pressed: indicate current mode with flash(es); more flashes if actually calling for heat.
      // Force display while UI controls are being used, eg to indicate temp pot position.
      if(justTouched || valveMode->inWarmMode()) // Generate flash(es) if in WARM mode or fiddling with UI other than Mode button.
        {
        // DHD20131223: only flash if the room not known to be dark so as to save energy and avoid disturbing sleep, etc.
        // In this case force resample of light level frequently in case user turns light on eg to operate unit.
        // Do show LED flash if user has recently operated controls (other than mode button) manually.
        // Flash infrequently if no recently operated controls and not in BAKE mode and not actually calling for heat;
        // this is to conserve batteries for those people who leave the valves in WARM mode all the time.
        if(justTouched ||
           ((forthTick
               || valveController->isCallingForHeat()
               || valveMode->inBakeMode()) && !ambLight->isRoomDark()))
          {
          // First flash to indicate WARM mode (or pot being twiddled).
          LEDon();
          // LED on stepwise proportional to temp pot setting.
          // Small number of steps (3) should help make positioning more obvious.
          const uint8_t wt = tempControl->getWARMTargetC();
          // Makes vtiny|tiny|medium flash for cool|OK|warm temperature target.
          // Stick to minimum length flashes to save energy unless just touched.
          if(minimiseOnTime || tempControl->isEcoTemperature(wt)) { veryTinyPause(); }
          else if(!tempControl->isComfortTemperature(wt)) { tinyPause(); }
          else { mediumPause(); }

          // Second flash to indicate actually calling for heat,
          // or likely to be calling for heat while interacting with the controls, to give fast user feedback (TODO-695).
          if((enhancedUIFeedback && valveController->isUnderTarget()) ||
              valveController->isCallingForHeat() ||
              valveMode->inBakeMode())
            {
            LEDoff();
            offPause(); // V0.09 was mediumPause().
            LEDon(); // flash
            // Stick to minimum length flashes to save energy unless just touched.
            if(minimiseOnTime || tempControl->isEcoTemperature(wt)) { veryTinyPause(); }
            else if(!tempControl->isComfortTemperature(wt)) { veryTinyPause(); veryTinyPause(); }
            else { tinyPause(); }

            if(valveMode->inBakeMode())
              {
              // Third (lengthened) flash to indicate BAKE mode.
              LEDoff();
              mediumPause(); // Note different flash off time to try to distinguish this last flash.
              LEDon();
              // Makes tiny|small|medium flash for eco|OK|comfort temperature target.
              // Stick to minimum length flashes to save energy unless just touched.
              if(minimiseOnTime || tempControl->isEcoTemperature(wt)) { veryTinyPause(); }
              else if(!tempControl->isComfortTemperature(wt)) { smallPause(); }
              else { mediumPause(); }
              }
            }
          }
        }

      // Even in FROST mode, and if actually calling for heat (eg opening the rad valve significantly, etc)
      // then emit a tiny double flash on every 4th tick.
      // This call for heat may be frost protection or pre-warming / anticipating demand.
      // DHD20130528: new 4th-tick flash in FROST mode...
      // DHD20131223: only flash if the room is not dark (ie or if no working light sensor) so as to save energy and avoid disturbing sleep, etc.
      else if(forthTick &&
              !ambLight->isRoomDark() &&
              valveController->isCallingForHeat() /* &&
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
      }

    // Ensure that the main UI LED is forced off unconditionally at least once each cycle.
    LEDoff();

    // Handle LEARN buttons (etc) in derived classes.
    // Also can be used to handle any simple user schedules.
    handleOtherUserControls(statusChange);

    const bool statusChanged = statusChange;
    statusChange = false; // Potential race.
    return(statusChanged);
    }

#endif // ModeButtonAndPotActuatorPhysicalUI_DEFINED


    }
