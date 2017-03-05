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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2017
*/

/*
 Simple valve schedule support for TRV.
 */

#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#endif

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "OTRadValve_SimpleValveSchedule.h"

#include "OTV0P2BASE_EEPROM.h"
#include "OTV0P2BASE_RTC.h"


namespace OTRadValve
{


// Check/apply the user's schedule.
// This should be called (at least) once each minute
// to apply any current schedule to the valve state,
// ie moving it between frost and warm modes.
//
// This will only move to frost mode when no current warm schedule
// is enabled, so overlapping schedules behave as expected.
//
//   * mm  minutes since midnight local time [0,1439]
void SimpleValveScheduleBase::applyUserSchedule(
        OTRadValve::ValveMode* const valveMode,
        const uint_least16_t mm) const
    {
    // Check all available schedules.
    const uint8_t maxSc = maxSchedules();
    for (uint8_t which = 0; which < maxSc; ++which)
        {
        // Check if now is the simple scheduled off time,
        // as minutes after midnight [0,1439];
        // invalid (eg ~0) if none set.
        // Programmed off/frost takes priority over on/warm if same
        // to bias towards energy-saving.
        // Note that in the presence of multiple overlapping schedules
        // only the last 'off' applies however,
        // ie when no schedule is 'on'.
        if ((!isAnyScheduleOnWARMNow(mm))
                && (mm == getSimpleScheduleOff(which)))
            {
            valveMode->setWarmModeDebounced(false);
            }
        else
        // Check if now is the simple scheduled on time for this schedule.
        if (mm == getSimpleScheduleOn(which))
            {
            valveMode->setWarmModeDebounced(true);
            }
        }
    }


// Get the simple/primary schedule off time, as minutes after midnight [0,1439]; invalid (eg ~0) if none set.
// This is based on specified start time and some element of the current eco/comfort bias.
//   * which  schedule number, counting from 0
uint_least16_t SimpleValveScheduleParams::getSimpleScheduleOff(const uint8_t which) const
  {
  const uint_least16_t startMins = getSimpleScheduleOn(which);
  if(startMins == (uint_least16_t)~0) { return(~0); }
  // Compute end from start, allowing for wrap-around at midnight.
  uint_least16_t endTime = startMins + PREWARM_MINS + onTime();
  if(endTime >= OTV0P2BASE::MINS_PER_DAY) { endTime -= OTV0P2BASE::MINS_PER_DAY; } // Allow for wrap-around at midnight.
  return(endTime);
  }

// True iff any schedule is currently 'on'/'WARM' even when schedules overlap.
// May be relatively slow/expensive.
// Can be used to suppress all 'off' activity except for the final one.
// Can be used to suppress set-backs during on times.
// Scheduled times near the midnight wrap-around are tricky.
bool SimpleValveScheduleParams::isAnyScheduleOnWARMNow(const uint_least16_t mm) const
  {
  if(mm >= OTV0P2BASE::MINS_PER_DAY) { return(false); } // Invalid time.

  const uint8_t maxS = maxSchedules();
  for(uint8_t which = 0; which < maxS; ++which)
    {
    const uint_least16_t s = getSimpleScheduleOn(which);
    // Deal with case where this schedule is not set at all (s == ~0);
    if(uint_least16_t(~0) == s) { continue; }

    const uint_least16_t e = getSimpleScheduleOff(which);

    // The test has to be aware if the end is apparently before the start,
    // ie having wrapped around midnight.
    if(s < e)
      {
      // Scheduled on period is not wrapped around midnight.
      // |    ... s   e .... |
      if((s <= mm) && (mm < e)) { return(true); }
      }
    else
      {
      // Scheduled on period is wrapped around midnight.
      // | e                   ....     s  |
      if((s <= mm) || (mm < e)) { return(true); }
      }
    }

  return(false);
  }

// True iff any schedule is due 'on'/'WARM' soon even when schedules overlap.
// May be relatively slow/expensive.
// Can be used to allow room to be brought up to at least a set-back temperature
// if very cold when a WARM period is due soon
// to help ensure that WARM target is met on time.
bool SimpleValveScheduleParams::isAnyScheduleOnWARMSoon(const uint_least16_t mm) const
  {
  if(mm >= OTV0P2BASE::MINS_PER_DAY) { return(false); } // Invalid time.
  const uint_least16_t mm0 = mm + PREPREWARM_MINS; // Look forward...
  const uint_least16_t mmadj = (mm0 >= OTV0P2BASE::MINS_PER_DAY) ? (mm0 - OTV0P2BASE::MINS_PER_DAY) : mm0;
  return(isAnyScheduleOnWARMNow(mmadj));
  }


#ifdef SimpleValveScheduleEEPROM_DEFINED

// Get the simple/primary schedule on time, as minutes after midnight [0,1439]; invalid (eg ~0) if none set.
// Will usually include a pre-warm time before the actual time set.
// Note that unprogrammed EEPROM value will result in invalid time, ie schedule not set.
//   * which  schedule number, counting from 0
uint_least16_t SimpleValveScheduleEEPROM::getSimpleScheduleOn(const uint8_t which) const
  {
  if(which >= MAX_SIMPLE_SCHEDULES) { return(~0); } // Invalid schedule number.
  uint8_t startMM;
  ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
    { startMM = eeprom_read_byte((uint8_t*)(V0P2BASE_EE_START_SIMPLE_SCHEDULE0_ON + which)); }
  if(startMM > MAX_COMPRESSED_MINS_AFTER_MIDNIGHT) { return(~0); } // No schedule set.
  // Compute start time from stored schedule value.
  return(computeScheduleOnTimeFromProgrammeByte(startMM));
  }

// Set the simple/primary simple on time.
//   * startMinutesSinceMidnightLT  is start/on time in minutes after midnight [0,1439]
//   * which  schedule number, counting from 0
// Invalid parameters will be ignored and false returned,
// else this will return true and isSimpleScheduleSet() will return true after this.
// NOTE: over-use of this routine may prematurely wear out the EEPROM.
bool SimpleValveScheduleEEPROM::setSimpleSchedule(const uint_least16_t startMinutesSinceMidnightLT, const uint8_t which)
  {
  if(which >= MAX_SIMPLE_SCHEDULES) { return(false); } // Invalid schedule number.
  if(startMinutesSinceMidnightLT >= OTV0P2BASE::MINS_PER_DAY) { return(false); } // Invalid time.

  // Set the schedule, minimising wear.
  const uint8_t startMM = computeProgrammeByteFromTime(startMinutesSinceMidnightLT);
  ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
    { OTV0P2BASE::eeprom_smart_update_byte((uint8_t*)(V0P2BASE_EE_START_SIMPLE_SCHEDULE0_ON + which), startMM); }
  return(true); // Assume EEPROM programmed OK...
  }

// Clear a simple schedule.
// There will be neither on nor off events from the selected simple schedule once this is called.
//   * which  schedule number, counting from 0
void SimpleValveScheduleEEPROM::clearSimpleSchedule(const uint8_t which)
  {
  if(which >= MAX_SIMPLE_SCHEDULES) { return; } // Invalid schedule number.
  // Clear the schedule back to 'unprogrammed' values, minimising wear.
  ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
    { OTV0P2BASE::eeprom_smart_erase_byte((uint8_t*)(V0P2BASE_EE_START_SIMPLE_SCHEDULE0_ON + which)); }
  }

// Returns true if any simple schedule is set, false otherwise.
// This implementation just checks for any valid schedule 'on' time.
bool SimpleValveScheduleEEPROM::isAnySimpleScheduleSet() const
  {
  for(uint8_t which = 0; which < MAX_SIMPLE_SCHEDULES; ++which)
    {
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
      {
      if(eeprom_read_byte((uint8_t*)(V0P2BASE_EE_START_SIMPLE_SCHEDULE0_ON + which)) <= MAX_COMPRESSED_MINS_AFTER_MIDNIGHT)
        { return(true); }
      }
    }
  return(false);
  }

#endif // SimpleValveScheduleEEPROM_DEFINED


}
