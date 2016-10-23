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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2016
*/

#ifndef ARDUINO_LIB_OTV0P2BASE_H
#define ARDUINO_LIB_OTV0P2BASE_H

#define ARDUINO_LIB_OTV0P2BASE_VERSION_MAJOR 1
#define ARDUINO_LIB_OTV0P2BASE_VERSION_MINOR 0

// Base hardware, power and system management for V0p2 AVR-based (ATMega328P) OpenTRV boards.

#define V0P2BASE_DEBUG

/*
  V0p2 (V0.2) core.

  DHD20130417: hardware setup on bare board.
    * 1MHz CPU clock (from 8MHz internal RC clock with /8 prescaler) ATmega328P running at 1.8V--5V (typically 2V--3.3V).
    * Fuse set for BOD-managed additional clock settle time, ie as fast a restart from sleep as possible.
    * All unused pins unconnected and nominally floating (though driven low as output where possible).
    * 32768Hz xtal between pins XTAL1 and XTAL2, async timer 2, for accurate timekeeping and low-power sleep.
    * All unused system modules turned off.

  Basic AVR power consumption ticking an (empty) control loop at ~0.5Hz should be ~1uA.
 */

// Basic compatibility support for Arduino and non-Arduino environments.
#include "utility/OTV0P2BASE_ArduinoCompat.h"

// Portable concurrency/atomicity support that should work for small MCUs and bigger platforms.
#include "utility/OTV0P2BASE_Concurrency.h"

// Hardware tests, eg as used in POST (Power On Self Test).
#include "utility/OTV0P2BASE_HardwareTests.h"

// Some basic utility functions and definitions.
#include "utility/OTV0P2BASE_Util.h"

// EEPROM space allocation and utilities including some of the simple rolling stats management.
#include "utility/OTV0P2BASE_EEPROM.h"

// Quick/simple PRNG (Pseudo-Random Number Generator).
#include "utility/OTV0P2BASE_QuickPRNG.h"

// Fast GPIO support routines.
#include "utility/OTV0P2BASE_FastDigitalIO.h"

// Minimal light-weight standard-speed OneWire(TM) support.
#include "utility/OTV0P2BASE_MinOW.h"

// Base/common sensor and actuator types.
#include "utility/OTV0P2BASE_Sensor.h"
#include "utility/OTV0P2BASE_Actuator.h"

// Concrete sensor implementations.
#include "utility/OTV0P2BASE_SensorAmbientLight.h"
#include "utility/OTV0P2BASE_SensorAmbientLightOccupancy.h"
#include "utility/OTV0P2BASE_SensorTemperaturePot.h"
#include "utility/OTV0P2BASE_SensorTemperatureC16Base.h"
#include "utility/OTV0P2BASE_SensorTMP112.h"
#include "utility/OTV0P2BASE_SensorSHT21.h"
#include "utility/OTV0P2BASE_SensorDS18B20.h"
#include "utility/OTV0P2BASE_SensorQM1.h"
#include "utility/OTV0P2BASE_SensorOccupancy.h"

// Basic immutable GPIO assignments and similar.
#include "utility/OTV0P2BASE_BasicPinAssignments.h"

// Power, micro timing, I/O management and other misc support.
#include "utility/OTV0P2BASE_Sleep.h"
#include "utility/OTV0P2BASE_PowerManagement.h"

// Software Real-Time Clock (RTC) support.
#include "utility/OTV0P2BASE_RTC.h"

// Simple valve programmer/scheduler.
#include "utility/OTV0P2BASE_SimpleValveSchedule.h"

// ADC (Analogue-to-Digital Converter) support.
#include "utility/OTV0P2BASE_ADC.h"

// Basic security support.
#include "utility/OTV0P2BASE_Security.h"

// Entropy management.
#include "utility/OTV0P2BASE_Entropy.h"

// Serial IO (hardware Serial + debug support).
#include "utility/OTV0P2BASE_Serial_IO.h"
// Soft Serial.
#include "utility/OTV0P2BASE_SoftSerial.h"
#include "utility/OTV0P2BASE_SoftSerial2.h"

// Specialist simple CRC support.
#include "utility/OTV0P2BASE_CRC.h"

// Support for JSON stats.
#include "utility/OTV0P2BASE_JSONStats.h"
// Support for older/simple compact binary stats.
#include "utility/OTV0P2BASE_SimpleBinaryStats.h"

// Common CLI utilities
#include "utility/OTV0P2BASE_CLI.h"

#endif
