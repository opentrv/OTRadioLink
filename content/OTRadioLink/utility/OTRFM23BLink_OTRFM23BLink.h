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

#include <util/atomic.h> // Atomic primitives for AVR.

#include <OTRadioLink.h>

#include <Arduino.h>
#include "OTV0P2BASE_BasicPinAssignments.h"
#include "OTV0P2BASE_FastDigitalIO.h"
#include "OTV0P2BASE_PowerManagement.h"

namespace OTRFM23BLink
    {
    // NOTE: SYSTEM-WIDE IMPLICATIONS FOR SPI USE.
    //
    // IF HARDWARE INTERRUPT HANDLING IS ENABLED FOR RFM23B
    // THEN SPI AND RFM23B OPERATIONS MAY BE PERFORMED IN THE ISR.
    // WHICH IMPLIES THAT (WHILE RFM23B INTERRUPTS ARE ENABLED)
    // ALL COMPOUND SPI OPERATIONS MAY NEED TO PERFORMED WITH
    // INTERRUPTS DISABLED.

    // Base class for RFM23B radio link hardware driver.
    // Neither re-entrant nor ISR-safe except where stated.
    // Contains elements that do not depend on template parameters.
    class OTRFM23BLinkBase : public OTRadioLink::OTRadioLink
        {
        public:
            // RX (minimum) queue capacity in messages.
            static const int QueueRXMsgsMin = 1;
            // Maximum raw RX message size in bytes.
            static const int MaxRXMsgLen = 64;
            // Maximum rawTX message size in bytes.
            static const int MaxTXMsgLen = 64;

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

            // Iff true then attempt to wake up as the start of a frame arrives, eg on sync.
            static const bool WAKE_ON_SYNC_RX = false;

            // Last RX error, as 1-deep queue; 0 if no error.
            // Marked as volatile for ISR-/thread- safe (sometimes lock-free) access.
            volatile uint8_t lastRXErr;

            // 1-deep RX queue and buffer used to accept data during RX.
            // Marked as volatile for ISR-/thread- safe (sometimes lock-free) access.
            volatile uint8_t lengthRX; // Non-zero when a frame is waiting.
            volatile uint8_t bufferRX[MaxRXMsgLen];

            // Constructor only available to deriving class.
            OTRFM23BLinkBase() : lastRXErr(0) { }

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

            // Internal routines to enable/disable RFM23B on the the SPI bus.
            // Versions accessible to the base class...
            virtual void _SELECT_() = 0;
            virtual void _DESELECT_() = 0;

            // Slower virtual calls but avoiding duplicated/header code.
            // Power SPI up and down given this particular SPI/RFM23B select line.
            virtual bool _upSPI_() = 0;
            virtual void _downSPI_() = 0;
            // Write to 8-bit register on RFM22.
            // SPI must already be configured and running.
            virtual void _writeReg8Bit_(const uint8_t addr, const uint8_t val) = 0;
            // Read from 8-bit register on RFM22.
            // SPI must already be configured and running.
            virtual uint8_t _readReg8Bit_(const uint8_t addr) = 0;
            // Enter standby mode (consume least possible power but retain register contents).
            // FIFO state and pending interrupts are cleared.
            // Typical consumption in standby 450nA (cf 15nA when shut down, 8.5mA TUNE, 18--80mA RX/TX).
            virtual void _modeStandbyAndClearState_() = 0;
            // Enter standby mode.
            // SPI must already be configured and running.
            virtual void _modeStandby_() = 0;
            // Enter transmit mode (and send any packet queued up in the TX FIFO).
            // SPI must already be configured and running.
            virtual void _modeTX_() = 0;
            // Enter receive mode.
            // SPI must already be configured and running.
            virtual void _modeRX_() = 0;
            // Read/discard status (both registers) to clear interrupts.
            // SPI must already be configured and running.
            virtual void _clearInterrupts_() = 0;

            // Returns true iff RFM23 appears to be correctly connected.
            bool _checkConnected();

            // Configure the radio from a list of register/value pairs in readonly PROGMEM/Flash, terminating with an 0xff register value.
            // NOTE: argument is not a pointer into SRAM, it is into PROGMEM!
            typedef uint8_t regValPair_t[2];
            void _registerBlockSetup(const regValPair_t* registerValues);

            // Clear TX FIFO.
            // SPI must already be configured and running.
            void _clearTXFIFO();

            // Clears the RFM23B TX FIFO and queues the supplied frame to send via the TX FIFO.
            // This routine does not change the frame area.
            // This uses an efficient burst write.
            void _queueFrameInTXFIFO(const uint8_t *bptr, uint8_t buflen);

            // Transmit contents of on-chip TX FIFO: caller should revert to low-power standby mode (etc) if required.
            // Returns true if packet apparently sent correctly/fully.
            // Does not clear TX FIFO (so possible to re-send immediately).
            bool _TXFIFO();

            // Put RFM23 into standby, attempt to read bytes from FIFO into supplied buffer.
            // Leaves RFM23 in low-power standby mode.
            // Trailing bytes (more than were actually sent) undefined.
            void _RXFIFO(uint8_t *buf, const uint8_t bufSize);

            // Switch listening off, on on to selected channel.
            // listenChannel will have been set by time this is called.
            virtual void _dolisten();

#if 0 // Defining the virtual destructor uses ~800+ bytes of Flash by forcing use of malloc()/free().
            // Ensure safe instance destruction when derived from.
            // by default attempts to shut down the sensor and otherwise free resources when done.
            // This uses ~800+ bytes of Flash by forcing use of malloc()/free().
            virtual ~OTRFM23BLinkBase() { }
#else
#define OTRFM23BLINK_NO_VIRT_DEST // Beware, no virtual destructor so be careful of use via base pointers.
#endif

        public:
            // Begin access to (initialise) this radio link if applicable and not already begun.
            // Returns true if it successfully began, false otherwise.
            // Allows logic to end() if required at the end of a block, etc.
            // Defaults to do nothing (and return false).
            virtual bool begin();

            // Fetches the current inbound RX minimum queue capacity and maximum RX (and TX) raw message size.
            virtual void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen)
                { queueRXMsgsMin = QueueRXMsgsMin; maxRXMsgLen = MaxRXMsgLen; maxTXMsgLen = MaxTXMsgLen; }

            // Fetches the first (oldest) queued RX message, returning its length, or 0 if no message waiting.
            // If the waiting message is too long it is truncated to fit,
            // so allocating a buffer at least one longer than any valid message
            // should indicate an oversize inbound message.
            virtual uint8_t getRXMsg(uint8_t *buf, uint8_t buflen);

            // Returns the current receive error state; 0 indicates no error, +ve is the error value.
            // RX errors may be queued with depth greater than one,
            // or only the last RX error may be retained.
            // Higher-numbered error states may be more severe or more specific.
            virtual uint8_t getRXErr() { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { const uint8_t r = (uint8_t)lastRXErr; lastRXErr = 0; return(r); } }

            // Send/TX a raw frame on the specified (default first/0) channel.
            // This does not add any pre- or post- amble (etc)
            // that particular receivers may require.
            // Revert afterwards to listen()ing if enabled,
            // else usually power down the radio if not listening.
            //   * power  hint to indicate transmission importance
            ///    and thus possibly power or other efforts to get it heard;
            //     this hint may be ignored.
            //   * listenAfter  if true then try to listen after transmit
            //     for enough time to allow a remote turn-around and TX;
            //     may be ignored if radio will revert to receive mode anyway.
            // Returns true if the transmission was made, else false.
            // May block to transmit (eg to avoid copying the buffer).
            virtual bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false);

            // End access to this radio link if applicable and not already ended.
            // Returns true if it needed to be ended.
            // Shuts down radio in safe low-power state.
            virtual bool end();
        };

    // Concrete impl class for RFM23B radio link hardware driver.
    // Neither re-entrant nor ISR-safe except where stated.
    // Configuration (the argument to config(), with channels == 1)
    // should be a list of register/value pairs in readonly PROGMEM/Flash,
    // terminating with an 0xff register value
    // ie argument is not a pointer into SRAM, it is into PROGMEM!
    // const uint8_t registerValues[][2]);

    // Hardwire to I/O pin for RFM23B active-low SPI device select: SPI_nSS_DigitalPin.
    // Hardwire to I/O pin for RFM23B active-low interrupt RFM_nIRQ_DigitalPin (-1 if none).
    template <uint8_t SPI_nSS_DigitalPin, int8_t RFM_nIRQ_DigitalPin = -1>
    class OTRFM23BLink : public OTRFM23BLinkBase
        {
        private:
            // Internal routines to enable/disable RFM23B on the the SPI bus.
            // These depend only on the (constant) SPI_nSS_DigitalPin template parameter
            // so these should turn into single assembler instructions in principle.
            inline void _SELECT() { fastDigitalWrite(SPI_nSS_DigitalPin, LOW); } // Select/enable RFM23B.
            inline void _DESELECT() { fastDigitalWrite(SPI_nSS_DigitalPin, HIGH); } // Deselect/disable RFM23B.
            // Versions accessible to the base class...
            virtual void _SELECT_() { _SELECT(); }
            virtual void _DESELECT_() { _DESELECT(); }

            // Power SPI up and down given this particular SPI/RFM23B select line.
            // Use all other default values.
            // Inlined non-virtual implementations for speed.
            inline bool _upSPI() { return(OTV0P2BASE::t_powerUpSPIIfDisabled<SPI_nSS_DigitalPin>()); }
            inline void _downSPI() { OTV0P2BASE::t_powerDownSPI<SPI_nSS_DigitalPin, OTV0P2BASE::V0p2_PIN_SPI_SCK, OTV0P2BASE::V0p2_PIN_SPI_MOSI, OTV0P2BASE::V0p2_PIN_SPI_MISO>(); }
            // Versions accessible to the base class...
            virtual bool _upSPI_() { return(_upSPI()); }
            virtual void _downSPI_() { _downSPI(); }

            // True if interrupt line is inactive (or doesn't exist).
            // A poll or interrupt service routine can terminate immediately if this is true.
            inline bool interruptLineIsEnabledAndInactive() { return(hasInterruptSupport && (LOW != fastDigitalRead(RFM_nIRQ_DigitalPin))); }

            // Write to 8-bit register on RFM23B.
            // SPI must already be configured and running.
            inline void _writeReg8Bit(const uint8_t addr, const uint8_t val)
                {
                _SELECT();
                _wr(addr | 0x80); // Force to write.
                _wr(val);
                _DESELECT();
                }
            // Version accessible to the base class...
            virtual void _writeReg8Bit_(const uint8_t addr, const uint8_t val) { _writeReg8Bit(addr, val); }

            // Write 0 to 16-bit register on RFM23B as burst.
            // SPI must already be configured and running.
            void _writeReg16Bit0(const uint8_t addr)
                {
                _SELECT();
                _wr(addr | 0x80); // Force to write.
                _wr(0);
                _wr(0);
                _DESELECT();
                }

            // Read from 8-bit register on RFM23B.
            // SPI must already be configured and running.
            inline uint8_t _readReg8Bit(const uint8_t addr)
                {
                _SELECT();
                _io(addr & 0x7f); // Force to read.
                const uint8_t result = _io(0); // Dummy value...
                _DESELECT();
                return(result);
                }
            // Version accessible to the base class...
            virtual uint8_t _readReg8Bit_(const uint8_t addr) { return(_readReg8Bit(addr)); }

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

            // Enter standby mode.
            // SPI must already be configured and running.
            inline void _modeStandby()
                {
                _writeReg8Bit(REG_OP_CTRL1, 0);
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINT_FLASHSTRING("Sb");
#endif
                }
            // Version accessible to the base class...
            virtual void _modeStandby_() { _modeStandby(); }

            // Enter transmit mode (and send any packet queued up in the TX FIFO).
            // SPI must already be configured and running.
            inline void _modeTX()
                {
                _writeReg8Bit(REG_OP_CTRL1, 9); // TXON | XTON
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINTLN_FLASHSTRING("Tx");
#endif
                }
            // Version accessible to the base class...
            virtual void _modeTX_() { _modeTX(); }

            // Enter receive mode.
            // SPI must already be configured and running.
            inline void _modeRX()
                {
                _writeReg8Bit(REG_OP_CTRL1, 5); // RXON | XTON
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINTLN_FLASHSTRING("Rx");
#endif
                }
            // Version accessible to the base class...
            virtual void _modeRX_() { _modeRX(); }

            // Read/discard status (both registers) to clear interrupts.
            // SPI must already be configured and running.
            // Inline for maximum speed ie minimum latency and CPU cycles.
            inline void _clearInterrupts()
                {
                _SELECT();
                _io(REG_INT_STATUS1 & 0x7f); // Force to read.
                _io(0);
                _io(0);
                _DESELECT();
                }
            // Version accessible to the base class...
            virtual void _clearInterrupts_() { _clearInterrupts(); }

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
            // Version accessible to the base class...
            virtual void _modeStandbyAndClearState_() { _modeStandbyAndClearState(); }

            // Read status (both registers) and clear interrupts.
            // Status register 1 is returned in the top 8 bits, register 2 in the bottom 8 bits.
            // Zero indicates no pending interrupts or other status flags set.
            uint16_t _readStatusBoth()
                {
                const bool neededEnable = _upSPI();
                const uint16_t result = _readReg16Bit(REG_INT_STATUS1);
                if(neededEnable) { _downSPI(); }
                return(result);
                }

            // Minimal set-up of I/O (etc) after system power-up.
            // Performs a software reset and leaves the radio deselected and in a low-power and safe state.
            // Will power up SPI if needed.
            // Inline to get to a sensible state in as few instructions as possible.
            inline void _powerOnInit()
                {
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINTLN_FLASHSTRING("RFM23 reset...");
#endif
                const bool neededEnable = _upSPI();
                _writeReg8Bit(REG_OP_CTRL1, REG_OP_CTRL1_SWRES);
                _modeStandby();
                if(neededEnable) { _downSPI(); }
                }

//            // Status from last _poll() in listen mode; undefined before first.
//            // Access only with interrupts blocked.
//            uint16_t _lastPollStatus;

            // Common handling of polling and ISR code.
            // NOT RENTRANT: interrupts must be blocked when this is called.
            // Keeping everything inline helps allow better ISR code generation
            // (less register pushes/pops since all use can be seen by the compiler).
            // Keeping this small minimises service time.
            // This does NOT attempt to interpret or filter inbound messages, just queues them.
            // Ensures radio is in RX mode at exit if listening is enabled.
            void _poll(const bool inISR)
                {
                // Nothing to do if not listening at the moment.
                if(-1 == getListenChannel()) { return; }
                // See what has arrived, if anything.
                const uint16_t status = _readStatusBoth();
//                _lastPollStatus = status;
                // Typical statuses during successful receive:
                //   * 0x2492
                //   * 0x3412
                if(status & 0x1000) // Received frame.
                    {
                    // If there is space in the queue then read in the frame, else discard it.
                    if(0 == queuedRXedMessageCount)
                        {
                        const size_t maxlen = sizeof(bufferRX);
                        _RXFIFO((uint8_t *)bufferRX, maxlen);
                        lengthRX = maxlen; // Not very clever yet!
                        queuedRXedMessageCount = 1; // Mark message as queued.
                        }
                    else
                        {
                        uint8_t tmpbuf[1];
                        _RXFIFO(tmpbuf, sizeof(tmpbuf)); // Discard...
                        ++droppedRXedMessageCountRecent;
                        lastRXErr = RXErr_DroppedFrame;
                        }
                    // Attempt to read the entire frame.
                    // Clear up and force back to listening...
                    _dolisten();
                    return;
                    }
                else if(WAKE_ON_SYNC_RX && (status & 0x80)) // Got sync from incoming message.
                    {
////    syncSeen = true;
                    // Keep waiting for rest of message...
                    // At this point in theory we could know exactly how long to wait.
                    return;
                    }
                else if(status & 0x8000) // RX FIFO overflow/underflow: give up and reset?
                    {
                    // Note the overrun error.
                    lastRXErr = RXErr_RXOverrun;
                    // Reset and force back to listening...
                    _dolisten();
                    return;
                    }
                }

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

            // Poll for incoming messages (eg where interrupts are not available).
            // Will only have any effect when listen(true, ...) is active.
            // Can be used safely in addition to handling inbound interrupts.
            // Where interrupts are not available should be called at least as often
            // as messages are expected to arrive to avoid radio receiver overrun.
            virtual void poll() { if(!interruptLineIsEnabledAndInactive()) { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { _poll(false); } } }

            // Handle simple interrupt for this radio link.
            // Must be fast and ISR (Interrupt Service Routine) safe.
            // Returns true if interrupt was successfully handled and cleared
            // else another interrupt handler in the chain may be called
            // to attempt to clear the interrupt.
            // Loosely has the effect of calling poll(),
            // but may respond to and deal with things other than inbound messages.
            // Initiating interrupt assumed blocked until this returns.
            virtual bool handleInterruptSimple()
                {
                if(interruptLineIsEnabledAndInactive()) { return(false); }
                _poll(true);
                return(true);
                }

            // Get current RSSI.
            // CURRENTLY RFM23B IMPL ONLY.
            // NOT OFFICIAL API: MAY BE WITHDRAWN AT ANY TIME.
            // Only valid when in RX mode.
            // Units as per RFM23B.
            uint8_t getRSSI()
                {
                ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
                    {
                    const bool neededEnable = _upSPI();
                    const uint8_t rssi = _readReg8Bit(REG_RSSI);
                    if(neededEnable) { _downSPI(); }
                    return(rssi);
                    }
                }

            // Get current mode.
            // CURRENTLY RFM23B IMPL ONLY.
            // NOT OFFICIAL API: MAY BE WITHDRAWN AT ANY TIME.
            // Only valid when in RX mode.
            // Units as per RFM23B.
            uint8_t getMode()
                {
                ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
                    {
                    const bool neededEnable = _upSPI();
                    const uint8_t mode = 0xf & _readReg8Bit(REG_OP_CTRL1);
                    if(neededEnable) { _downSPI(); }
                    return(mode);
                    }
                }

//            // Get last status in listen mode.
//            // CURRENTLY RFM23B IMPL ONLY.
//            // NOT OFFICIAL API: MAY BE WITHDRAWN AT ANY TIME.
//            // Only valid when in RX/listen mode.
//            // Units as per RFM23B.
//            uint16_t get_lastPollStatus_()
//                {
//                ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
//                    {
//                    return(_lastPollStatus);
//                    }
//                }

            // True if there is hardware interrupt support.
            // This might be dedicated to the radio, or shared with other devices.
            // Should be a compile-time constant.
            static const bool hasInterruptSupport = (RFM_nIRQ_DigitalPin >= 0);

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
