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
*/

/*
 Minimal light-weight standard-speed OneWire(TM) support.

 Only supported on V0p2/AVR currently.
 */

#ifndef OTV0P2BASE_MINOW_H
#define OTV0P2BASE_MINOW_H
#ifdef ARDUINO_ARCH_AVR // Only supported on V0p2/AVR currently.


#include <stdint.h>

#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#endif

// Source of default DQ pin.
#include "utility/OTV0P2BASE_BasicPinAssignments.h"
// Fast GPIO support and micro timing routines.
#include "utility/OTV0P2BASE_FastDigitalIO.h"
#include "utility/OTV0P2BASE_Sleep.h"

namespace OTV0P2BASE
{


// Minimal light-weight standard-speed OneWire(TM) support.
// OneWire protocol: http://www.maximintegrated.com/en/app-notes/index.mvp/id/126
//    The system must be capable of generating an accurate and repeatable 1us delay for standard speed ...
//    The four basic operations of a 1-Wire bus are Reset, Write 1 bit, Write 0 bit, and Read bit.
// Timings (us): A 6, B 64, C 60, D 10, E 9, F 55, G 0, H 480, I 70, J 410.
// OneWire search/discovery: http://www.maximintegrated.com/en/app-notes/index.mvp/id/187
// All transactions with OneWire slaves should start with a reset()
// which will also ensure that the GPIO is correctly configured.
// Not intended to be thread-/ISR- safe.
// Operations on separate instances (using different GPIOs) can be concurrent.
// Generally use the derived class templated for the particular GPIO pin.
#define MinimalOneWireBase_DEFINED
class MinimalOneWireBase
  {
  private:
    // Core non-ephemeral search parameters as per Maxim doc 187.
    bool lastDeviceFlag;
    int lastDiscrepancy;

  protected:
    MinimalOneWireBase(volatile uint8_t *const ir, const uint8_t rm) : inputReg(ir), regMask(rm) { }

    // Register and mask can be used for generic less time-critical operations.
    // Input/base register for the port.
    volatile uint8_t *const inputReg;
    // Bit mask for the OW pin.
    const uint8_t regMask;

    // Address in use for search.
    uint8_t addr[8];

    // Standardised delays; must be inlined and usually have interrupts turned off around them.
    // These are all reduced by enough time to allow two instructions, eg maximally-fast port operations.
    static const uint8_t stdDelayReduction = 5; // 5 suggested by COHEAT in the field 2015/09, originally 2;
    inline void delayA() const { OTV0P2BASE_delay_us(  6 - stdDelayReduction); }
    inline void delayB() const { OTV0P2BASE_delay_us( 64 - stdDelayReduction); }
    inline void delayC() const { OTV0P2BASE_delay_us( 60 - stdDelayReduction); }
    inline void delayD() const { OTV0P2BASE_delay_us( 10 - stdDelayReduction); }
    inline void delayE() const { OTV0P2BASE_delay_us(  9 - stdDelayReduction); }
    inline void delayF() const { OTV0P2BASE_delay_us( 55 - stdDelayReduction); }
    inline void delayG() const { OTV0P2BASE_delay_us(  0 - stdDelayReduction); }
    inline void delayH() const { OTV0P2BASE_delay_us(480 - stdDelayReduction); }
    inline void delayI() const { OTV0P2BASE_delay_us( 70 - stdDelayReduction); }
    inline void delayJ() const { OTV0P2BASE_delay_us(410 - stdDelayReduction); }

    // Fast direct GPIO operations.
    // Will be fastest (eg often single instructions) if their arguments are compile-time constants.
#if defined(__AVR_ATmega328P__) // Probably all AVR.
    // Set selected bit low if an output, else turn off weak pull-up if an input.
    inline void bitWriteLow  (volatile uint8_t *const inputReg, const uint8_t bitmask) { *(inputReg+2) &= ~bitmask; }
    // Set selected bit high if an output, else turn on weak pull-up if an input.
    inline void bitWriteHigh (volatile uint8_t *const inputReg, const uint8_t bitmask) { *(inputReg+2) |=  bitmask; }
    // Set selected bit to be an output.
    inline void bitModeOutput(volatile uint8_t *const inputReg, const uint8_t bitmask) { *(inputReg+1) |=  bitmask; }
    // Set selected bit to be an input.
    inline void bitModeInput (volatile uint8_t *const inputReg, const uint8_t bitmask) { *(inputReg+1) &= ~bitmask; }
    // Read selected bit.
    inline bool bitReadIn    (volatile uint8_t *const inputReg, const uint8_t bitmask) { return(0 != ((*inputReg) & bitmask)); }
#endif

  public:
    // Reset interface; returns false if no slave device present.
    // Reset the 1-Wire bus slave devices and ready them for a command.
    // Delay G (0); drive bus low, delay H (48); release bus, delay I (70); sample bus, 0 = device(s) present, 1 = no device present; delay J (410).
    // Marks the interface as initialised.
    bool reset();

    // Read one bit from slave; returns true if high/1.
    // Read a bit from the 1-Wire slaves (Read time slot).
    // Drive bus low, delay A (6); release bus, delay E (9); sample bus to read bit from slave; delay F (55).
    // With a slow CPU it is not possible to implement these primitives here and achieve correct timings.
    virtual bool read_bit() = 0;

    // Write one bit leaving the bus powered afterwards.
    // Write 1: drive bus low, delay A; release bus, delay B.
    // Write 0: drive bus low, delay C; release bus, delay D.
    // With a slow CPU it is not possible to implement these primitives here and achieve correct timings.
    virtual void write_bit(bool high) = 0;

    // Clear/restart search.
    void reset_search();

    // Search for the next device.
    // Returns true if a new address has been found;
    // false means no devices or all devices already found or bus shorted.
    // This does not check the CRC.
    // Follows the broad algorithm shown in http://www.maximintegrated.com/en/app-notes/index.mvp/id/187
    bool search(uint8_t newAddr[8]);

    // Read a byte.
    // Read least-significant-bit first.
    uint8_t read();

    // Write a byte leaving the bus unpowered at the end.
    // Write least-significant-bit first.
    void write(uint8_t v);

    // Write multiple bytes, leaving the bus unpowered at the end.
    void write_bytes(const uint8_t *buf, uint16_t count);

    // Select a particular device on the bus.
    void select(const uint8_t addr[8]);

    // Select all devices on the bus
    void skip(void);
};

// Not intended to be thread-/ISR- safe.
// Operations on separate instances (using different GPIOs) can be concurrent.
template <uint8_t DigitalPin = V0p2_PIN_OW_DQ_DATA>
class MinimalOneWire : public MinimalOneWireBase
  {
  private:
    // Compute the input/base register for the port.
    // This may need further parameterisation for non-ATMega328P systems.
    volatile uint8_t *getInputReg() const { return(fastDigitalInputRegister(DigitalPin)); }
    // Compute the bit mask for the OW pin in advance.
    // This may need further parameterisation for non-ATMega328P systems.
    static const uint8_t regMask = _fastDigitalMask(DigitalPin);

  public:
    MinimalOneWire() : MinimalOneWireBase(getInputReg(), regMask) { reset_search(); }

    // Read one bit from slave; returns true if high/1.
    // Read a bit from the 1-Wire slaves (Read time slot).
    // Drive bus low, delay A (6); release bus, delay E (9); sample bus to read bit from slave; delay F (55).
    // Speed/timing are critical.
    virtual bool read_bit()
      {
      bool result = false;

      volatile uint8_t *const inputReg = getInputReg();

      // Locks out all interrupts until the final delay to keep timing as accurate as possible,
      // restoring them to their original state when done.
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
        // Drive bus/DQ low.
        bitWriteLow(inputReg, regMask);
        bitModeOutput(inputReg, regMask);
        // Delay A.
        delayA();
        // Release the bus (ie let it float).
        bitModeInput(inputReg, regMask);
        // Delay E.
        delayE();
        // Sample response from slave.
        result = bitReadIn(inputReg, regMask);
        }
      // Delay F.
      // Timing is not critical here so interrupts are allowed in again...
      delayF();

      return(result);
      }

    // Write one bit leaving the bus powered afterwards.
    // Write 1: drive bus low, delay A; release bus, delay B.
    // Write 0: drive bus low, delay C; release bus, delay D.
    virtual void write_bit(const bool high)
      {
      volatile uint8_t *const inputReg = getInputReg();

      // Locks out all interrupts until the final delay to keep timing as accurate as possible,
      // restoring them to their original state when done.
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
        // Drive bus/DQ low.
        bitWriteLow(inputReg, regMask);
        bitModeOutput(inputReg, regMask);
        // Delay A (for 1) or C (for 0).
        if(high) { delayA(); } else { delayC(); }
        // Release the bus (ie let it float).
        bitModeInput(inputReg, regMask);
        }
      // Delay B (for 1) or D (for 0).
      // Timing is not critical here so interrupts are allowed in again...
      if(high) { delayB(); } else { delayD(); }
      }
  };


}

#endif // ARDUINO_ARCH_AVR // Only supported on V0p2/AVR currently.
#endif
