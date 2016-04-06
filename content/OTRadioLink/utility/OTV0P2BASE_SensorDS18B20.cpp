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

Author(s) / Copyright (s): Damon Hart-Davis 2014--2016
                           John Harvey 2014 (initial DS18B20 code)
                           Deniz Erbilgin 2015--2016
                           Jeremy Poulter 2016
*/

/*
 DS18B20 OneWire(TM) temperature detector.
 */


#include "OTV0P2BASE_SensorDS18B20.h"

 // Model IDs
#define DS18S20_MODEL_ID 0x10
#define DS18B20_MODEL_ID 0x28
#define DS1822_MODEL_ID  0x22

 // OneWire commands
#define CMD_START_CONVO       0x44  // Tells device to take a temperature reading and put it on the scratchpad
#define CMD_COPY_SCRATCH      0x48  // Copy EEPROM
#define CMD_READ_SCRATCH      0xBE  // Read EEPROM
#define CMD_WRITE_SCRATCH     0x4E  // Write to EEPROM
#define CMD_RECALL_SCRATCH    0xB8  // Reload from last known
#define CMD_READ_POWER_SUPPLY 0xB4  // Determine if device needs parasite power
#define CMD_ALARM_SEARCH      0xEC  // Query bus for devices with an alarm condition

 // Scratchpad locations
#define LOC_TEMP_LSB          0
#define LOC_TEMP_MSB          1
#define LOC_HIGH_ALARM_TEMP   2
#define LOC_LOW_ALARM_TEMP    3
#define LOC_CONFIGURATION     4
#define LOC_INTERNAL_BYTE     5
#define LOC_COUNT_REMAIN      6
#define LOC_COUNT_PER_C       7
#define LOC_SCRATCHPAD_CRC    8

 // Error Codes
#define DEVICE_DISCONNECTED   -127


namespace OTV0P2BASE
{


// Initialise the device(s) (if any) before first use.
// Returns true iff successful.
// Uses specified order DS18B20 found on bus.
// May need to be reinitialised if precision changed.
bool TemperatureC16_DS18B20::init()
  {
#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("DS18B20 init ");
  DEBUG_SERIAL_PRINTLN();
#endif

  bool found = false;
  uint8_t count = 0;
  uint8_t address[8];

  // Ensure no bad search state.
  minOW.reset_search();

  while(minOW.search(address))
    {
#if 0 && defined(DEBUG)
    // Found a device.
    DEBUG_SERIAL_PRINT_FLASHSTRING("addr:");
    for(int i = 0; i < 8; ++i)
      {
      DEBUG_SERIAL_PRINT(' ');
      DEBUG_SERIAL_PRINTFMT(address[i], HEX);
      }
    DEBUG_SERIAL_PRINTLN();
#endif

    if(DS18B20_MODEL_ID != address[0])
      {
#if 0 && defined(DEBUG)
          DEBUG_SERIAL_PRINTLN_FLASHSTRING("Not a DS18B20, skipping...");
#endif
      continue;
      }

    // Found one and configured it!
    found = true;
    count++;

#if 0 && defined(DEBUG)
    DEBUG_SERIAL_PRINTLN_FLASHSTRING("Setting precision...");
#endif

    minOW.reset();
    // Write scratchpad/config.
    minOW.select(address);
    minOW.write(CMD_WRITE_SCRATCH);
    minOW.write(0); // Th: not used.
    minOW.write(0); // Tl: not used.
    minOW.write(((precision - 9) << 5) | 0x1f); // Config register; lsbs all 1.
    }

#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINTLN_FLASHSTRING("No more devices...");
#endif
  minOW.reset_search(); // Be kind to any other OW search user.

  // Search has been run (whether DS18B20 was found or not).
  initialised = true;

  sensorCount = count;
  return(found);
  }

// Force a read/poll of temperature and return the value sensed in nominal units of 1/16 C.
// At sub-maximum precision lsbits will be zero or undefined.
// Expensive/slow.
// Not thread-safe nor usable within ISRs (Interrupt Service Routines).
int16_t TemperatureC16_DS18B20::read()
  {
  if(1 == readMultiple(&value, 1)) { return(value); }
  value = DEFAULT_INVALID_TEMP; 
  return(DEFAULT_INVALID_TEMP);
  }

// Force a read/poll of temperature from multiple DS18B20 sensors; returns number of values read.
// The value sensed, in nominal units of 1/16 C,
// is written to the array of uint16_t (with count elements) pointed to by values.
// The values are written in the order they are found on the One-Wire bus.
// index specifies the sensor to start reading at 0 being the first (and the default).
// This can be used to read more sensors
// than elements in the values array.
// The return is the number of values read.
// At sub-maximum precision lsbits will be zero or undefined.
// Expensive/slow.
// Not thread-safe nor usable within ISRs (Interrupt Service Routines).
uint8_t TemperatureC16_DS18B20::readMultiple(int16_t *const values, const uint8_t count, uint8_t index)
  {
  if(!initialised) { init(); }
  if(0 == sensorCount) { return(0); }

  uint8_t sensor = 0;

  // Start a temperature reading.
  minOW.reset();
  minOW.skip();
  minOW.write(CMD_START_CONVO); // Start conversion without parasite power.

  // Ensure no bad search state.
  minOW.reset_search();

  uint8_t address[8];
  while(minOW.search(address))
    {
    // Is this a DS18B20?
    if(DS18B20_MODEL_ID != address[0])
      {
#if 0 && defined(DEBUG)
      DEBUG_SERIAL_PRINTLN_FLASHSTRING("Not a DS18B20, skipping...");
#endif
      continue;
      }

    // Have we reached the first sensor we are interested in
    if(index > 0)
      {
      index--;
      continue;
      }

    minOW.reset();
    minOW.select(address);

    // Poll for conversion complete (bus released)...
    // Don't allow indefinite blocking;
    // give up after a second of so returning count of values read before this.
    uint8_t i = 67; // Allow for ~1s as ~15ms per loop.
    while(minOW.read_bit() == 0)
      {
      if(--i == 0) { return(sensor); }
      OTV0P2BASE::nap(WDTO_15MS);
      }

    // Fetch temperature (scratchpad read).
    minOW.reset();
    minOW.select(address);
    minOW.write(CMD_READ_SCRATCH);

    // Read first two bytes of 9 available.  (No CRC config or check.)
    const uint8_t d0 = minOW.read();
    const uint8_t d1 = minOW.read();
    // Terminate read and let DS18B20 go back to sleep.
    minOW.reset();

    // Extract raw temperature, masking any undefined lsbit.
    // TODO: mask out undefined LSBs if precision not maximum.
    const int16_t rawC16 = (d1 << 8) | (d0);

    values[sensor++] = rawC16;

    // Do we have any space left?
    if(sensor >= count) { break; }
    }

  return(sensor);
  }

uint8_t TemperatureC16_DS18B20::getSensorCount()
  {
  if(!initialised) { init(); }
  return(sensorCount);
  }
}
