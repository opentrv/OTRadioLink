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
 SHT21 temperature and relative humidity sensor.

 V0p2/AVR specific for now.
 */


#include "OTV0P2BASE_SensorSHT21.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h> // Arduino I2C library.
#endif

#include "OTV0P2BASE_Entropy.h"
#include "OTV0P2BASE_PowerManagement.h"
#include "OTV0P2BASE_Sleep.h"


namespace OTV0P2BASE
{


#if defined(RoomTemperatureC16_SHT21_DEFINED) || defined(HumiditySensorSHT21_DEFINED)

static const uint8_t SHT21_I2C_ADDR = 0x40;
static const uint8_t SHT21_I2C_CMD_TEMP_HOLD   = 0xe3;
static const uint8_t SHT21_I2C_CMD_TEMP_NOHOLD = 0xf3;
static const uint8_t SHT21_I2C_CMD_RH_HOLD     = 0xe5;
static const uint8_t SHT21_I2C_CMD_RH_NOHOLD   = 0xf5;
static const uint8_t SHT21_I2C_CMD_USERREG     = 0xe7; // User register...

// If defined, sample 8-bit RH (for for 1%) and 12-bit temp (for 1/16C).
// This should save time and energy.
static const bool SHT21_USE_REDUCED_PRECISION = true;

// Set true once SHT21 has been initialised.
static volatile bool SHT21_initialised;

// Initialise/configure SHT21, usually once only.
// TWI must already be powered up.
static void SHT21_init()
  {
  if(SHT21_USE_REDUCED_PRECISION)
    {
    // Soft reset in order to sample at reduced precision.
    Wire.beginTransmission(SHT21_I2C_ADDR);
    Wire.write((byte) SHT21_I2C_CMD_USERREG); // Select control register.
    Wire.endTransmission();
    Wire.requestFrom(SHT21_I2C_ADDR, 1U);
    while(Wire.available() < 1)
      {
      // Wait for data, but avoid rolling over the end of a minor cycle...
      if(OTV0P2BASE::getSubCycleTime() >= OTV0P2BASE::GSCT_MAX - 2)
        { return; } // Failed, and not initialised.
      }
    const uint8_t curUR = Wire.read();
    //  DEBUG_SERIAL_PRINT_FLASHSTRING("UR: ");
    //  DEBUG_SERIAL_PRINTFMT(curUR, HEX);
    //  DEBUG_SERIAL_PRINTLN();

    // Preserve reserved bits (3, 4, 5) and sample 8-bit RH (for for 1%) and 12-bit temp (for 1/16C).
    const uint8_t newUR = (curUR & 0x38) | 3;
    Wire.beginTransmission(SHT21_I2C_ADDR);
    Wire.write((byte) SHT21_I2C_CMD_USERREG); // Select control register.
    Wire.write((byte) newUR);
    Wire.endTransmission();
    }
  SHT21_initialised = true;
  }
#endif // defined(RoomTemperatureC16_SHT21_DEFINED) || defined(HumiditySensorSHT21_DEFINED)


#ifdef RoomTemperatureC16_SHT21_DEFINED
// Measure and return the current ambient temperature in units of 1/16th C.
// This may contain up to 4 bits of information to the right of the fixed binary point.
// This may consume significant power and time.
// Probably no need to do this more than (say) once per minute.
// The first read will initialise the device as necessary and leave it in a low-power mode afterwards.
int RoomTemperatureC16_SHT21::read()
  {
  const bool neededPowerUp = OTV0P2BASE::powerUpTWIIfDisabled();

  // Initialise/config if necessary.
  if(!SHT21_initialised) { SHT21_init(); }

  // Max RH measurement time:
  //   * 14-bit: 85ms
  //   * 12-bit: 22ms
  //   * 11-bit: 11ms
  // Use blocking data fetch for now.
  Wire.beginTransmission(SHT21_I2C_ADDR);
  Wire.write((byte) SHT21_I2C_CMD_TEMP_HOLD); // Select control register.
#if defined(SHT21_USE_REDUCED_PRECISION)
  OTV0P2BASE::nap(WDTO_30MS); // Should cover 12-bit conversion (22ms).
#else
  OTV0P2BASE::sleepLowPowerMs(90); // Should be plenty for slowest (14-bit) conversion (85ms).
#endif
  //delay(100);
  Wire.endTransmission();
  Wire.requestFrom(SHT21_I2C_ADDR, 3U);
  while(Wire.available() < 3)
    {
    // Wait for data, but avoid rolling over the end of a minor cycle...
    if(OTV0P2BASE::getSubCycleTime() >= OTV0P2BASE::GSCT_MAX - 2)
      { return(DEFAULT_INVALID_TEMP); } // Failure value: may be able to to better.
    }
  uint16_t rawTemp = (Wire.read() << 8);
  rawTemp |= (Wire.read() & 0xfc); // Clear status ls bits.

  // Power down TWI ASAP.
  if(neededPowerUp) { OTV0P2BASE::powerDownTWI(); }

  // Nominal formula: C = -46.85 + ((175.72*raw) / (1L << 16));
  const int c16 = -750 + ((5623L * rawTemp) >> 17); // FIXME: find a faster approximation...

  // Capture entropy if (transformed) value has changed.
  if((uint8_t)c16 != (uint8_t)value) { ::OTV0P2BASE::addEntropyToPool((uint8_t)rawTemp, 0); } // Claim zero entropy as may be forced by Eve.

  value = c16;
  return(c16);
  }
#endif // RoomTemperatureC16_SHT21_DEFINED

#ifdef HumiditySensorSHT21_DEFINED
// Measure and return the current relative humidity in %; range [0,100] and 255 for error.
// This may consume significant power and time.
// Probably no need to do this more than (say) once per minute.
// The first read will initialise the device as necessary and leave it in a low-power mode afterwards.
// Returns 255 (~0) in case of error.
uint8_t HumiditySensorSHT21::read()
  {
  const bool neededPowerUp = OTV0P2BASE::powerUpTWIIfDisabled();

  // Initialise/config if necessary.
  if(!SHT21_initialised) { SHT21_init(); }

  // Get RH%...
  // Max RH measurement time:
  //   * 12-bit: 29ms
  //   *  8-bit:  4ms
  // Use blocking data fetch for now.
  Wire.beginTransmission(SHT21_I2C_ADDR);
  Wire.write((byte) SHT21_I2C_CMD_RH_HOLD); // Select control register.
#if defined(SHT21_USE_REDUCED_PRECISION)
  OTV0P2BASE::sleepLowPowerMs(5); // Should cover 8-bit conversion (4ms).
#else
  OTV0P2BASE::nap(WDTO_30MS); // Should cover even 12-bit conversion (29ms).
#endif
  Wire.endTransmission();
  Wire.requestFrom(SHT21_I2C_ADDR, 3U);
  while(Wire.available() < 3)
    {
    // Wait for data, but avoid rolling over the end of a minor cycle...
    if(OTV0P2BASE::getSubCycleTime() >= OTV0P2BASE::GSCT_MAX)
      {
      //      DEBUG_SERIAL_PRINTLN_FLASHSTRING("giving up");
      return(~0);
      }
    }
  const uint8_t rawRH = Wire.read();
  const uint8_t rawRL = Wire.read();

  // Power down TWI ASAP.
  if(neededPowerUp) { OTV0P2BASE::powerDownTWI(); }

  const uint16_t raw = (((uint16_t)rawRH) << 8) | (rawRL & 0xfc); // Clear status ls bits.
  const uint8_t result = -6 + ((125L * raw) >> 16);

  // Capture entropy from raw status bits iff (transformed) reading has changed.
  if(value != result) { OTV0P2BASE::addEntropyToPool(rawRL ^ rawRH, 0); } // Claim zero entropy as may be forced by Eve.

  value = result;
  if(result > (HUMIDTY_HIGH_RHPC + HUMIDITY_EPSILON_RHPC)) { highWithHyst = true; }
  else if(result < (HUMIDTY_HIGH_RHPC - HUMIDITY_EPSILON_RHPC)) { highWithHyst = false; }
  return(result);
  }
#endif // RoomTemperatureC16_SHT21_DEFINED


}
