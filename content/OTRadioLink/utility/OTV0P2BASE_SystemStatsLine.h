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
template
  <
  class valveMode_t /*= OTRadValve::ValveMode*/, const valveMode_t *const valveMode,

//  class occupancy_t = SimpleTSUint8Sensor /*PseudoSensorOccupancyTracker*/, const occupancy_t *occupancyOpt = NULL,
//  class ambLight_t = SimpleTSUint8Sensor /*SensorAmbientLightBase*/, const ambLight_t *ambLightOpt = NULL,
//  class tempC16_t = Sensor<int16_t> /*TemperatureC16Base*/, const tempC16_t *tempC16Opt = NULL,
//  class humidity_t = SimpleTSUint8Sensor /*HumiditySensorBase*/, const humidity_t *humidityOpt = NULL,

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
        // Configured for maximum different possible stats shown in rotation.
        static constexpr uint8_t maxStatsLineValues = 5;
        SimpleStatsRotation<maxStatsLineValues> ss1;

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


// TODO


//#if defined(ENABLE_NOMINAL_RAD_VALVE)
//  Serial.print(NominalRadValve.get()); Serial.print('%'); // Target valve position.
//#endif
//  const int temp = TemperatureC16.get();
//  Serial.print('@'); Serial.print(temp >> 4); Serial.print('C'); // Unrounded whole degrees C.
//      Serial.print(temp & 0xf, HEX); // Show 16ths in hex.
//
////#if 0
////  // *P* section: low power flag shown only if (battery) low.
////  if(Supply_mV.isSupplyVoltageLow()) { Serial.print(F(";Plow")); }
////#endif
//
//#ifdef ENABLE_FULL_OT_CLI
//  // *X* section: Xmit security level shown only if some non-essential TX potentially allowed.
//  const OTV0P2BASE::stats_TX_level xmitLevel = OTV0P2BASE::getStatsTXLevel();
//  if(xmitLevel < OTV0P2BASE::stTXnever) { Serial.print(F(";X")); Serial.print(xmitLevel); }
//#endif
//
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
//
//#if 1 && defined(ENABLE_JSON_OUTPUT) && !defined(ENABLE_TRIMMED_MEMORY)
//  Serial.print(';'); // Terminate previous section.
//  char buf[40]; // Keep short enough not to cause overruns.
//  static const uint8_t maxStatsLineValues = 5;
//  static OTV0P2BASE::SimpleStatsRotation<maxStatsLineValues> ss1; // Configured for maximum different stats.
////  ss1.put(TemperatureC16); // Already at start of = stats line.
//#if defined(HUMIDITY_SENSOR_SUPPORT)
//  ss1.put(RelHumidity);
//#endif // defined(HUMIDITY_SENSOR_SUPPORT)
//#if defined(ENABLE_AMBLIGHT_SENSOR)
//  ss1.put(AmbLight);
//#endif // ENABLE_AMBLIGHT_SENSOR
//  ss1.put(Supply_cV);
//#if defined(ENABLE_OCCUPANCY_SUPPORT)
//  ss1.put(Occupancy);
////  ss1.put(Occupancy.vacHTag(), Occupancy.getVacancyH()); // EXPERIMENTAL
//#endif // defined(ENABLE_OCCUPANCY_SUPPORT)
//#if defined(ENABLE_MODELLED_RAD_VALVE) && !defined(ENABLE_TRIMMED_MEMORY)
//    ss1.put(NominalRadValve.tagCMPC(), NominalRadValve.getCumulativeMovementPC()); // EXPERIMENTAL
//#endif // ENABLE_MODELLED_RAD_VALVE
//  const uint8_t wrote = ss1.writeJSON((uint8_t *)buf, sizeof(buf), 0, true);
//  if(0 != wrote) { Serial.print(buf); }
//#endif // defined(ENABLE_JSON_OUTPUT) && !defined(ENABLE_TRIMMED_MEMORY)



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
