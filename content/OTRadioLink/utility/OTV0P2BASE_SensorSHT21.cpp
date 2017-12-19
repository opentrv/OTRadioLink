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

#ifdef EFR32FG1P133F256GM48
#include "i2c_driver.h"
#endif // EFR32FG1P133F256GM48


namespace OTV0P2BASE
{

#if defined(RoomTemperatureC16_SHT21_DEFINED) || defined(HumiditySensorSHT21_DEFINED)

static constexpr uint8_t SHT21_I2C_ADDR = 0x40;
static constexpr uint8_t SHT21_I2C_CMD_TEMP_HOLD   = 0xe3;
static constexpr uint8_t SHT21_I2C_CMD_TEMP_NOHOLD = 0xf3;
static constexpr uint8_t SHT21_I2C_CMD_RH_HOLD     = 0xe5;
static constexpr uint8_t SHT21_I2C_CMD_RH_NOHOLD   = 0xf5;
static constexpr uint8_t SHT21_I2C_CMD_USERREG     = 0xe7; // User register...

// If defined, sample 8-bit RH (for 1%) and 12-bit temp (for 1/16C).
// Reduced precision should save time and energy.
static constexpr bool SHT21_USE_REDUCED_PRECISION = true;

// Set true once SHT21 has been initialised.
static volatile bool SHT21_initialised;


#if defined(ARDUINO_ARCH_AVR)
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
#elif defined(EFR32FG1P133F256GM48)
// Initialise/configure SHT21, usually once only.
// TWI must already be powered up.
static void SHT21_init()
{
    if(SHT21_USE_REDUCED_PRECISION) {
        // Soft reset in order to sample at reduced precision.
        uint8_t cmd[1] = { SHT21_I2C_CMD_USERREG };
        uint8_t rxMsg[1] = { 0 };
        i2c0.read(SHT21_I2C_ADDR, cmd, sizeof(cmd), rxMsg, sizeof(rxMsg));
        const uint8_t curUR = rxMsg[0];

        // Preserve reserved bits (3, 4, 5) and sample 8-bit RH (for for 1%) and 12-bit temp (for 1/16C).
        const uint8_t newUR = (curUR & 0x38) | 3;
        uint8_t configMsg[2] = { (SHT21_I2C_CMD_USERREG & 0xfe) , newUR};
        i2c0.write(SHT21_I2C_ADDR, configMsg, sizeof(configMsg)); // setupMsg
    }
    SHT21_initialised = true;
}
#endif
#endif // defined(RoomTemperatureC16_SHT21_DEFINED) || defined(HumiditySensorSHT21_DEFINED)


#ifdef RoomTemperatureC16_SHT21_DEFINED

// Abstracting read function I2C transactions
namespace {
#ifdef ARDUINO_ARCH_AVR
inline uint16_t i2cTempRead(bool neededPowerUp)
{
    // Max RH measurement time:
    //   * 14-bit: 85ms
    //   * 12-bit: 22ms
    //   * 11-bit: 11ms
    // Use blocking data fetch for now.
    Wire.beginTransmission(SHT21_I2C_ADDR);
    Wire.write((byte) SHT21_I2C_CMD_TEMP_HOLD); // Select control register.
    if(SHT21_USE_REDUCED_PRECISION) {
        // Should cover 12-bit conversion (22ms).
        OTV0P2BASE::nap(WDTO_30MS);
    } else {
        // Should be plenty for slowest (14-bit) conversion (85ms).
        OTV0P2BASE::sleepLowPowerMs(90);
    }
    Wire.endTransmission();
    Wire.requestFrom(SHT21_I2C_ADDR, 3U);
    while(Wire.available() < 3) {
        // Wait for data, but avoid rolling over the end of a minor cycle...
        if(OTV0P2BASE::getSubCycleTime() >= OTV0P2BASE::GSCT_MAX - 2) {
            return(DEFAULT_INVALID_TEMP);  // Failure value: may be able to to better.
        }
    }
    uint16_t rawTemp = (Wire.read() << 8);
    rawTemp |= (Wire.read() & 0xfc); // Clear status ls bits.

    // Power down TWI ASAP.
    if(neededPowerUp) { OTV0P2BASE::powerDownTWI(); }

    return (rawTemp);
}
#elif defined(EFR32FG1P133F256GM48)
// TODO Do stuff while reading, check for reaching the end of a sub cycle.
inline uint16_t readTemp(bool neededPowerUp)
{
    // Max RH measurement time:
    //   * 14-bit: 85ms
    //   * 12-bit: 22ms
    //   * 11-bit: 11ms
    // Use blocking data fetch for now.
    uint8_t cmd_temp_hold[1] = { SHT21_I2C_CMD_TEMP_HOLD };
    uint8_t rxMsg[2] = {0, 0};
    i2c0.read(SHT21_I2C_ADDR, cmd_temp_hold, sizeof(cmd_temp_hold), rxMsg, sizeof(rxMsg));
    #if 0  // This was in between the split write/read model on the arduino.
    if(SHT21_USE_REDUCED_PRECISION)
    // Should cover 12-bit conversion (22ms).
    { OTV0P2BASE::nap(WDTO_30MS); }
    else
    // Should be plenty for slowest (14-bit) conversion (85ms).
    { OTV0P2BASE::sleepLowPowerMs(90); }
    #endif

    #if 0  // This was the polling bit on the arduino.
    while(Wire.available() < 3)
    {
      // Wait for data, but avoid rolling over the end of a minor cycle...
    if(OTV0P2BASE::getSubCycleTime() >= OTV0P2BASE::GSCT_MAX - 2)
      { return(DEFAULT_INVALID_TEMP); } // Failure value: may be able to to better.
    }
    #endif
    uint_fast16_t rawTemp = (rxMsg[0] << 8) | (rxMsg[1] & 0xfc);

    return (rawTemp);
}
#endif
}

// Measure and return the current ambient temperature in units of 1/16th C.
// This may contain up to 4 bits of information to RHS of the fixed binary point.
// This may consume significant power and time.
// Probably no need to do this more than (say) once per minute.
// The first read will initialise the device as necessary
// and leave it in a low-power mode afterwards.
int16_t RoomTemperatureC16_SHT21::read()  // XXX
  {
  const bool neededPowerUp = OTV0P2BASE::powerUpTWIIfDisabled();

  // Initialise/config if necessary.
  if(!SHT21_initialised) { SHT21_init(); }

  const uint_fast16_t rawTemp = readTemp(neededPowerUp);

  // Nominal formula: C = -46.85 + ((175.72*raw) / (1L << 16));
  // FIXME: find a good but faster approximation...
  // FIXME: should the shift/division be rounded to nearest?
  // FIXME: break out calculation and unit test against example in datasheet.
  const int_fast16_t c16 = -750 + int_fast16_t((5623 * int_fast32_t(rawTemp)) >> 17);

  // Capture entropy if (transformed) value has changed.
  // Claim one bit of noise in the raw value if the full value has changed,
  // though it is possible that this might be manipulatable by Eve,
  // and nearly all of the raw info is visible in the result.
//   if(c16 != value) { addEntropyToPool((uint8_t)rawTemp, 1); }  // XXX

  value = c16;
  return(c16);
  }
#endif // RoomTemperatureC16_SHT21_DEFINED

#ifdef HumiditySensorSHT21_DEFINED

namespace {
// Abstract for different i2c drivers.
#ifdef ARDUINO_ARCH_AVR
inline uint16_t readRH(const bool neededPowerUp) {
    // Get RH%...
    // Max RH measurement time:
    //   * 12-bit: 29ms
    //   *  8-bit:  4ms
    // Use blocking data fetch for now.
    Wire.beginTransmission(SHT21_I2C_ADDR);
    Wire.write((byte) SHT21_I2C_CMD_RH_HOLD); // Select control register.
    if(SHT21_USE_REDUCED_PRECISION) {
    // Should cover 8-bit conversion (4ms).
        OTV0P2BASE::sleepLowPowerMs(5);
    } else {
    // Should cover even 12-bit conversion (29ms).
        OTV0P2BASE::nap(WDTO_30MS);
    }
    Wire.endTransmission();
    Wire.requestFrom(SHT21_I2C_ADDR, 3U);
    while(Wire.available() < 3) {
        // Wait for data, but avoid rolling over the end of a minor cycle...
        if(OTV0P2BASE::getSubCycleTime() >= OTV0P2BASE::GSCT_MAX - 2) {
            // DEBUG_SERIAL_PRINTLN_FLASHSTRING("giving up");
            return(~0);
        }
    }
    const uint8_t rawRH = Wire.read();
    const uint8_t rawRL = Wire.read();

    // Power down TWI ASAP.
    if(neededPowerUp) { OTV0P2BASE::powerDownTWI(); }

    // Assemble raw value, clearing status ls bits.
    const uint16_t raw = (((uint16_t)rawRH) << 8) | (rawRL & 0xfc);
    return (rawRH);
}
#elif defined(EFR32FG1P133F256GM48)
inline uint16_t readRH(const bool neededPowerUp) {
    // Max RH measurement time:
    //   * 14-bit: 85ms
    //   * 12-bit: 22ms
    //   * 11-bit: 11ms
    // Use blocking data fetch for now.
    uint8_t cmd_temp_hold[1] = { SHT21_I2C_CMD_RH_HOLD };
    uint8_t rxMsg[2] = {0, 0};
    i2c0.read(SHT21_I2C_ADDR, cmd_temp_hold, sizeof(cmd_temp_hold), rxMsg, sizeof(rxMsg));
    #if 0  // This was in between the split write/read model on the arduino.
    if(SHT21_USE_REDUCED_PRECISION)
    // Should cover 12-bit conversion (22ms).
    { OTV0P2BASE::nap(WDTO_30MS); }
    else
    // Should be plenty for slowest (14-bit) conversion (85ms).
    { OTV0P2BASE::sleepLowPowerMs(90); }
    #endif

    #if 0  // This was the polling bit on the arduino.
    while(Wire.available() < 3)
    {
      // Wait for data, but avoid rolling over the end of a minor cycle...
    if(OTV0P2BASE::getSubCycleTime() >= OTV0P2BASE::GSCT_MAX - 2)
      { return(DEFAULT_INVALID_TEMP); } // Failure value: may be able to to better.
    }
    #endif
    const uint_fast16_t rawRH = (rxMsg[0] << 8) | (rxMsg[1] & 0xfc);

    return (rawRH);
}
#endif
}

// Measure and return the current relative humidity in %; range [0,100] and 255 for error.
// This may consume significant power and time.
// Probably no need to do this more than (say) once per minute.
// The first read will initialise the device as necessary and leave it in a low-power mode afterwards.
// Returns 255 (~0) in case of error.
uint8_t HumiditySensorSHT21::read()  // XXX
  {
  const bool neededPowerUp = OTV0P2BASE::powerUpTWIIfDisabled();

  // Initialise/config if necessary.
  if(!SHT21_initialised) { SHT21_init(); }

  const uint_fast16_t rawRH = readRH(neededPowerUp);

  // FIXME: should the shift/division be rounded to nearest?
  // FIXME: break out calculation and unit test against example in datasheet.
  const uint8_t result = uint8_t(-6 + ((125 * uint_fast32_t(rawRH)) >> 16));

  // Capture entropy from raw status bits iff (transformed) reading has changed.
  // Claim no entropy since only a fraction of a bit is not in the result.
//   if(value != result) { OTV0P2BASE::addEntropyToPool(rawRL ^ rawRH, 0); } // XXX

  value = result;
  if(result > (HUMIDTY_HIGH_RHPC + HUMIDITY_EPSILON_RHPC)) { highWithHyst = true; }
  else if(result < (HUMIDTY_HIGH_RHPC - HUMIDITY_EPSILON_RHPC)) { highWithHyst = false; }
  return(result);
  }
#endif // RoomTemperatureC16_SHT21_DEFINED


}
