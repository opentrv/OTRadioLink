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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2017
*/

/*
 Simple valve schedule support for TRV.

 V0p2/AVR only for now.
 */

#ifndef OTV0P2BASE_SIMPLEVALVESCHEDULE_H
#define OTV0P2BASE_SIMPLEVALVESCHEDULE_H

#include "OTV0P2BASE_EEPROM.h"
#include "OTV0P2BASE_Util.h"


#include "OTRadValve_ValveMode.h"


namespace OTRadValve
{


// Base for simple single-button (per programme) on-time scheduler, for individual TRVs.
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
        // Note that unprogrammed schedule value will result in invalid time, ie schedule not set.
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
        //   * mm  minutes from midnight (usually local time);
        //     must be less than OTV0P2BASE::MINS_PER_DAY
        virtual bool isAnyScheduleOnWARMNow(uint_least16_t mm) const = 0;

        // True iff any schedule is due 'on'/'WARM' soon even when schedules overlap.
        // May be relatively slow/expensive.
        // Can be used to allow room to be brought up to at least a set-back temperature
        // if very cold when a WARM period is due soon
        // to help ensure that WARM target is met on time.
        //   * mm  minutes from midnight (usually local time);
        //     must be less than OTV0P2BASE::MINS_PER_DAY
        virtual bool isAnyScheduleOnWARMSoon(uint_least16_t mm) const = 0;

        // True iff any schedule is currently 'on'/'WARM' even when schedules overlap.
        // May be relatively slow/expensive.
        // Can be used to suppress all 'off' activity except for the final one.
        // Can be used to suppress set-backs during on times.
        virtual bool isAnySimpleScheduleSet() const = 0;

        // Check/apply the user's schedule.
        // This should be called (at least) once each minute
        // to apply any current schedule to the valve state,
        // ie moving it between frost and warm modes.
        //
        // This will only move to frost mode when no current warm schedule
        // is enabled, so overlapping schedules behave as expected.
        //
        //   * mm  minutes since midnight local time [0,1439]
        void applyUserSchedule(OTRadValve::ValveMode* const valveMode,
                const uint_least16_t mm) const;
   };

// Some basic properties and implementation of a simple scheduler.
// These will hold for the EEPROM-backed AVR version for example,
// as well as a more testable RAM-based version.
class SimpleValveScheduleParams : public SimpleValveScheduleBase
    {
    public:
        // Granularity of simple schedule in minutes (values may be rounded/truncated to nearest); strictly positive.
        static constexpr uint8_t SIMPLE_SCHEDULE_GRANULARITY_MINS = 6;

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

        // True iff any schedule is 'on'/'WARN' even when schedules overlap.
        // Can be used to suppress all 'off' activity except for the final one.
        // Can be used to suppress set-backs during on times.
        //   * mm  minutes from midnight (usually local time);
        //     must be less than OTV0P2BASE::MINS_PER_DAY
        // Scheduled times near the midnight wrap-around are tricky.
        virtual bool isAnyScheduleOnWARMNow(uint_least16_t mm) const override;

        // True iff any schedule is due 'on'/'WARM' soon even when schedules overlap.
        // May be relatively slow/expensive.
        // Can be used to allow room to be brought up to at least a set-back temperature
        // if very cold when a WARM period is due soon
        // (to help ensure that WARM target is met on time).
        //   * mm  minutes from midnight (usually local time);
        //     must be less than OTV0P2BASE::MINS_PER_DAY
        virtual bool isAnyScheduleOnWARMSoon(uint_least16_t mm) const override;

        // Maximum mins-after-midnight compacted value in one byte.
        // Exposed to facilitate unit testing.
        static constexpr uint8_t MAX_COMPRESSED_MINS_AFTER_MIDNIGHT = ((OTV0P2BASE::MINS_PER_DAY / SIMPLE_SCHEDULE_GRANULARITY_MINS) - 1);

        // Compute byte to store for specified on/off time.
        // This byte is directly suitable to store in EEPROM / etc.
        //   * startMinutesSinceMidnightLT  minutes from local midnight;
        //     must be less than OTV0P2BASE::MINS_PER_DAY
        // Returns value in range [0,MAX_COMPRESSED_MINS_AFTER_MIDNIGHT].
        // Exposed to facilitate unit testing.
        static uint8_t computeProgrammeByteFromTime(const uint_least16_t startMinutesSinceMidnightLT)
            {
            // Round down; will bring times forward to start of grain.
            return(startMinutesSinceMidnightLT / SIMPLE_SCHEDULE_GRANULARITY_MINS);
            }

        // Computes the schedule time from the programme byte..
        // Essentially performs the reverse of computeProgrammeByteFromTime()
        // to the storage granularity.
        //   * startMM  the programme byte;
        //     must be no more than MAX_COMPRESSED_MINS_AFTER_MIDNIGHT
        // Exposed to facilitate unit testing.
        static uint_least16_t computeTimeFromPrgrammeByte(const uint8_t startMM)
            {
            // Compute start time from stored schedule value.
            return(SIMPLE_SCHEDULE_GRANULARITY_MINS * startMM);
            }

        // Compute the simple/primary schedule on time, as minutes after midnight [0,1439].
        // Will usually include a pre-warm time before the actual time set.
        //   * startMM  the programme byte;
        //     must be no more than MAX_COMPRESSED_MINS_AFTER_MIDNIGHT
        // Exposed to facilitate unit testing.
        static uint_least16_t computeScheduleOnTimeFromProgrammeByte(const uint8_t startMM)
            {
            uint_least16_t startTime = computeTimeFromPrgrammeByte(startMM);
            // Wind back start time to allow room to reach target on time.
            const uint8_t windBackM = PREWARM_MINS;
            // Deal with wrap-around at midnight.
            if(windBackM > startTime) { startTime += OTV0P2BASE::MINS_PER_DAY; }
            startTime -= windBackM;
            return(startTime);
            }
    };

// Mock/testable class sharing some core impl with SimpleValveScheduleEEPROM.
template<uint8_t maxSched = 2>
class SimpleValveScheduleMock final : public SimpleValveScheduleParams
    {
    private:
        // One byte per program, using same bit patterns as EEPROM.
        uint8_t programmes[maxSched];

    public:
        // Ensure all schedules cleared/empty on construction.
        SimpleValveScheduleMock()
            { for(int i = 0; i < maxSched; ++i) { clearSimpleSchedule(i); } }

        // Returns maximum number of schedules supported.
        virtual uint8_t maxSchedules() const override final { return(maxSched); }

        // Get the simple schedule on time, as minutes after midnight [0,1439]; invalid (eg ~0) if none set.
        // Will usually include a pre-warm time before the actual time set.
        // Note that unprogrammed schedule value will result in invalid time, ie schedule not set.
        //   * which  schedule number, counting from 0
        virtual uint_least16_t getSimpleScheduleOn(uint8_t which) const override
            {
            if(which >= maxSchedules()) { return(~0); } // Invalid schedule number.
            const uint8_t startMM = programmes[which];
            if(startMM > MAX_COMPRESSED_MINS_AFTER_MIDNIGHT) { return(~0); } // No schedule set.
            // Compute start time from stored schedule value.
            return(computeScheduleOnTimeFromProgrammeByte(startMM));
            }

        // Set the simple simple on time.
        //   * startMinutesSinceMidnightLT  is start/on time in minutes after midnight [0,1439]
        //   * which  schedule number, counting from 0
        // Invalid parameters will be ignored and false returned,
        // else this will return true and isSimpleScheduleSet() will return true after this.
        // NOTE: over-use of this routine can prematurely wear out the EEPROM.
        virtual bool setSimpleSchedule(uint_least16_t startMinutesSinceMidnightLT, uint8_t which) override
            {
            if(which >= maxSchedules()) { return(false); } // Invalid schedule number.
            if(startMinutesSinceMidnightLT >= OTV0P2BASE::MINS_PER_DAY) { return(false); } // Invalid time.
            const uint8_t startMM = computeProgrammeByteFromTime(startMinutesSinceMidnightLT);
            programmes[which] = startMM;
            return(true);
            }

        // Clear a simple schedule.
        // There will be neither on nor off events from the selected simple schedule once this is called.
        //   * which  schedule number, counting from 0
        virtual void clearSimpleSchedule(const uint8_t which) override
            {
            if(which >= maxSchedules()) { return; } // Invalid schedule number.
            // Clear the schedule back to 'unprogrammed' values.
            programmes[which] = 0xff;
            }

        // True iff any schedule is currently 'on'/'WARM' even when schedules overlap.
        // May be relatively slow/expensive.
        // Can be used to suppress all 'off' activity except for the final one.
        // Can be used to suppress set-backs during on times.
        virtual bool isAnySimpleScheduleSet() const override
            {
            for(uint8_t which = 0; which < maxSchedules(); ++which)
                {
                if(programmes[which] <= MAX_COMPRESSED_MINS_AFTER_MIDNIGHT)
                  { return(true); }
                }
            return(false);
            }
    };


#ifdef ARDUINO_ARCH_AVR
// Simple single-button (per programme) on-time scheduler, for individual TRVs.
// Uses one EEPROM byte per program.
// Has an on-time that may be varied by, for example, comfort level.
#define SimpleValveScheduleEEPROM_DEFINED
class SimpleValveScheduleEEPROM : public SimpleValveScheduleParams
    {
    public:
        // Number of supported schedules.
        // Can be more than the number of buttons, but later schedules will be CLI-only.
        // Depends on space reserved in EEPROM for programmes,
        // one byte per programme.
        static constexpr uint8_t MAX_SIMPLE_SCHEDULES = V0P2BASE_EE_START_MAX_SIMPLE_SCHEDULES;

        // Returns maximum number of schedules supported.
        virtual uint8_t maxSchedules() const override final { return(MAX_SIMPLE_SCHEDULES); }

        // Get the simple schedule on time, as minutes after midnight [0,1439]; invalid (eg ~0) if none set.
        // Will usually include a pre-warm time before the actual time set.
        // Note that unprogrammed EEPROM value will result in invalid time, ie schedule not set.
        //   * which  schedule number, counting from 0
        virtual uint_least16_t getSimpleScheduleOn(uint8_t which) const override;

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

        // True iff any schedule is currently 'on'/'WARM' even when schedules overlap.
        // May be relatively slow/expensive.
        // Can be used to suppress all 'off' activity except for the final one.
        // Can be used to suppress set-backs during on times.
        virtual bool isAnySimpleScheduleSet() const override;
    };

// Customised scheduler implementation as for OpenTRV V0p2 REV2 ~2015.
// Adjusts the 'on' time based on:
//   * ECO/comfort bias
//   * if the room has been vacant for a long time (eg days)
// The second requires a non-NULL occupancy tracker pointer to be supplied.
class SimpleValveSchedule_PseudoSensorOccupancyTracker final { public: bool longVacant() const { return(false); } };
template<
    const uint8_t learnedOnM, const uint8_t learnedOnComfortM,
    class tempControl_t, const tempControl_t *tempControl,
    class occupancy_t = SimpleValveSchedule_PseudoSensorOccupancyTracker, const occupancy_t *occupancy = NULL
    >
class SimpleValveSchedule final : public SimpleValveScheduleEEPROM
    {
    public:
        // Allow scheduled on time to dynamically depend on comfort level.
        virtual uint8_t onTime() const override
            {
            // Simplify the logic where no variation in on time is required.
            if(learnedOnM == learnedOnComfortM) { return(learnedOnM); }
            else
                {
                // Variable 'on' time depending on how 'eco' the settings are.
                // Three-way split based on current WARM target temperature,
                // for a relatively gentle change in behaviour
                // along the valve dial for example.
                const uint8_t wt = tempControl->getWARMTargetC();
                if(tempControl->isEcoTemperature(wt))
                    { return(learnedOnM); }
                else if(tempControl->isComfortTemperature(wt))
                    { return(learnedOnComfortM); }
                // If occupancy detection is enabled
                // and the area is vacant for a long time (>1d)
                // and not at maximum comfort end of scale
                // then truncate the on period to attempt to save energy.
                else if((NULL != occupancy) && occupancy->longVacant())
                    { return(learnedOnM); }
                // Intermediate on-time for middle of the eco/comfort scale.
                else
                    { return((learnedOnM + learnedOnComfortM) / 2); }
                }
            }
    };

#endif // ARDUINO_ARCH_AVR


// Empty type-correct substitute for SimpleValveScheduleBase
// for when no Scheduler is require to simplify coding.
// Never has schedules nor allows them to be set.
class NULLValveSchedule final : public SimpleValveScheduleBase
  {
  public:
    virtual uint8_t maxSchedules() const override { return(0); }
    virtual uint8_t onTime() const override { return(1); }
    virtual uint_least16_t getSimpleScheduleOff(uint8_t) const override { return(uint_least16_t(~0)); }
    virtual uint_least16_t getSimpleScheduleOn(uint8_t) const override { return(uint_least16_t(~0)); }
    virtual bool setSimpleSchedule(uint_least16_t, uint8_t) override { return(false); }
    virtual void clearSimpleSchedule(uint8_t) override { }
    virtual bool isAnyScheduleOnWARMNow(uint_least16_t) const override { return(false); }
    virtual bool isAnyScheduleOnWARMSoon(uint_least16_t) const override { return(false); }
    virtual bool isAnySimpleScheduleSet() const override { return(false); }
  };

// Dummy substitute for SimpleValveScheduleBase
// for when no Scheduler is required to simplify coding.
// Never has schedules nor allows them to be set.
class DummyValveSchedule final
    {
    public:
        static uint_least16_t getSimpleScheduleOff(uint8_t) { return(uint_least16_t(~0)); }
        static uint_least16_t getSimpleScheduleOn(uint8_t)  { return(uint_least16_t(~0)); }
        static bool isAnyScheduleOnWARMNow() { return(false); }
        static bool isAnyScheduleOnWARMSoon() { return(false); }
        static bool isAnySimpleScheduleSet() { return(false); }
    };


}
#endif
