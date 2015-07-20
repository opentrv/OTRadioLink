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

Author(s) / Copyright (s): Damon Hart-Davis 2015
*/

/*
 * OpenTRV RFM23B Radio Link base class.
 */

/**TEMPORARILY IN TEST SKETCH BEFORE BEING MOVED TO OWN LIBRARY. */

#ifndef OTRFM23BLINK_OTRFM23BLINK_H
#define OTRFM23BLINK_OTRFM23BLINK_H

#include <stddef.h>
#include <stdint.h>

// Arduino library...
#include <OTRadioLink.h>

#include <Arduino.h>
#include "OTV0P2BASE_BasicPinAssignments.h"
#include "OTV0P2BASE_FastDigitalIO.h"
#include "OTV0P2BASE_PowerManagement.h"

namespace OTRFM23BLink
    {
    // Base class for RFM23B radio link hardware driver.
    // Neither re-entrant nor ISR-safe except where stated.
    // Contains elements that do not depend on template parameters.
    class OTRFM23BLinkBase : public OTRadioLink::OTRadioLink
        {
        protected:
            static const uint8_t REG_INT_STATUS1 = 3; // Interrupt status register 1.
            static const uint8_t REG_INT_STATUS2 = 4; // Interrupt status register 2.
            static const uint8_t REG_INT_ENABLE1 = 5; // Interrupt enable register 1.
            static const uint8_t REG_INT_ENABLE2 = 6; // Interrupt enable register 2.
            static const uint8_t REG_OP_CTRL1 = 7; // Operation and control register 1.
            static const uint8_t REG_OP_CTRL1_SWRES = 0x80u; // Software reset (at write) in OP_CTRL1.
            static const uint8_t REG_OP_CTRL2 = 8; // Operation and control register 2.
            static const uint8_t REG_RSSI = 0x26; // RSSI.
            static const uint8_t REG_RSSI1 = 0x28; // Antenna 1 diversity / RSSI.
            static const uint8_t REG_RSSI2 = 0x29; // Antenna 2 diversity / RSSI.
            static const uint8_t REG_TX_POWER = 0x6d; // Transmit power.
            static const uint8_t REG_RX_FIFO_CTRL = 0x7e; // RX FIFO control.
            static const uint8_t REG_FIFO = 0x7f; // TX FIFO on write, RX FIFO on read.
            // Allow validation of RFM22/RFM23 device and SPI connection to it.
            static const uint8_t SUPPORTED_DEVICE_TYPE = 0x08; // Read from register 0.
            static const uint8_t SUPPORTED_DEVICE_VERSION = 0x06; // Read from register 1.

            // Write/read one byte over SPI...
            // SPI must already be configured and running.
            // TODO: convert from busy-wait to sleep, at least in a standby mode, if likely longer than 10s of uS.
            // At lowest SPI clock prescale (x2) this is likely to spin for ~16 CPU cycles (8 bits each taking 2 cycles).
            inline uint8_t _io(const uint8_t data) { SPDR = data; while (!(SPSR & _BV(SPIF))) { } return(SPDR); }
            // Write one byte over SPI (ignoring the value read back).
            // SPI must already be configured and running.
            // TODO: convert from busy-wait to sleep, at least in a standby mode, if likely longer than 10s of uS.
            // At lowest SPI clock prescale (x2) this is likely to spin for ~16 CPU cycles (8 bits each taking 2 cycles).
            inline void _wr(const uint8_t data) { SPDR = data; while (!(SPSR & _BV(SPIF))) { } }

//            // Returns true iff RFM23 appears to be correctly connected.
//            bool checkConnected();

//            // Power SPI up and down given this particular SPI/RFM23B select line.
//            virtual bool upSPI() = 0;
//            virtual void downSPI() = 0;

#if 0 // Defining the virtual destructor uses ~800+ bytes of Flash by forcing use of malloc()/free().
            // Ensure safe instance destruction when derived from.
            // by default attempts to shut down the sensor and otherwise free resources when done.
            // This uses ~800+ bytes of Flash by forcing use of malloc()/free().
            virtual ~OTRFM23BLinkBase() { }
#else
#define OTRFM23BLINK_NO_VIRT_DEST // Beware, no virtual destructor so be careful of use via base pointers.
#endif
        };

    // Concrete impl class for RFM23B radio link hardware driver.
    // Neither re-entrant nor ISR-safe except where stated.
    template <uint8_t SPI_nSS_DigitalPin> // Hardwire to I/O pin for RFM23B active-low SPI device select.
    class OTRFM23BLink : public OTRFM23BLinkBase
        {
        private:
            // Internal routines to enable/disable RFM23B on the the SPI bus.
            // These depend only on the (constant) SPI_nSS_DigitalPin template parameter
            // so these should turn into single assembler instructions in principle.
            inline void _SELECT() { fastDigitalWrite(SPI_nSS_DigitalPin, LOW); } // Select/enable RFM23B.
            inline void _DESELECT() { fastDigitalWrite(SPI_nSS_DigitalPin, HIGH); } // Deselect/disable RFM23B.

            // Power SPI up and down given this particular SPI/RFM23B select line.
            // Use all other default values.
            // Inlined non-virtual implementations for speed.
            inline bool _upSPI() { return(OTV0P2BASE::t_powerUpSPIIfDisabled<SPI_nSS_DigitalPin>()); }
            inline void _downSPI() { OTV0P2BASE::t_powerDownSPI<SPI_nSS_DigitalPin, OTV0P2BASE::V0p2_PIN_SPI_SCK, OTV0P2BASE::V0p2_PIN_SPI_MOSI, OTV0P2BASE::V0p2_PIN_SPI_MISO>(); }
//            // Versions accessible to the base class...
//            virtual bool upSPI() { return(_upSPI()); }
//            virtual void downSPI() { _downSPI(); }

            // Write to 8-bit register on RFM22.
            // SPI must already be configured and running.
            inline void _writeReg8Bit(const uint8_t addr, const uint8_t val)
                {
                _SELECT();
                _wr(addr | 0x80); // Force to write.
                _wr(val);
                _DESELECT();
                }

            // Write 0 to 16-bit register on RFM22 as burst.
            // SPI must already be configured and running.
            void _writeReg16Bit0(const uint8_t addr)
                {
                _SELECT();
                _wr(addr | 0x80); // Force to write.
                _wr(0);
                _wr(0);
                _DESELECT();
                }

            // Read from 8-bit register on RFM22.
            // SPI must already be configured and running.
            inline uint8_t _readReg8Bit(const uint8_t addr)
                {
                _SELECT();
                _io(addr & 0x7f); // Force to read.
                const uint8_t result = _io(0); // Dummy value...
                _DESELECT();
                return(result);
                }

            // Read from 16-bit big-endian register pair.
            // The result has the first (lower-numbered) register in the most significant byte.
            uint16_t _readReg16Bit(const uint8_t addr)
                {
                _SELECT();
                _io(addr & 0x7f); // Force to read.
                uint16_t result = ((uint16_t)_io(0)) << 8;
                result |= ((uint16_t)_io(0));
                _DESELECT();
                return(result);
                }

            // Returns true iff RFM23 appears to be correctly connected.
            bool _checkConnected()
                {
                const bool neededEnable = _upSPI();
                bool isOK = false;
                const uint8_t rType = _readReg8Bit(0); // May read as 0 if not connected at all.
                if(SUPPORTED_DEVICE_TYPE == rType)
                    {
                    const uint8_t rVersion = _readReg8Bit(1);
                    if(SUPPORTED_DEVICE_VERSION == rVersion)
                        { isOK = true; }
                    }
#if 0 && defined(DEBUG)
if(!isOK) { DEBUG_SERIAL_PRINTLN_FLASHSTRING("RFM23 bad"); }
#endif
                if(neededEnable) { _downSPI(); }
                return(isOK);
                }

            // Enter standby mode.
            // SPI must already be configured and running.
            void _modeStandby()
              {
              _writeReg8Bit(REG_OP_CTRL1, 0);
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINT_FLASHSTRING("Sb");
#endif
              }

            // Read/discard status (both registers) to clear interrupts.
            // SPI must already be configured and running.
            void _clearInterrupts()
              {
              //  _RFM22WriteReg8Bit(RFM22REG_INT_STATUS1, 0);
              //  _RFM22WriteReg8Bit(RFM22REG_INT_STATUS2, 0); // TODO: combine in burst write with previous...
              _writeReg16Bit0(REG_INT_STATUS1);
              }

            // Enter standby mode (consume least possible power but retain register contents).
            // FIFO state and pending interrupts are cleared.
            // Typical consumption in standby 450nA (cf 15nA when shut down, 8.5mA TUNE, 18--80mA RX/TX).
            void _modeStandbyAndClearState()
                {
                const bool neededEnable = _upSPI();
                _modeStandby();
                // Clear RX and TX FIFOs simultaneously.
                _writeReg8Bit(REG_OP_CTRL2, 3); // FFCLRRX | FFCLRTX
                _writeReg8Bit(REG_OP_CTRL2, 0); // Needs both writes to clear.
                // Disable all interrupts.
                //  _RFM22WriteReg8Bit(RFM22REG_INT_ENABLE1, 0);
                //  _RFM22WriteReg8Bit(RFM22REG_INT_ENABLE2, 0);
                _writeReg16Bit0(REG_INT_ENABLE1);
                // Clear any interrupts already/still pending...
                _clearInterrupts();
                if(neededEnable) { _downSPI(); }
// DEBUG_SERIAL_PRINTLN_FLASHSTRING("SCS");
                }

            // Minimal set-up of I/O (etc) after system power-up.
            // Performs a software reset and leaves the radio deselected and in a low-power and safe state.
            // Will power up SPI if needed.
            void _powerOnInit()
                {
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINTLN_FLASHSTRING("RFM23 reset...");
#endif
                const bool neededEnable = _upSPI();
                _writeReg8Bit(REG_OP_CTRL1, REG_OP_CTRL1_SWRES);
                _modeStandby();
                if(neededEnable) { _downSPI(); }
                }

        protected:
            // Configure the hardware.
            // Called from configure() once nChannels and channelConfig is set.
            // Returns false if hardware not present or config is invalid.
            // Defaults to do nothing.
            virtual bool _doconfig() { return(false); } // FIXME

            // Switch listening on or off.
            // listenChannel will have been set when this is called.
            virtual void _dolisten() { } // FIXME

        public:
            OTRFM23BLink() { }

            // Do very minimal pre-initialisation, eg at power up, to get radio to safe low-power mode.
            // Argument is read-only pre-configuration data;
            // may be mandatory for some radio types, else can be NULL.
            // This pre-configuration data depends entirely on the radio implementation,
            // but could for example be a minimal set of register number/values pairs in ROM.
            // This routine must not lock up if radio is not actually available/fitted.
            // Argument is ignored for this implementation.
            // NOT INTERRUPT SAFE and should not be called concurrently with any other RFM23B/SPI operation.
            virtual void preinit(const void *preconfig) { _powerOnInit(); }

            // Begin access to (initialise) this radio link if applicable and not already begun.
            // Returns true if it successfully begun, false otherwise.
            // Allows logic to end() if required at the end of a block, etc.
            // Defaults to do nothing (and return false).
            virtual bool begin()
                {
//  // Check that the radio is correctly connected; panic if not...
//  if(!RFM22CheckConnected()) { panic(); }
//  // Configure the radio.
//  RFM22RegisterBlockSetup(FHT8V_RFM22_Reg_Values);
//  // Put the radio in low-power standby mode.
//  RFM22ModeStandbyAndClearState();
                if(1 != nChannels) { return(false); }
                if(!_checkConnected()) { return(false); }
                // TODO
                return(false); // FIXME
                }

            // Returns true if this radio link is currently available.
            // True by default unless implementation overrides.
            // For those radios that need starting this will be false before begin().
            virtual bool isAvailable() { return(false); } // FIXME

            // Fetches the current inbound RX queue capacity and maximum raw message size.
            static const int QueueRXMsgsMax = 1;
            static const int MaxRXMsgLen = 64;
            static const int MaxTXMsgLen = 64;
            virtual void getCapacity(uint8_t &queueRXMsgsMax, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen)
                { queueRXMsgsMax = QueueRXMsgsMax; maxRXMsgLen = MaxRXMsgLen; maxTXMsgLen = MaxTXMsgLen; }

            // Fetches the current count of queued messages for RX.
            virtual uint8_t getRXMsgsQueued() { return(0); } // FIXME

            // Fetches the first (oldest) queued RX message, returning its length, or 0 if no message waiting.
            // If the waiting message is too long it is truncated to fit,
            // so allocating a buffer at least one longer than any valid message
            // should indicate an oversize inbound message.
            virtual uint8_t getRXMsg(uint8_t *buf, uint8_t buflen) { return(0); } // FIXME

//            // Returns the current receive error state; 0 indicates no error, +ve is the error value.
//            // RX errors may be queued with depth greater than one,
//            // or only the last RX error may be retained.
//            // Higher-numbered error states may be more severe.
//            virtual uint8_t getRXRerr() { return(0); }

            // Send/TX a frame on the specified channel, optionally quietly.
            // Revert afterwards to listen()ing if enabled,
            // else usually power down the radio if not listening.
            //   * quiet  if true then send can be quiet
            //     (eg if the receiver is known to be close by)
            //     to make better use of bandwidth; this hint may be ignored.
            //   * listenAfter  if true then try to listen after transmit
            //     for enough time to allow a remote turn-around and TX;
            //     may be ignored if radio will revert to receive mode anyway.
            // Returns true if the transmission was made, else false.
            // May block to transmit (eg to avoid copying the buffer).
            virtual bool send(int channel, const uint8_t *buf, uint8_t buflen, bool quiet = false, bool listenAfter = false) { return(false); } // FIXME

            // Poll for incoming messages (eg where interrupts are not available).
            // Will only have any effect when listen(true, ...) is active.
            // Can be used safely in addition to handling inbound interrupts.
            // Where interrupts are not available should be called at least as often
            // and messages are expected to arrive to avoid radio receiver overrun.
            // Default is to do nothing.
            virtual void poll() { } // FIXME

            // Handle simple interrupt for this radio link.
            // Must be fast and ISR (Interrupt Service Routine) safe.
            // Returns true if interrupt was successfully handled and cleared
            // else another interrupt handler in the chain may be called
            // to attempt to clear the interrupt.
            // Loosely has the effect of calling poll(),
            // but may respond to and deal with things other than inbound messages.
            // By default does nothing (and returns false).
            virtual bool handleInterruptSimple() { return(false); } // FIXME

            // End access to this radio link if applicable and not already ended.
            // Returns true if it needed to be ended.
            // Shuts down radio in safe low-power state.
            virtual bool end() { return(false); } // FIXME

#if 0 // Defining the virtual destructor uses ~800+ bytes of Flash by forcing use of malloc()/free().
            // Ensure safe instance destruction when derived from.
            // by default attempts to shut down the sensor and otherwise free resources when done.
            // This uses ~800+ bytes of Flash by forcing use of malloc()/free().
            virtual ~OTRFM23BLink() { end(); }
#else
#define OTRFM23BLINK_NO_VIRT_DEST // Beware, no virtual destructor so be careful of use via base pointers.
#endif
        };

    }
#endif
