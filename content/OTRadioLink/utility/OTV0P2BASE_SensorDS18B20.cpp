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
 DS18B20 OneWire(TM) temperature detector.
 */


#include "OTV0P2BASE_SensorDS18B20.h"

//#include <Arduino.h>

//#include "OTV0P2BASE_ADC.h"
//#include "OTV0P2BASE_BasicPinAssignments.h"
//#include "OTV0P2BASE_Entropy.h"
//#include "OTV0P2BASE_PowerManagement.h"
//#include "OTV0P2BASE_Sleep.h"


namespace OTV0P2BASE
{


// Initialise the device (if any) before first use.
// Returns true iff successful.
// Uses specified order DS18B20 found on bus.
// May need to be reinitialised if precision changed.
bool TemperatureC16_DS18B20::init()
  {
//  DEBUG_SERIAL_PRINTLN_FLASHSTRING("DS18B20 init...");
  bool found = false;

  // Ensure no bad search state.
  minOW.reset_search();

  for( ; ; )
    {
    if(!minOW.search(address))
      {
      minOW.reset_search(); // Be kind to any other OW search user.
      break;
      }

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

    if(0x28 != address[0])
      {
#if 0 && defined(DEBUG)
      DEBUG_SERIAL_PRINTLN_FLASHSTRING("Not a DS18B20, skipping...");
#endif
      continue;
      }

#if 0 && defined(DEBUG)
    DEBUG_SERIAL_PRINTLN_FLASHSTRING("Setting precision...");
#endif
    minOW.reset();
    // Write scratchpad/config
    minOW.select(address);
    minOW.write(0x4e);
    minOW.write(0); // Th: not used.
    minOW.write(0); // Tl: not used.
//    MinOW.write(DS1820_PRECISION | 0x1f); // Config register; lsbs all 1.
    minOW.write(((precision - 9) << 6) | 0x1f); // Config register; lsbs all 1.

    // Found one and configured it!
    found = true;
    }

  // Search has been run (whether DS18B20 was found or not).
  initialised = true;

  if(!found)
    {
    DEBUG_SERIAL_PRINTLN_FLASHSTRING("DS18B20 not found");
    address[0] = 0; // Indicate that no DS18B20 was found.
    }
  return(found);
  }

// Force a read/poll of temperature and return the value sensed in nominal units of 1/16 C.
// At sub-maximum precision lsbits will be zero or undefined.
// Expensive/slow.
// Not thread-safe nor usable within ISRs (Interrupt Service Routines).
int16_t TemperatureC16_DS18B20::read()
  {
  if(!initialised) { init(); }
  if(0 == address[0]) { value = INVALID_TEMP; return(INVALID_TEMP); }

  // Start a temperature reading.
  minOW.reset();
  minOW.select(address);
  minOW.write(0x44); // Start conversion without parasite power.
  //delay(750); // 750ms should be enough.
  // Poll for conversion complete (bus released)...
  while(minOW.read_bit() == 0) { OTV0P2BASE::nap(WDTO_15MS); }

  // Fetch temperature (scratchpad read).
  minOW.reset();
  minOW.select(address);
  minOW.write(0xbe);
  // Read first two bytes of 9 available.  (No CRC config or check.)
  const uint8_t d0 = minOW.read();
  const uint8_t d1 = minOW.read();
  // Terminate read and let DS18B20 go back to sleep.
  minOW.reset();

  // Extract raw temperature, masking any undefined lsbit.
  // TODO: mask out undefined LSBs if precision not maximum.
  const int16_t rawC16 = (d1 << 8) | (d0);

  return(rawC16);
  }


}
