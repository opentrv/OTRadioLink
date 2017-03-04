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

#include "OTV0P2BASE_Sensor.h"
#include "OTV0P2BASE_JSONStats.h"
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
//  class occupancy_t = SimpleTSUint8Sensor /*PseudoSensorOccupancyTracker*/, const occupancy_t *occupancyOpt = NULL,
//  class ambLight_t = SimpleTSUint8Sensor /*SensorAmbientLightBase*/, const ambLight_t *ambLightOpt = NULL,
//  class tempC16_t = Sensor<int16_t> /*TemperatureC16Base*/, const tempC16_t *tempC16Opt = NULL,
//  class humidity_t = SimpleTSUint8Sensor /*HumiditySensorBase*/, const humidity_t *humidityOpt = NULL,

#if defined(ARDUINO)
  // For Arduino, default to logging to the (first) hardware Serial device,
  // and starting it up, flushing it, and shutting it down also.
  Print const *outputDevice = Serial,
  const bool wakeFlushSleepSerial = true
#else
  Print const *outputDevice = (Print *)NULL,
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
#if defined(ARDUINO_ARCH_AVR)
            const bool neededWaking = wakeFlushSleepSerial &&
                OTV0P2BASE::powerUpSerialIfDisabled<OTV0P2BASE::V0P2_UART_BAUD_DEFAULT>(); // FIXME
#else
            static_assert(!wakeFlushSleepSerial, "wakeFlushSleepSerial needs hardware serial");
#endif


// TODO



#if defined(ARDUINO_ARCH_AVR)
            // Ensure that all text is sent before this routine returns, in case any sleep/powerdown follows that kills the UART.
            OTV0P2BASE::flushSerialSCTSensitive();

            if(neededWaking) { OTV0P2BASE::powerDownSerial(); }
#endif
            }
    };


}
#endif
