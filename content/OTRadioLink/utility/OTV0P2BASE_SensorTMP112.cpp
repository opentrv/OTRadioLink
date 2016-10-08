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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2016
*/

/*
 TMP112 temperature sensor.

 V0p2/AVR specific for now.
 */


#include "OTV0P2BASE_SensorTMP112.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h> // Arduino I2C library.
#endif

#include "OTV0P2BASE_Entropy.h"
#include "OTV0P2BASE_PowerManagement.h"
#include "OTV0P2BASE_Sleep.h"


namespace OTV0P2BASE
{


#ifdef RoomTemperatureC16_TMP112_DEFINED

// TMP102 and TMP112 should be interchangeable: latter has better guaranteed accuracy.
static const uint8_t TMP112_I2C_ADDR = 72;
static const uint8_t TMP112_REG_TEMP = 0; // Temperature register.
static const uint8_t TMP112_REG_CTRL = 1; // Control register.
static const uint8_t TMP112_CTRL_B1 = 0x31; // Byte 1 for control register: 12-bit resolution and shutdown mode (SD).
static const uint8_t TMP112_CTRL_B1_OS = 0x80; // Control register: one-shot flag in byte 1.
static const uint8_t TMP112_CTRL_B2 = 0x0; // Byte 2 for control register: 0.25Hz conversion rate and not extended mode (EM).

// Measure/store/return the current room ambient temperature in units of 1/16th C.
// This may contain up to 4 bits of information to the right of the fixed binary point.
// This may consume significant power and time.
// Probably no need to do this more than (say) once per minute.
// The first read will initialise the device as necessary and leave it in a low-power mode afterwards.
// This will simulate a zero temperature in case of detected error talking to the sensor as fail-safe for this use.
// Check for errors at certain critical places, not everywhere.
int16_t RoomTemperatureC16_TMP112::read()
  {
  const bool neededPowerUp = OTV0P2BASE::powerUpTWIIfDisabled();

#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("TMP112 needed power-up: ");
  DEBUG_SERIAL_PRINT(neededPowerUp);
  DEBUG_SERIAL_PRINTLN();
#endif

  // Force start of new one-shot temperature measurement/conversion to complete.
  Wire.beginTransmission(TMP112_I2C_ADDR);
  Wire.write((byte) TMP112_REG_CTRL); // Select control register.
  Wire.write((byte) TMP112_CTRL_B1); // Clear OS bit.
  //Wire.write((byte) TMP112_CTRL_B2);
  Wire.endTransmission();
  Wire.beginTransmission(TMP112_I2C_ADDR);
  Wire.write((byte) TMP112_REG_CTRL); // Select control register.
  Wire.write((byte) TMP112_CTRL_B1 | TMP112_CTRL_B1_OS); // Start one-shot conversion.
  //Wire.write((byte) TMP112_CTRL_B2);
  if(Wire.endTransmission()) { return(DEFAULT_INVALID_TEMP); } // Exit if error.

  // Wait for temperature measurement/conversion to complete, in low-power sleep mode for the bulk of the time.
#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINTLN_FLASHSTRING("TMP112 waiting for conversion...");
#endif
  Wire.beginTransmission(TMP112_I2C_ADDR);
  Wire.write((byte) TMP112_REG_CTRL); // Select control register.
  if(Wire.endTransmission()) { return(DEFAULT_INVALID_TEMP); } // Exit if error.
  for(int i = 8; --i; ) // 2 orbits should generally be plenty.
    {
    if(i <= 0) { return(DEFAULT_INVALID_TEMP); } // Exit if error.
    if(Wire.requestFrom(TMP112_I2C_ADDR, 1U) != 1) { return(DEFAULT_INVALID_TEMP); } // Exit if error.
    const byte b1 = Wire.read();
    if(b1 & TMP112_CTRL_B1_OS) { break; } // Conversion completed.
    OTV0P2BASE::nap(WDTO_15MS); // One or two of these naps should allow typical ~26ms conversion to complete...
    }

  // Fetch temperature.
#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINTLN_FLASHSTRING("TMP112 fetching temperature...");
#endif
  Wire.beginTransmission(TMP112_I2C_ADDR);
  Wire.write((byte) TMP112_REG_TEMP); // Select temperature register (set ptr to 0).
  if(Wire.endTransmission()) { return(DEFAULT_INVALID_TEMP); } // Exit if error.
  if(Wire.requestFrom(TMP112_I2C_ADDR, 2U) != 2)  { return(DEFAULT_INVALID_TEMP); }
  if(Wire.endTransmission()) { return(DEFAULT_INVALID_TEMP); } // Exit if error.

  const byte b1 = Wire.read(); // MSByte, should be signed whole degrees C.
  const uint8_t b2 = Wire.read(); // Avoid sign extension...

  if(neededPowerUp) { OTV0P2BASE::powerDownTWI(); }

  // Builds 12-bit value (assumes not in extended mode) and sign-extends if necessary for sub-zero temps.
  const int t16 = (b1 << 4) | (b2 >> 4) | ((b1 & 0x80) ? 0xf000 : 0);

#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("TMP112 temp: ");
  DEBUG_SERIAL_PRINT(b1);
  DEBUG_SERIAL_PRINT_FLASHSTRING("C / ");
  DEBUG_SERIAL_PRINT(temp16);
  DEBUG_SERIAL_PRINTLN();
#endif

  // Capture entropy if (transformed) value has changed.
  if((uint8_t)t16 != (uint8_t)value) { ::OTV0P2BASE::addEntropyToPool(b1 ^ b2, 0); } // Claim zero entropy as may be forced by Eve.

  value = t16;
  return(t16);
  }

#endif // RoomTemperatureC16_TMP112_DEFINED

}
