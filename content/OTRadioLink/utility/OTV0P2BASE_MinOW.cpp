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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2015
Note: original XABC code from "EternityForest" appears to be in the public domain.
*/

/*
 Minimal light-weight standard-speed OneWire(TM) support.

 Only supported on V0p2/AVR currently.
 */

#ifdef ARDUINO_ARCH_AVR // Only supported on V0p2/AVR currently.

#include <util/atomic.h>

#include "OTV0P2BASE_MinOW.h"


namespace OTV0P2BASE
{


// Create very light-weight standard-speed OneWire(TM) support if a pin has been allocated to it.
// Meant to be similar to use to OneWire library V2.2.
// Supports search but not necessarily CRC.
// Designed to work with 1MHz/1MIPS CPU clock.

// See: http://www.pjrc.com/teensy/td_libs_OneWire.html
// See: http://www.tweaking4all.com/hardware/arduino/arduino-ds18b20-temperature-sensor/
//
// Note also that the 1MHz CPU clock causes timing problems in the OneWire library with delayMicroseconds().
// See: http://forum.arduino.cc/index.php?topic=217004.0
// See: http://forum.arduino.cc/index.php?topic=46696.0

// Reset interface; returns false if no slave device present.
// Reset the 1-Wire bus slave devices and ready them for a command.
// Delay G (0); drive bus low, delay H (48); release bus, delay I (70); sample bus, 0 = device(s) present, 1 = no device present; delay J (410).
// Timing intervals quite long so slightly slower impl here in base class is OK.
bool MinimalOneWireBase::reset()
  {
  bool result = false;

  // Locks out all interrupts until the final recovery delay to keep timing as accurate as possible,
  // restoring them to their original state when done.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
    // Delay G (0).
    delayG();
    // Drive bus/DQ low.
    bitWriteLow(inputReg, regMask);
    bitModeOutput(inputReg, regMask);
    // Delay H.
    delayH();
    // Release the bus (ie let it float).
    bitModeInput(inputReg, regMask);
    // Delay I.
    delayI();
    // Sample for presence pulse from slave; low signal means slave present.
    result = !bitReadIn(inputReg, regMask);
    }
  // Delay J.
  // Complete the reset sequence recovery.
  // Timing is not critical here so interrupts are allowed in again...
  delayJ();

#if 0 && defined(V0P2BASE_DEBUG)
  V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("OW reset");
  if(result) { V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING(" found slave(s)"); }
  V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif

  return(result);
  }

// Read a byte.
// Read least-significant-bit first.
uint8_t MinimalOneWireBase::read()
  {
  uint8_t result = 0;
  for(uint8_t i = 0; i < 8; ++i)
    {
    result >>= 1;
    if(read_bit()) { result |= 0x80; } else { result &= 0x7f; }
    }
  return(result);
  }

// Write a byte leaving the bus unpowered at the end.
// Write least-significant-bit first.
void MinimalOneWireBase::write(uint8_t v)
  {
  for(uint8_t i = 0; i < 8; ++i)
    {
    write_bit(0 != (v & 1));
    v >>= 1;
    }
  }

void MinimalOneWireBase::reset_search()
  {
  lastDeviceFlag = false;
  lastDiscrepancy = 0;
  for(int i = sizeof(addr); --i >= 0; ) { addr[i] = 0; }
  }

// Search for the next device.
// Returns true if a new address has been found;
// false means no devices or all devices already found or bus shorted.
// This does not check the CRC.
// Follows the broad algorithm shown in http://www.maximintegrated.com/en/app-notes/index.mvp/id/187
bool MinimalOneWireBase::search(uint8_t newAddr[])
  {
  bool result = false;

  // If not at last device, reset and start again.
  if(!lastDeviceFlag)
    {
    uint8_t addrByteNumber = 0;
    uint8_t addrByteMask = 1;
    uint8_t idBitNumber = 1;
    uint8_t lastZero = 0;

    // 1-Wire reset
    if(!reset())
      {
      // Reset search state other than addr.
      lastDeviceFlag = false;
      lastDiscrepancy = 0;
      return(false); // No slave devices on bus...
      }

    // Send search command.
    write(0xf0);

    // Start the search loop.
    do
      {
      // Read bit and the complement.
      const bool idBit = read_bit();
      const bool cmplIDBit = read_bit();
#if 0 && defined(V0P2BASE_DEBUG)
      V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("Search read bit&compl: ");
      V0P2BASE_DEBUG_SERIAL_PRINT(idBit);
      V0P2BASE_DEBUG_SERIAL_PRINT(cmplIDBit);
      V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif

      // Stop if no slave devices on the bus.
      if(idBit && cmplIDBit) { break; }

      bool searchDirection;

      // If all active (non-waiting) slaves have the same next address bit
      // then that bit becomes the search direction.
      if(idBit != cmplIDBit) { searchDirection = idBit; }
      else
        {
        if(idBitNumber < lastDiscrepancy)
          { searchDirection = (0 != (addr[addrByteNumber] & addrByteMask)); }
        else
          { searchDirection = (idBitNumber == lastDiscrepancy); }

        // If direction is false/0 then remember its position in lastZero.
        if(false == searchDirection) { lastZero = idBitNumber; }
        }

      // Set/clear addr bit as appropriate.
      if(searchDirection) { addr[addrByteNumber] |= addrByteMask; }
      else { addr[addrByteNumber] &= ~addrByteMask; }

      // Adjust the mask, etc.
      ++idBitNumber;
      if(0 == (addrByteMask <<= 1))
        {
#if 0 && defined(V0P2BASE_DEBUG)
        V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("Addr byte: ");
        V0P2BASE_DEBUG_SERIAL_PRINTFMT(addr[addrByteNumber], HEX);
        V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif
        addrByteMask = 1;
        ++addrByteNumber;
        }

      // Send the next search bit...
#if 0 && defined(V0P2BASE_DEBUG)
      V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("Search write: ");
      V0P2BASE_DEBUG_SERIAL_PRINT(searchDirection);
      V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif
      write_bit(searchDirection);
      } while(addrByteNumber < 8); // Collect all address bytes!

    if(idBitNumber == 65)
      {
      // Success!
      lastDiscrepancy = lastZero;
      if(0 == lastZero) { lastDeviceFlag = true; }
      result = true;
      }
    }

  if(!result || (0 == addr[0]))
    {
    // No device found, so reset to be like first!
    lastDeviceFlag = false;
    lastDiscrepancy = 0;
    result = false;
    }

  for(uint8_t i = 0; i < 8; ++i) { newAddr[i] = addr[i]; }
  return(result);
  }

// Select a particular device on the bus.
void MinimalOneWireBase::select(const uint8_t addr[8])
  {
  write(0x55);
  for(uint8_t i = 0; i < 8; ++i) { write(addr[i]); }
  }

// Do a ROM skip.
void MinimalOneWireBase::skip()
  {
  write(0xCC); // Skip ROM
  }


}

#endif // ARDUINO_ARCH_AVR // Only supported on V0p2/AVR currently.
