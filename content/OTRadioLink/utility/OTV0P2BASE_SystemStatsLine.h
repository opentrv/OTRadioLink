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

Author(s) / Copyright (s): Damon Hart-Davis 2017
*/

/*
 Simple single-line system stats display (eg to Serial).

 (Derived from UI_Minimal.cpp 2013--2017 including PICAXE code.)
 */

#ifndef OTV0P2BASE_SYSTEMSTATSLINE_H
#define OTV0P2BASE_SYSTEMSTATSLINE_H

#include <stdint.h>
#include <string.h>

#include "OTV0P2BASE_JSONStats.h"
#include "OTV0P2BASE_Sensor.h"
#include "OTV0P2BASE_Serial_LineType_InitChar.h"
#include "OTV0P2BASE_Serial_IO.h"
#include "OTV0P2BASE_PowerManagement.h"
#include "OTV0P2BASE_Util.h"


namespace OTV0P2BASE
{


// V0.09 / V0.2 style '=' stats line generation.
// Parameterised and made somewhat unit testable.
//
// This class is parameterised on the output channel/device
// and sensors and other sources to show data from.
//
// Much of the code has to be in the header
// to be able to see the application's compile-time ENABLE headers
// and other compile-time selections for maximum parsimony.
//
// This is especially useful for development and debugging,
// and a status line is usually requested about once per minute.
//
// At most one of these instances should be created, usually statically,
// in a typical device such as a valve.
// Devices may omit this to save code and data space.

// Sends a short 1-line CRLF-terminated status report on the serial connection
// (at 'standard' baud).
// Similar to original PICAXE V0.1 output to allow one parser to handle both.
// Will turn on UART just for the duration of this call if powered off.
// Has multiple sections, some optional, starting with a unique letter
// and separated with ';'.

//Status output may look like this...
//=F0%@18C;T16 36 W255 0 F255 0;S5 5 17
//=W0%@18C;T16 38 W255 0 F255 0;S5 5 17
//=W0%@18C;T16 39 W255 0 F255 0;S5 5 17
//=W0%@18C;T16 40 W16 39 F17 39;S5 5 17
//=W0%@18C;T16 41 W16 39 F17 39;S5 5 17
//=W0%@17C;T16 42 W16 39 F17 39;S5 5 17
//=W20%@17C;T16 43 W16 39 F17 39;S5 5 17
//=W20%@17C;T16 44 W16 39 F17 39;S5 5 17
//=F0%@17C;T16 45 W16 39 F17 39;S5 5 17
//
//When driving an FHT8V wireless radiator valve it may look like this:
//=F0%@18C;T2 30 W10 0 F12 0;S5 5 17 wf;HC255 255
//=F0%@18C;T2 30 W10 0 F12 0;S5 5 17 wf;HC255 255
//=W0%@18C;T2 31 W10 0 F12 0;S5 5 17 wf;HC255 255
//=W10%@18C;T2 32 W10 0 F12 0;S5 5 17 wf;HC255 255
//=W20%@18C;T2 33 W10 0 F12 0;S5 5 17 wfo;HC255 255
//
// Or on a REV8 boiler controller ~20170203 like this:
//=F@23CA;T1 8 W255 0 F255 0 W255 0 F255 0;S 6 18 e;C5
//
//'=' starts the status line and CRLF ends it; sections are separated with ";".
//The initial 'W' or 'F' is WARM or FROST mode indication.  (If BAKE mode is supported, 'B' may be shown instead of 'W' when in BAKE.)
//The nn% is the target valve open percentage.
//The @nnCh gives the current measured room temperature in (truncated, not rounded) degrees C, followed by hex digit for 16ths.
//The ";" terminates this initial section.
//Thh mm is the local current 24h time in hours and minutes.
//Whh mm is the scheduled on/warm time in hours and minutes, or an invalid time if none.
//Fhh mm is the scheduled off/frost time in hours and minutes, or an invalid time if none.
//The ";" terminates this schedule section.
//'S' introduces the current and settable-target temperatures in Celsius/centigrade, if supported.
//eg 'S5 5 17'
//The first number is the current target in C, the second is the FROST target, the third is the WARM target.
//The 'e' or 'c' indicates eco or comfort bias.
//A 'w' indicates that this hour is predicted for smart warming ('f' indicates not), and another 'w' the hour ahead.
//A trailing 'o' indicates room occupancy.
//The ";" terminates this 'settable' section.
//
//'HC' introduces the optional FHT8V house codes section, if supported and codes are set.
//eg 'HC99 99'
//HChc1 hc2 are the house codes 1 and 2 for an FHT8V valve.

template
  <
    // Mandatory values.
    class valveMode_t /*= OTRadValve::ValveMode*/, const valveMode_t *const valveMode,

    // Optional values (each object can be NULL).
    class radValve_t /*= OTRadValve::AbstractRadValve*/, const radValve_t *const modelledRadValveOpt = NULL,
    class tempC16_t = Sensor<int16_t> /*TemperatureC16Base*/, const tempC16_t *tempC16Opt = NULL,
    class humidity_t = SimpleTSUint8Sensor /*HumiditySensorBase*/, const humidity_t *humidityOpt = NULL,
    class ambLight_t = SimpleTSUint8Sensor /*SensorAmbientLightBase*/, const ambLight_t *ambLightOpt = NULL,
    class occupancy_t = SimpleTSUint8Sensor /*PseudoSensorOccupancyTracker*/, const occupancy_t *occupancyOpt = NULL,
    class schedule_t = SimpleValveScheduleBase, const schedule_t *schedule = NULL,

    // True to enable trailing rotating JSON stats.
    const bool enableTrailingJSONStats = true,

#if defined(ARDUINO)
    // For Arduino, default to logging to the (first) hardware Serial device,
    // and starting it up, flushing it, and shutting it down also.
    class Print_t = decltype(Serial), Print *const printer = Serial,
    const bool wakeFlushSleepSerial = true
#else
    // Must supply non-NULL output Print device / stream.
    class Print_t = Print, Print_t *const printer = (Print_t*)NULL,
    // True if Serial should be woken / flushed / put to sleep.
    // Should be false for non-AVR platforms.
    const bool wakeFlushSleepSerial = false
#endif

  >
class SystemStatsLine final
    {
    private:
        // Count of available stats to post in JSON section.
        static constexpr uint8_t ss1Size = !enableTrailingJSONStats ? 0 :
            (
            ((NULL != humidityOpt) ? 1 : 0) +
            ((NULL != ambLightOpt) ? 1 : 0) +
            ((NULL != occupancyOpt) ? 1 : 0) +
            0 // ((NULL != modelledRadValveOpt) ? 1 : 0)
            );
        static constexpr bool noJS = (0 == ss1Size);
        // Configured for maximum different possible stats shown in rotation.
        // If JSON stats not enabled then omit object and code dependency.
        class dummySSR final
            {
            public:
                bool put(MSG_JSON_SimpleStatsKey_t, int16_t, bool = false) { return(false); }
                template <class T> bool put(const OTV0P2BASE::SensorCore<T> &, bool = false) { return(false); }
                inline uint8_t writeJSON(...) { return(0); }
            };
        template <bool Condition, typename TypeTrue, typename TypeFalse>
          struct typeIf;
        template <typename TypeTrue, typename TypeFalse>
          struct typeIf<true, TypeTrue, TypeFalse> { typedef TypeTrue t; };
        template <typename TypeTrue, typename TypeFalse>
          struct typeIf<false, TypeTrue, TypeFalse> { typedef TypeFalse t; };
        typedef typename typeIf<noJS, dummySSR, SimpleStatsRotation<ss1Size>>::t ss1_type;
        ss1_type ss1;

    public:
        void serialStatusReport()
            {
            static_assert(NULL != printer, "outputDevice must not be null");
            static_assert(NULL != valveMode, "valveMode must not be null");

#if defined(ARDUINO_ARCH_AVR)
            const bool neededWaking = wakeFlushSleepSerial &&
                OTV0P2BASE::powerUpSerialIfDisabled<>();
#else
            static_assert(!wakeFlushSleepSerial, "wakeFlushSleepSerial needs hardware Serial");
#endif

            // Aim to overlap CPU usage with characters being TXed
            // for throughput determined primarily by output size and baud
            // for real Serial or other async output.

            // Stats line starts with distinguished marker character.
            // Initial '=' section with common essentials.
            printer->print((char) SERLINE_START_CHAR_STATS);
            // Valve device mode F/W/B.
            printer->print(valveMode->inWarmMode() ? (valveMode->inBakeMode() ? 'B' : 'W') : 'F');

            // Valve target percent open, if available.
            // Display as nn% where nn is in decimal, eg from "0%" to "100%".
            if(NULL != modelledRadValveOpt)
                {
                printer->print(modelledRadValveOpt->get());
                printer->print('%');
                }

            // Temperature in C, if available.
            // Display as:
            //   * '@'
            //   * unrounded whole degrees C
            //   * 'C'
            //   * then 16ths as a single (upper-case) hex digit
            // eg "23CA" for 23+10/16 C
            //
            // Note that the trailing hex digit was not present originally.
            if(NULL != tempC16Opt)
                {
                const int16_t temp = tempC16Opt->get();
                printer->print('@');
                printer->print(int(temp >> 4));
                printer->print('C');
                printer->print(int(temp & 0xf), 16);
                }

            //#ifdef ENABLE_FULL_OT_CLI
            //  // *X* section: Xmit security level shown only if some non-essential TX potentially allowed.
            //  const OTV0P2BASE::stats_TX_level xmitLevel = OTV0P2BASE::getStatsTXLevel();
            //  if(xmitLevel < OTV0P2BASE::stTXnever) { Serial.print(F(";X")); Serial.print(xmitLevel); }
            //#endif


// TODO


//#ifdef ENABLE_FULL_OT_CLI
//  // *T* section: time and schedules.
//  const uint_least8_t hh = OTV0P2BASE::getHoursLT();
//  const uint_least8_t mm = OTV0P2BASE::getMinutesLT();
//  Serial.print(';'); // End previous section.
//  Serial.print('T'); Serial.print(hh); OTV0P2BASE::Serial_print_space(); Serial.print(mm);
//#if defined(SCHEDULER_AVAILABLE)
//  // Show all schedules set.
//  for(uint8_t scheduleNumber = 0; scheduleNumber < Scheduler.MAX_SIMPLE_SCHEDULES; ++scheduleNumber)
//    {
//    OTV0P2BASE::Serial_print_space();
//    uint_least16_t startMinutesSinceMidnightLT = Scheduler.getSimpleScheduleOn(scheduleNumber);
//    const bool invalidStartTime = startMinutesSinceMidnightLT >= OTV0P2BASE::MINS_PER_DAY;
//    const int startH = invalidStartTime ? 255 : (startMinutesSinceMidnightLT / 60);
//    const int startM = invalidStartTime ? 0 : (startMinutesSinceMidnightLT % 60);
//    Serial.print('W'); Serial.print(startH); OTV0P2BASE::Serial_print_space(); Serial.print(startM);
//    OTV0P2BASE::Serial_print_space();
//    uint_least16_t endMinutesSinceMidnightLT = Scheduler.getSimpleScheduleOff(scheduleNumber);
//    const bool invalidEndTime = endMinutesSinceMidnightLT >= OTV0P2BASE::MINS_PER_DAY;
//    const int endH = invalidEndTime ? 255 : (endMinutesSinceMidnightLT / 60);
//    const int endM = invalidEndTime ? 0 : (endMinutesSinceMidnightLT % 60);
//    Serial.print('F'); Serial.print(endH); OTV0P2BASE::Serial_print_space(); Serial.print(endM);
//    }
//  if(Scheduler.isAnyScheduleOnWARMNow()) { Serial.print('*'); } // Indicate that at least one schedule is active now.
//#endif // ENABLE_SINGLETON_SCHEDULE
//#endif
//
//  // *S* section: settable target/threshold temperatures, current target, and eco/smart/occupied flags.
//#if defined(ENABLE_SETTABLE_TARGET_TEMPERATURES) || defined(TEMP_POT_AVAILABLE)
//  Serial.print(';'); // Terminate previous section.
//  Serial.print('S'); // Current settable temperature target, and FROST and WARM settings.
//#ifdef ENABLE_LOCAL_TRV
//  Serial.print(NominalRadValve.getTargetTempC());
//#endif // ENABLE_LOCAL_TRV
//  OTV0P2BASE::Serial_print_space();
//  Serial.print(tempControl.getFROSTTargetC());
//  OTV0P2BASE::Serial_print_space();
//  const uint8_t wt = tempControl.getWARMTargetC();
//  Serial.print(wt);
//#ifdef ENABLE_FULL_OT_CLI
//  // Show bias.
//  OTV0P2BASE::Serial_print_space();
//  Serial.print(tempControl.hasEcoBias() ? (tempControl.isEcoTemperature(wt) ? 'E' : 'e') : (tempControl.isComfortTemperature(wt) ? 'C': 'c')); // Show eco/comfort bias.
//#endif // ENABLE_FULL_OT_CLI
//#endif // ENABLE_SETTABLE_TARGET_TEMPERATURES
//
//  // *C* section: central hub values.
//#if defined(ENABLE_BOILER_HUB) || defined(ENABLE_STATS_RX)
//  // Print optional hub boiler-on-time section if apparently set (non-zero) and thus in hub mode.
//  const uint8_t boilerOnMinutes = getMinBoilerOnMinutes();
//  if(boilerOnMinutes != 0)
//    {
//    Serial.print(';'); // Terminate previous section.
//    Serial.print('C'); // Indicate central hub mode available.
//    Serial.print(boilerOnMinutes); // Show min 'on' time, or zero if disabled.
//    }
//#endif
//
//  // *H* section: house codes for local FHT8V valve and if syncing, iff set.
//#if defined(ENABLE_FHT8VSIMPLE)
//  // Print optional house code section if codes set.
//  const uint8_t hc1 = FHT8V.nvGetHC1();
//  if(hc1 != 255)
//    {
//    Serial.print(F(";HC"));
//    Serial.print(hc1);
//    OTV0P2BASE::Serial_print_space();
//    Serial.print(FHT8V.nvGetHC2());
//    if(!FHT8V.isInNormalRunState())
//      {
//      OTV0P2BASE::Serial_print_space();
//      Serial.print('s'); // Indicate syncing with trailing lower-case 's' in field...
//      }
//    }
//#endif
//
//#if defined(ENABLE_LOCAL_TRV) && !defined(ENABLE_TRIMMED_MEMORY)
//  // *M* section: min-valve-percentage open section, iff not at default value.
//  const uint8_t minValvePcOpen = NominalRadValve.getMinValvePcReallyOpen();
//  if(OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN != minValvePcOpen) { Serial.print(F(";M")); Serial.print(minValvePcOpen); }
//#endif


            // If allowed, print trailing JSON rotation of key values.
            if(!noJS)
                {
                // Terminate previous section.
                printer->print(';');

                // Buffer for { ... } JSON output.
                // Keep short to avoid serial overruns.
                char buf[40];
                ss1.put(*humidityOpt);
                ss1.put(*ambLightOpt);
                ss1.put(*occupancyOpt);
                //  ss1.put(Occupancy.vacHTag(), Occupancy.getVacancyH()); // EXPERIMENTAL
//                ss1.put(modelledRadValveOpt->tagCMPC(), modelledRadValveOpt->getCumulativeMovementPC()); // EXPERIMENTAL
// TODO
//ss1.put(Supply_cV);

                  const uint8_t wrote = ss1.writeJSON((uint8_t *)buf, sizeof(buf), 0, true);
                  if(0 != wrote) { printer->print(buf); }
                  }

            // Terminate line.
            printer->println();

#if defined(ARDUINO_ARCH_AVR)
            // Ensure that all text is sent before this routine returns,
            // in case any sleep/powerdown follows that kills the UART.
            OTV0P2BASE::flushSerialSCTSensitive();

            if(neededWaking) { OTV0P2BASE::powerDownSerial(); }
#endif
            }
    };


}
#endif
