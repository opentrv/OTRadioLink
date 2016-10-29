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

Author(s) / Copyright (s): Damon Hart-Davis 2015
*/

/*
 Simple schedule support for TRV.

 V0p2/AVR only for now.
 */

#ifndef OTV0P2BASE_SIMPLEVALVESCHEDULE_H
#define OTV0P2BASE_SIMPLEVALVESCHEDULE_H

#include "OTV0P2BASE_EEPROM.h"
#include "OTV0P2BASE_Util.h"

namespace OTV0P2BASE
{


// Base for simple single-button (per programme) on-time scheduler, for individual TRVs.
// Uses one EEPROM byte per program.
// Has an on-time that may be varied by, for example, comfort level.
#define SimpleValveScheduleBase_DEFINED
class SimpleValveScheduleBase
   {
   public:
        // Returns maximum number of schedules supported.
        virtual uint8_t maxSchedules() const = 0;

        // Returns the basic on-time for the program, in minutes; strictly positive.
        // Does not include pre-warm (not pre-pre-warm time).
        // Overriding may vary with arbitrary external parameters.
        // This implementation provides a very simple fixed time.
        virtual uint8_t onTime() const = 0;

        // Get the simple schedule off time, as minutes after midnight [0,1439]; invalid (eg ~0) if none set.
        // This is based on specified start time and some element of the current eco/comfort bias.
        //   * which  schedule number, counting from 0
        virtual uint_least16_t getSimpleScheduleOff(uint8_t which) const = 0;

        // Get the simple schedule on time, as minutes after midnight [0,1439]; invalid (eg ~0) if none set.
        // Will usually include a pre-warm time before the actual time set.
        // Note that unprogrammed EEPROM value will result in invalid time, ie schedule not set.
        //   * which  schedule number, counting from 0
        virtual uint_least16_t getSimpleScheduleOn(uint8_t which) const = 0;

        // Set the simple simple on time.
        //   * startMinutesSinceMidnightLT  is start/on time in minutes after midnight [0,1439]
        //   * which  schedule number, counting from 0
        // Invalid parameters will be ignored and false returned,
        // else this will return true and isSimpleScheduleSet() will return true after this.
        // NOTE: over-use of this routine can prematurely wear out the EEPROM.
        virtual bool setSimpleSchedule(uint_least16_t startMinutesSinceMidnightLT, uint8_t which) = 0;

        // Clear a simple schedule.
        // There will be neither on nor off events from the selected simple schedule once this is called.
        //   * which  schedule number, counting from 0
        virtual void clearSimpleSchedule(uint8_t which) = 0;

        // True iff any schedule is 'on'/'WARN' even when schedules overlap.
        // Can be used to suppress all 'off' activity except for the final one.
        // Can be used to suppress set-backs during on times.
        virtual bool isAnyScheduleOnWARMNow() const = 0;

        // True iff any schedule is due 'on'/'WARM' soon even when schedules overlap.
        // May be relatively slow/expensive.
        // Can be used to allow room to be brought up to at least a set-back temperature
        // if very cold when a WARM period is due soon (to help ensure that WARM target is met on time).
        virtual bool isAnyScheduleOnWARMSoon() const = 0;

        // True iff any schedule is currently 'on'/'WARM' even when schedules overlap.
        // May be relatively slow/expensive.
        // Can be used to suppress all 'off' activity except for the final one.
        // Can be used to suppress set-backs during on times.
        virtual bool isAnySimpleScheduleSet() const = 0;
   };


#ifdef ARDUINO_ARCH_AVR
// Simple single-button (per programme) on-time scheduler, for individual TRVs.
// Uses one EEPROM byte per program.
// Has an on-time that may be varied by, for example, comfort level.
#define SimpleValveScheduleEEPROM_DEFINED
class SimpleValveScheduleEEPROM : public SimpleValveScheduleBase
    {
    public:
        // Granularity of simple schedule in minutes (values may be rounded/truncated to nearest); strictly positive.
        static constexpr uint8_t SIMPLE_SCHEDULE_GRANULARITY_MINS = 6;

        // Number of supported schedules.
        // Can be more than the number of buttons, but later schedules will be CLI-only.
        // Depends on space reserved in EEPROM for programmes, one byte per programme.
        static constexpr uint8_t MAX_SIMPLE_SCHEDULES = V0P2BASE_EE_START_MAX_SIMPLE_SCHEDULES;

        // Returns maximum number of schedules supported.
        virtual uint8_t maxSchedules() const override { return(MAX_SIMPLE_SCHEDULES); }

        // Target basic scheduled on time for heating in minutes (typically 1h); strictly positive.
        static constexpr uint8_t BASIC_SCHEDULED_ON_TIME_MINS = 60;

        // Pre-warm time before learned/scheduled WARM period,
        // based on basic scheduled on time and allowing for some wobble in the timing resolution.
        // DHD20151122: even half an hour may not be enough if very cold and heating system not good.
        // DHD20160112: with 60m BASIC_SCHEDULED_ON_TIME_MINS this should yield ~36m.
        static constexpr uint8_t PREWARM_MINS = OTV0P2BASE::fnmax(30, (SIMPLE_SCHEDULE_GRANULARITY_MINS + (BASIC_SCHEDULED_ON_TIME_MINS/2)));

        // Setback period before WARM period to help ensure that the WARM target can be reached on time.
        // Important for slow-to-heat rooms that have become very cold.
        // Similar to or a little longer than PREWARM_MINS
        // so that we can safely use this without causing distress, eg waking people up.
        // DHD20160112: with 36m PREWARM_MINS this should yield ~54m for a total run-up of 90m.
        static constexpr uint8_t PREPREWARM_MINS = (3*(PREWARM_MINS/2));

        // Returns the basic on-time for the program, in minutes; strictly positive.
        // Does not include pre-warm (not pre-pre-warm time).
        // Overriding may vary with arbitrary external parameters.
        // This implementation provides a very simple fixed time.
        virtual uint8_t onTime() const override { return(BASIC_SCHEDULED_ON_TIME_MINS); }

        // Get the simple schedule off time, as minutes after midnight [0,1439]; invalid (eg ~0) if none set.
        // This is based on specified start time and some element of the current eco/comfort bias.
        //   * which  schedule number, counting from 0
        virtual uint_least16_t getSimpleScheduleOff(uint8_t which) const override;

        // Get the simple schedule on time, as minutes after midnight [0,1439]; invalid (eg ~0) if none set.
        // Will usually include a pre-warm time before the actual time set.
        // Note that unprogrammed EEPROM value will result in invalid time, ie schedule not set.
        //   * which  schedule number, counting from 0
        virtual /*static*/ uint_least16_t getSimpleScheduleOn(uint8_t which) const override;

        // Set the simple simple on time.
        //   * startMinutesSinceMidnightLT  is start/on time in minutes after midnight [0,1439]
        //   * which  schedule number, counting from 0
        // Invalid parameters will be ignored and false returned,
        // else this will return true and isSimpleScheduleSet() will return true after this.
        // NOTE: over-use of this routine can prematurely wear out the EEPROM.
        virtual bool setSimpleSchedule(uint_least16_t startMinutesSinceMidnightLT, uint8_t which) override;

        // Clear a simple schedule.
        // There will be neither on nor off events from the selected simple schedule once this is called.
        //   * which  schedule number, counting from 0
        virtual void clearSimpleSchedule(uint8_t which) override;

        // True iff any schedule is 'on'/'WARN' even when schedules overlap.
        // Can be used to suppress all 'off' activity except for the final one.
        // Can be used to suppress set-backs during on times.
        virtual bool isAnyScheduleOnWARMNow() const override;

        // True iff any schedule is due 'on'/'WARM' soon even when schedules overlap.
        // May be relatively slow/expensive.
        // Can be used to allow room to be brought up to at least a set-back temperature
        // if very cold when a WARM period is due soon (to help ensure that WARM target is met on time).
        virtual bool isAnyScheduleOnWARMSoon() const override;

        // True iff any schedule is currently 'on'/'WARM' even when schedules overlap.
        // May be relatively slow/expensive.
        // Can be used to suppress all 'off' activity except for the final one.
        // Can be used to suppress set-backs during on times.
        virtual bool isAnySimpleScheduleSet() const override;
    };

#endif // ARDUINO_ARCH_AVR


// Empty type-correct substitute for SimpleValveScheduleBase
// for when no Scheduler is require to simplify coding.
// Never has schedules nor allows them to be set.
class NULLValveSchedule : public SimpleValveScheduleBase
  {
  public:
    virtual uint8_t maxSchedules() const override { return(0); }
    virtual uint8_t onTime() const override { return(1); }
    virtual uint_least16_t getSimpleScheduleOff(uint8_t) const override { return(~0); }
    virtual uint_least16_t getSimpleScheduleOn(uint8_t) const override { return(~0); }
    virtual bool setSimpleSchedule(uint_least16_t, uint8_t) override { return(false); }
    virtual void clearSimpleSchedule(uint8_t) override { }
    virtual bool isAnyScheduleOnWARMNow() const override { return(false); }
    virtual bool isAnyScheduleOnWARMSoon() const override { return(false); }
    virtual bool isAnySimpleScheduleSet() const override { return(false); }
  };

// Dummy substitute for SimpleValveScheduleBase
// for when no Scheduler is require to simplify coding.
// Never has schedules nor allows them to be set.
class DummyValveSchedule
    {
    public:
        static uint_least16_t getSimpleScheduleOff(uint8_t) { return(~0); }
        static uint_least16_t getSimpleScheduleOn(uint8_t)  { return(~0); }
        static bool isAnyScheduleOnWARMNow() { return(false); }
        static bool isAnyScheduleOnWARMSoon() { return(false); }
        static bool isAnySimpleScheduleSet() { return(false); }
    };

}
#endif
