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
                           Milenko Alcin 2016
*/

/*
 * OpenTRV RFM23B Radio Link base class.
 *
 * Currently V0p2/AVR ONLY.
 */

#ifndef OTRFM23BLINK_OTRFM23BLINK_H
#define OTRFM23BLINK_OTRFM23BLINK_H

#include <stddef.h>
#include <stdint.h>

#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h> // Atomic primitives for AVR.
#endif

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <OTV0p2Base.h>
#include <OTRadioLink.h>
#include "OTRadioLink_ISRRXQueue.h"


namespace OTRFM23BLink
    {
    // NOTE: SYSTEM-WIDE IMPLICATIONS FOR SPI USE.
    //
    // IF HARDWARE INTERRUPT HANDLING IS ENABLED FOR RFM23B
    // THEN SPI AND RFM23B OPERATIONS MAY BE PERFORMED IN THE ISR.
    // WHICH IMPLIES THAT (WHILE RFM23B INTERRUPTS ARE ENABLED)
    // ALL COMPOUND SPI OPERATIONS MAY NEED TO PERFORMED WITH
    // INTERRUPTS DISABLED.

    // All foreground RFM23B access should be protected from interrupts
    // but this code's ISR that may interfere with (eg) register access.

    // See end for library of common configurations.

#ifdef ARDUINO_ARCH_AVR
    // Base class for RFM23B radio link hardware driver.
    // Neither re-entrant nor ISR-safe except where stated.
    // Contains elements that do not depend on template parameters.
#define OTRFM23BLinkBase_DEFINED
    class OTRFM23BLinkBase : public OTRadioLink::OTRadioLink
        {
        protected:
            // Configure the radio from a list of register/value pairs in readonly PROGMEM/Flash, terminating with an 0xff register value.
            // NOTE: argument is not a pointer into SRAM, it is into PROGMEM!
            typedef uint8_t regValPair_t[2];
            void _registerBlockSetup(const regValPair_t* registerValues);

        public:
            // Maximum raw RX message size in bytes.
            static const int MaxRXMsgLen = 64;
            // Maximum rawTX message size in bytes.
            static const int MaxTXMsgLen = 64;

            // Maximum allowed TX time, milliseconds.
            // Attempting a longer TX will result in a timeout.
            static const int MAX_TX_ms = 1000;

            // Typical maximum size of encoded FHT8V/FS20 frame for OpenTRV as at 2015/07.
            static const uint8_t MAX_RX_FRAME_FHT8V = 45;
            // Default expected maximum size of mixed data (eg including JSON frames).
            // Too large a value may mean some frames are lost due to overrun/wrap-around.
            // To small a value may truncate long inbound frames and waste space.
            // Allowing ~15ms/~bytes (at 1.8ms/byte for FHT8V/FS20) for servicing time
            // seems prudent given typical V0p2 OpenTRV polling behaviour as at 2015/07.
            // So set this to 52 or less if not able to service RX with an interrupt
            // when receiving FTH8V/FS20 and JSON frames by polling at ~15ms intervals.
            // The RFM23B default is 55.
            static const uint8_t MAX_RX_FRAME_FHT8V_POLL_15ms = 52;
            // If the RX is serviced with an interrupt
            // then much nearer the whole 64-byte frame / RXFIFO is usable
            // how much depending on data rate and interrupt response time
            // especially from low-power sleep.
            // Previous (before 2015/07/22) max JSON frame length was preamble + 55 + 1-byte CRC,
            // so attempt to be higher than that.
            static const uint8_t MAX_RX_FRAME_DEFAULT = 60;

            // Type of the config information that this radio expects passed
            // as the config field of the OTRadioChannelConfig object.
            // This is an array of {0xff, 0xff} terminated register number/value pairs,
            // in Flash/PROGMEM, which is cast to a void* for OTRadioChannelConfig::config.
            // Type of one channel's array of register pairs.
            typedef const uint8_t RFM23_Reg_Values_t[][2] PROGMEM;

        protected:
            // Currently configured channel; starts at default 0.
            uint8_t _currentChannel;

            // RFM23B_REG_03_INTERRUPT_STATUS1
            static const uint16_t RFM23B_IFFERROR   = 0x80<<8;
            static const uint16_t RFM23B_ITXFFAFULL = 0x40<<8;
            static const uint16_t RFM23B_ITXFFAEM   = 0x20<<8;
            static const uint16_t RFM23B_IRXFFAFULL = 0x10<<8;
            static const uint16_t RFM23B_IEXT       = 0x08<<8;
            static const uint16_t RFM23B_IPKSENT    = 0x04<<8;
            static const uint16_t RFM23B_IPKVALID   = 0x02<<8;
            static const uint16_t RFM23B_ICRCERROR  = 0x01<<8;

            // RFM23B_REG_04_INTERRUPT_STATUS2
            static const uint8_t RFM23B_ISWDET     =    0x80;
            static const uint8_t RFM23B_IPREAVAL   =    0x40;
            static const uint8_t RFM23B_IPREAINVAL =    0x20;
            static const uint8_t RFM23B_IRSSI      =    0x10;
            static const uint8_t RFM23B_IWUT       =    0x08;
            static const uint8_t RFM23B_ILBD       =    0x04;
            static const uint8_t RFM23B_ICHIPRDY   =    0x02;
            static const uint8_t RFM23B_IPOR       =    0x01;

            // RFM23B_REG_05_INTERRUPT_ENABLE1
            static const uint8_t RFM23B_ENFFERR    =    0x80;
            static const uint8_t RFM23B_ENTXFFAFUL =    0x40;
            static const uint8_t RFM23B_ENTXFFAEM  =    0x20;
            static const uint8_t RFM23B_ENRXFFAFUL =    0x10;
            static const uint8_t RFM23B_ENEXT      =    0x08;
            static const uint8_t RFM23B_ENPKSENT   =    0x04;
            static const uint8_t RFM23B_ENPKVALID  =    0x02;
            static const uint8_t RFM23B_ENCRCERROR =    0x01;

            // RFM23B_REG_06_INTERRUPT_ENABLE2
            static const uint8_t RFM23B_ENSWDET    =    0x80;
            static const uint8_t RFM23B_ENPREAVAL  =    0x40;
            static const uint8_t RFM23B_ENPREAINVAL=    0x20;
            static const uint8_t RFM23B_ENRSSI     =    0x10;
            static const uint8_t RFM23B_ENWUT      =    0x08;
            static const uint8_t RFM23B_ENLBDI     =    0x04;
            static const uint8_t RFM23B_ENCHIPRDY  =    0x02;
            static const uint8_t RFM23B_ENPOR      =    0x01;

            // RFM23B_REG_30_DATA_ACCESS_CONTROL
            static const uint8_t RFM23B_ENPACRX    =    0x80;
            static const uint8_t RFM23B_ENPACTX    =    0x08;

// RH_RF22_REG_33_HEADER_CONTROL2               
            static const uint8_t RFM23B_HDLEN      =    0x70;
            static const uint8_t RFM23B_HDLEN_0    =    0x00;
            static const uint8_t RFM23B_HDLEN_1    =    0x10;
            static const uint8_t RFM23B_HDLEN_2    =    0x20;
            static const uint8_t RFM23B_HDLEN_3    =    0x30;
            static const uint8_t RFM23B_HDLEN_4    =    0x40;
            static const uint8_t RFM23B_VARPKLEN   =    0x00;
            static const uint8_t RFM23B_FIXPKLEN   =    0x08;
            static const uint8_t RFM23B_SYNCLEN    =    0x06;
            static const uint8_t RFM23B_SYNCLEN_1  =    0x00;
            static const uint8_t RFM23B_SYNCLEN_2  =    0x02;
            static const uint8_t RFM23B_SYNCLEN_3  =    0x04;
            static const uint8_t RFM23B_SYNCLEN_4  =    0x06;
            static const uint8_t RFM23B_PREALEN8   =    0x01;


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
            static const uint8_t REG_30_DATA_ACCESS_CONTROL = 0x30; 
            static const uint8_t REG_33_HEADER_CONTROL2  = 0x33; 
            static const uint8_t REG_3E_PACKET_LENGTH    = 0x3e; 
            static const uint8_t REG_3A_TRANSMIT_HEADER3 = 0x3a; 
            static const uint8_t REG_47_RECEIVED_HEADER3 = 0x47; 
            static const uint8_t REG_4B_RECEIVED_PACKET_LENGTH = 0x4b; 
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

            // Typical maximum frame length in bytes [1,63] to optimise radio behaviour.
            // Too long may allow overruns, too short may make long-frame reception hard.
            volatile uint8_t maxTypicalFrameBytes;

            // If true then allow RX operations.
            const bool allowRXOps;

            // Constructor only available to deriving class.
            OTRFM23BLinkBase(bool _allowRX = true)
              : _currentChannel(0), lastRXErr(0), maxTypicalFrameBytes(MAX_RX_FRAME_DEFAULT), allowRXOps(_allowRX)
              { }

            // Write/read one byte over SPI...
            // SPI must already be configured and running.
            // TODO: convert from busy-wait to sleep, at least in a standby mode, if likely longer than 10s of uS.
            // At lowest SPI clock prescale (x2) this is likely to spin for ~16 CPU cycles (8 bits each taking 2 cycles).
            // Treat as if this does not alter state, though in some cases it will.
            inline uint8_t _io(const uint8_t data) const { SPDR = data; while (!(SPSR & _BV(SPIF))) { } return(SPDR); }
            // Write one byte over SPI (ignoring the value read back).
            // SPI must already be configured and running.
            // TODO: convert from busy-wait to sleep, at least in a standby mode, if likely longer than 10s of uS.
            // At lowest SPI clock prescale (x2) this is likely to spin for ~16 CPU cycles (8 bits each taking 2 cycles).
            inline void _wr(const uint8_t data) { SPDR = data; while (!(SPSR & _BV(SPIF))) { } }

            // Internal routines to enable/disable RFM23B on the the SPI bus.
            // Versions accessible to the base class...
            virtual void _SELECT_() const = 0;
            virtual void _DESELECT_() const = 0;

            // Slower virtual calls but avoiding duplicated/header code.
            // Power SPI up and down given this particular SPI/RFM23B select line.
            virtual bool _upSPI_() const = 0;
            virtual void _downSPI_() const = 0;
            // Write to 8-bit register on RFM22.
            // SPI must already be configured and running.
            virtual void _writeReg8Bit_(const uint8_t addr, const uint8_t val) = 0;
            // Read from 8-bit register on RFM22.
            // SPI must already be configured and running.
            // Treat as if this does not alter state, though in some cases it will.
            virtual uint8_t _readReg8Bit_(const uint8_t addr) const = 0;
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
            bool _checkConnected() const;

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

            // Switch listening off, on to selected channel.
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
            // Configure radio for transmission via specified channel < nChannels; non-negative.
            void _setChannel(uint8_t channel);
   
#if 1 && defined(MILENKO_DEBUG)
            // Compact register dump
            void readRegs(uint8_t from, uint8_t to, uint8_t noHeader = 0);
            void printHex(int val); 
#endif

        public:
            // Set typical maximum frame length in bytes [1,63] to optimise radio behaviour.
            // Too long may allow overruns, too short may make long-frame reception hard.
            void setMaxTypicalFrameBytes(uint8_t maxTypicalFrameBytes);

            // Begin access to (initialise) this radio link if applicable and not already begun.
            // Returns true if it successfully began, false otherwise.
            // Allows logic to end() if required at the end of a block, etc.
            // Defaults to do nothing (and return false).
            virtual bool begin();

            // Returns the current receive error state; 0 indicates no error, +ve is the error value.
            // RX errors may be queued with depth greater than one,
            // or only the last RX error may be retained.
            // Higher-numbered error states may be more severe or more specific.
            virtual uint8_t getRXErr() { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { const uint8_t r = (uint8_t)lastRXErr; lastRXErr = 0; return(r); } return 0;} // FIXME Added return 0 to fix warning

            // Send/TX a raw frame on the specified (default first/0) channel.
            // This does not add any pre- or post- amble (etc)
            // that particular receivers may require.
            // Revert afterwards to listen()ing if enabled,
            // else usually power down the radio if not listening.
            //   * power  hint to indicate transmission importance
            //     and thus possibly power or other efforts to get it heard;
            //     this hint may be ignored.
            //   * listenAfter  if true then try to listen after transmit
            //     for enough time to allow a remote turn-around and TX;
            //     may be ignored if radio will revert to receive mode anyway.
            // Returns true if the transmission was made, else false.
            // May block to transmit (eg to avoid copying the buffer).
            //
            // Implementation specifics:
            //   * at TXmax will do double TX with 15ms sleep/IDLE mode between.
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
    //
    // Hardwire to I/O pin for RFM23B active-low SPI device select: SPI_nSS_DigitalPin.
    // Hardwire to I/O pin for RFM23B active-low interrupt RFM_nIRQ_DigitalPin (-1 if none).
    // Set the targetISRRXMinQueueCapacity to at least 2, or 3 if RAM space permits, for busy RF channels.
    // With allowRX == false as much as possible of the receive side is disabled.
#define OTRFM23BLink_DEFINED
    static const uint8_t DEFAULT_RFM23B_RX_QUEUE_CAPACITY = 3;
    template <uint8_t SPI_nSS_DigitalPin, int8_t RFM_nIRQ_DigitalPin = -1, uint8_t targetISRRXMinQueueCapacity = 3, bool allowRX = true>
    class OTRFM23BLink : public OTRFM23BLinkBase
        {
        private:
            // Use some template meta-programming
            // to replace the RX queue with a dummy if RX is not allowed/required.
            // Eg see https://en.wikibooks.org/wiki/C%2B%2B_Programming/Templates/Template_Meta-Programming#Compile-time_programming
            template <bool Condition, typename TypeTrue, typename TypeFalse>
              class typeIf;
            template <typename TypeTrue, typename TypeFalse>
              struct typeIf<true, TypeTrue, TypeFalse> { typedef TypeTrue t; };
            template <typename TypeTrue, typename TypeFalse>
              struct typeIf<false, TypeTrue, TypeFalse> { typedef TypeFalse t; };
            typename typeIf<allowRX, ::OTRadioLink::ISRRXQueueVarLenMsg<MaxRXMsgLen, targetISRRXMinQueueCapacity>, ::OTRadioLink::ISRRXQueueNULL>::t queueRX;

//#if 0
//            // Simple and fast 1-deep queue.
//            ::OTRadioLink::ISRRXQueue1Deep<MaxRXMsgLen> queueRX;
//#else
//            // Queue that can make good use of space for variable-length messages.
//            ::OTRadioLink::ISRRXQueueVarLenMsg<MaxRXMsgLen, targetISRRXMinQueueCapacity> queueRX;
//#endif

            // Internal routines to enable/disable RFM23B on the the SPI bus.
            // These depend only on the (constant) SPI_nSS_DigitalPin template parameter
            // so these should turn into single assembler instructions in principle.
            // Introduce some delays to allow signals to stabilise if running slow.
            // From the RFM23B datasheet (S3/p14) tEN & tSS are 20ns so waits shouldn't be necessary for AVR CPU speeds!
            static const bool runSPISlow = ::OTV0P2BASE::DEFAULT_RUN_SPI_SLOW;
            inline void _nSSWait() const { OTV0P2BASE_busy_spin_delay(runSPISlow?4:0); }
            // Wait from SPI select to op, and after op to deselect, and after deselect.
            inline void _SELECT() const { fastDigitalWrite(SPI_nSS_DigitalPin, LOW); _nSSWait(); } // Select/enable RFM23B.
            inline void _DESELECT() const { _nSSWait(); fastDigitalWrite(SPI_nSS_DigitalPin, HIGH); _nSSWait(); } // Deselect/disable RFM23B.
            // Versions accessible to the base class...
            virtual void _SELECT_() const { _SELECT(); }
            virtual void _DESELECT_() const { _DESELECT(); }

            // Power SPI up and down given this particular SPI/RFM23B select line.
            // Use all other default values.
            // Inlined non-virtual implementations for speed.
            inline bool _upSPI() const { return(OTV0P2BASE::t_powerUpSPIIfDisabled<SPI_nSS_DigitalPin, runSPISlow>()); }
            inline void _downSPI() const { OTV0P2BASE::t_powerDownSPI<SPI_nSS_DigitalPin, OTV0P2BASE::V0p2_PIN_SPI_SCK, OTV0P2BASE::V0p2_PIN_SPI_MOSI, OTV0P2BASE::V0p2_PIN_SPI_MISO, runSPISlow>(); }
            // Versions accessible to the base class...
            virtual bool _upSPI_() const { return(_upSPI()); }
            virtual void _downSPI_() const { _downSPI(); }

            // True if interrupt line is inactive (or doesn't exist).
            // A poll or interrupt service routine can terminate immediately if this is true.
            inline bool interruptLineIsEnabledAndInactive() const { return(hasInterruptSupport && (LOW != fastDigitalRead(RFM_nIRQ_DigitalPin))); }

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
            // Treat as if this does not alter state, though in some cases it will.
            inline uint8_t _readReg8Bit(const uint8_t addr) const
                {
                _SELECT();
                _io(addr & 0x7f); // Force to read.
                const uint8_t result = _io(0); // Dummy value...
                _DESELECT();
                return(result);
                }
            // Version accessible to the base class...
            virtual uint8_t _readReg8Bit_(const uint8_t addr) const { return(_readReg8Bit(addr)); }

            // Read from 16-bit big-endian register pair.
            // The result has the first (lower-numbered) register in the most significant byte.
            // Treat as if this does not alter state, though in some cases it will.
            uint16_t _readReg16Bit(const uint8_t addr) const
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
#if 0 && defined(V0P2BASE_DEBUG)
V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("Sb");
#endif
                }
            // Version accessible to the base class...
            virtual void _modeStandby_() { _modeStandby(); }

            // Enter transmit mode (and send any packet queued up in the TX FIFO).
            // SPI must already be configured and running.
            inline void _modeTX()
                {
                _writeReg8Bit(REG_OP_CTRL1, 9); // TXON | XTON
#if 0 && defined(V0P2BASE_DEBUG)
V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("Tx");
#endif
                }
            // Version accessible to the base class...
            virtual void _modeTX_() { _modeTX(); }

            // Enter receive mode.
            // SPI must already be configured and running.
            inline void _modeRX()
                {
                _writeReg8Bit(REG_OP_CTRL1, 5); // RXON | XTON
#if 0 && defined(V0P2BASE_DEBUG)
V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("Rx");
#endif
                }
            // Version accessible to the base class...
            virtual void _modeRX_() { _modeRX(); }

            // Read/discard status (both registers) to clear interrupts.
            // SPI must already be configured and running.
            // Inline for maximum speed ie minimum latency and CPU cycles.
            // Interrupts from interfering access must already be blocked.
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
            // POWERS UP SPI IF NECESSARY.
            void _modeStandbyAndClearState()
                {
                // Lock out interrupts while fiddling with interrupts.
                ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
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
                    }
// V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("SCS");
                }
            // Version accessible to the base class...
            virtual void _modeStandbyAndClearState_() { _modeStandbyAndClearState(); }

            // Read status (both registers) and clear interrupts.
            // Status register 1 is returned in the top 8 bits, register 2 in the bottom 8 bits.
            // Zero indicates no pending interrupts or other status flags set.
            // POWERS UP SPI IF NECESSARY.
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
#if 0 && defined(V0P2BASE_DEBUG)
V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("RFM23 reset...");
#endif
                const bool neededEnable = _upSPI();
                _writeReg8Bit(REG_OP_CTRL1, REG_OP_CTRL1_SWRES);
                _modeStandby();
                if(neededEnable) { _downSPI(); }
                }

            // Common handling of polling and ISR code.
            // NOT RENTRANT: interrupts must be blocked when this is called.
            // Keeping everything inline helps allow better ISR code generation
            // (less register pushes/pops since all use can be seen by the compiler).
            // Keeping this small minimises service time.
            // This does NOT attempt to interpret or filter inbound messages, just queues them.
            // Ensures radio is in RX mode at exit if listening is enabled.
            void _poll()
                {
                // Nothing to do if RX is not allowed.
                if(!allowRX) { return; }
 
                // Nothing to do if not listening at the moment.
                if(-1 == getListenChannel()) { return; }

                // See what has arrived, if anything.
                const uint16_t status = _readStatusBoth();

                // We need to check if RFM23B is in packet mode and based on that 
                // we select interrupt routine.
                const bool neededEnable = _upSPI_();
                const uint8_t rxMode = _readReg8Bit_(REG_30_DATA_ACCESS_CONTROL);
                if(neededEnable) { _downSPI_(); }
                if(rxMode & RFM23B_ENPACRX)
                  {
                  // Packet-handling mode...
                    if(status & RFM23B_IPKVALID) // Packet received OK
                        {
                        const bool neededEnable = _upSPI();
                        // Extract packet/frame length...
                        uint8_t lengthRX; 
                        // Number of bytes to read depends whether fixed of variable packet length
                        if ((_readReg8Bit_(REG_33_HEADER_CONTROL2) & RFM23B_FIXPKLEN ) == RFM23B_FIXPKLEN ) 
                           lengthRX = _readReg8Bit(REG_3E_PACKET_LENGTH);
                        else
                           lengthRX = _readReg8Bit(REG_4B_RECEIVED_PACKET_LENGTH);
                        if(neededEnable) { _downSPI(); }
                        // Received frame.
                        // If there is space in the queue then read in the frame, else discard it.
                        volatile uint8_t *const bufferRX = (lengthRX > MaxRXMsgLen) ? NULL :
                            queueRX._getRXBufForInbound();
                        if(NULL != bufferRX)
                            {
                            // Attempt to read the entire frame.
                            _RXFIFO((uint8_t *)bufferRX, MaxRXMsgLen);
                            // If an RX filter is present then apply it.
                            quickFrameFilter_t *const f = filterRXISR;
                            if((NULL != f) && !f(bufferRX, lengthRX))
                                {
                                ++filteredRXedMessageCountRecent; // Drop the frame: filter didn't like it.
                                queueRX._loadedBuf(0); // Don't queue this frame...
                                }
                            else
                                {
                                queueRX._loadedBuf(lengthRX); // Queue message.
                                }
                            }
                        else
                            {
                            // DISCARD/drop frame that there is no room to RX.
                            uint8_t tmpbuf[1];
                            _RXFIFO(tmpbuf, sizeof(tmpbuf));
                            ++droppedRXedMessageCountRecent;
                            lastRXErr = RXErr_DroppedFrame;
                            }
                        // Clear up and force back to listening...
                        _dolisten();
                        //return;
                        }
#if 0 && defined(MILENKO_DEBUG)
                    // Preamble received
                    if(status & RFM23B_IPREAVAL)
                        {
                        _lastPreambleTime = millis();
                        V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("IPREAVAL ");
                        }
#endif
                    }
                else
                    {
                    // Non-packet-handling mode... (eg FS20/OOK style)

                    // Typical statuses during successful receive:
                    //   * 0x2492
                    //   * 0x3412
                    if(status & 0x8000)
                        {
                        // RX FIFO overflow/underflow: give up and reset.
                        // Do this first to avoid trying to read a mangled/overrun frame.
                        // Note the overrun error.
                        lastRXErr = RXErr_RXOverrun;
                        // Reset and force back to listening...
                        _dolisten();
                        return;
                        }
                    else if(status & 0x1000)
                        {
                        // Received frame.
                        // If there is space in the queue then read in the frame, else discard it.
                        volatile uint8_t *const bufferRX = queueRX._getRXBufForInbound();
                        if(NULL != bufferRX)
                            {
                            // Attempt to read the entire frame.
                            _RXFIFO((uint8_t *)bufferRX, MaxRXMsgLen);
                            uint8_t lengthRX = MaxRXMsgLen; // Not very clever yet!
                            // If an RX filter is present then apply it.
                            quickFrameFilter_t *const f = filterRXISR;
                            if((NULL != f) && !f(bufferRX, lengthRX))
                                {
                                ++filteredRXedMessageCountRecent; // Drop the frame: filter didn't like it.
                                queueRX._loadedBuf(0); // Don't queue this frame...
                                }
                            else
                                {
                                queueRX._loadedBuf(lengthRX); // Queue message.
                                }
                            }
                        else
                            {
                            // DISCARD/drop frame that there is no room to RX.
                            uint8_t tmpbuf[1];
                            _RXFIFO(tmpbuf, sizeof(tmpbuf));
                            ++droppedRXedMessageCountRecent;
                            lastRXErr = RXErr_DroppedFrame;
                            }
                        // Clear up and force back to listening...
                        _dolisten();
                        return;
                        }
                    else if(WAKE_ON_SYNC_RX && (status & 0x80))
                        {
                        // Got sync from incoming message.
                        // Could in principle time until the RX FIFO should have filled.
                        // Could also be used to "listen-before-TX" to reduce collisions.
    ////    syncSeen = true;
                        // Keep waiting for rest of message...
                        // At this point in theory we could know exactly how long to wait.
                        return;
                        }
                    }
                }

        public:
            // True if there is hardware interrupt support.
            // This might be dedicated to the radio, or shared with other devices.
            // Should be a compile-time constant.
            static const bool hasInterruptSupport = (RFM_nIRQ_DigitalPin >= 0);

            OTRFM23BLink() : OTRFM23BLinkBase(allowRX) { }

            // Do very minimal pre-initialisation, eg at power up, to get radio to safe low-power mode.
            // Argument is read-only pre-configuration data;
            // may be mandatory for some radio types, else can be NULL.
            // This pre-configuration data depends entirely on the radio implementation,
            // but could for example be a minimal set of register number/values pairs in ROM.
            // This routine must not lock up if radio is not actually available/fitted.
            // Argument is ignored for this implementation.
            // NOT INTERRUPT SAFE and should not be called concurrently with any other RFM23B/SPI operation.
            virtual void preinit(const void */*preconfig*/) { _powerOnInit(); }

            // Poll for incoming messages (eg where interrupts are not available) and other processing.
            // Can be used safely in addition to handling inbound/outbound interrupts.
            // Where interrupts are not available should be called at least as often
            // as messages are expected to arrive to avoid radio receiver overrun.
            // May also be used for output processing,
            // eg to run a transmit state machine.
            // May be called very frequently and should not take more than a few 100ms per call.
            virtual void poll() { if(!interruptLineIsEnabledAndInactive()) { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { _poll(); } } }

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
                if(!allowRX) { return(false); }
                if(interruptLineIsEnabledAndInactive()) { return(false); }
                _poll();
                return(true);
                }

            // Get current RSSI.
            // CURRENTLY RFM23B IMPL ONLY.
            // NOT OFFICIAL API: MAY BE WITHDRAWN AT ANY TIME.
            // Only valid when in RX mode.
            // Units as per RFM23B.
            //    RSSI [0..255] ~ [-120..20]dB 0.5 dB Steps
            //    where roughly
            //    RSSI [16..230] ~ [-120..0]dB 0.5 dB Steps
            //    RSSI [231] ~ [0.5..20]dB 0.5 dB Steps
            uint8_t getRSSI() const
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
            uint8_t getMode() const
                {
                ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
                    {
                    const bool neededEnable = _upSPI();
                    const uint8_t mode = 0xf & _readReg8Bit(REG_OP_CTRL1);
                    if(neededEnable) { _downSPI(); }
                    return(mode);
                    }
                }

            // Fetches the current inbound RX minimum queue capacity and maximum RX (and TX) raw message size.
            virtual void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen) const
                {
                queueRX.getRXCapacity(queueRXMsgsMin, maxRXMsgLen);
                maxTXMsgLen = MaxTXMsgLen;
                }

            // Fetches the current count of queued messages for RX.
            // ISR-/thread- safe.
            virtual uint8_t getRXMsgsQueued() const { return(queueRX.getRXMsgsQueued()); }

            // Peek at first (oldest) queued RX message, returning a pointer or NULL if no message waiting.
            // The pointer returned is NULL if there is no message,
            // else the pointer is to the start of the message/frame
            // and the length is in the byte before the start of the frame.
            // This allows a message to be decoded directly from the queue buffer
            // without copying or the use of another buffer.
            // The returned pointer and length are valid until the next
            //     peekRXMessage() or removeRXMessage()
            // This does not remove the message or alter the queue.
            // The buffer pointed to MUST NOT be altered.
            // Not intended to be called from an ISR.
            virtual const volatile uint8_t *peekRXMsg() const { return(queueRX.peekRXMsg()); }

            // Remove the first (oldest) queued RX message.
            // Typically used after peekRXMessage().
            // Does nothing if the queue is empty.
            // Not intended to be called from an ISR.
            virtual void removeRXMsg() { queueRX.removeRXMsg(); }

#if 0 // Defining the virtual destructor uses ~800+ bytes of Flash by forcing use of malloc()/free().
            // Ensure safe instance destruction when derived from.
            // by default attempts to shut down the sensor and otherwise free resources when done.
            // This uses ~800+ bytes of Flash by forcing use of malloc()/free().
            virtual ~OTRFM23BLink() { end(); }
#else
#define OTRFM23BLINK_NO_VIRT_DEST // Beware, no virtual destructor so be careful of use via base pointers.
#endif
        };


    // Library of common RFM23B configurations.
    // Only link in (refer to) those required at run-time.

    // Minimal register settings for FS20 (FHT8V) compatible 868.35MHz (EU band 48) OOK 5kbps carrier, no packet handler.
    // Provide RFM22/RFM23 register settings for use with FHT8V in Flash memory.
    // Consists of a sequence of (reg#,value) pairs terminated with a 0xff register number.  The reg#s are <128, ie top bit clear.
    // Magic numbers c/o Mike Stirling!
    // Note that this assumes default register settings in the RFM23B when powered up.
    extern const OTRFM23BLinkBase::RFM23_Reg_Values_t FHT8V_RFM23_Reg_Values;

    // Full register settings for 868.5MHz (EU band 48) GFSK 57.6kbps.
    // Full config including all default values, so safe for dynamic switching.
    extern const OTRFM23BLinkBase::RFM23_Reg_Values_t StandardRegSettingsGFSK57600;

    // Full register settings for FS20 (FHT8B) 868.35MHz (EU band 48) OOK 5kbps carrier, no packet handler.
    // Full config including all default values, so safe for dynamic switching.
    extern const OTRFM23BLinkBase::RFM23_Reg_Values_t StandardRegSettingsOOK5000;

    // Full register settings for 868.0MHz (EU band 48) GFSK 49.26 kbps.
    // Full config including all default values, so safe for dynamic switching.
    extern const OTRFM23BLinkBase::RFM23_Reg_Values_t StandardRegSettingsJeeLabs;
#endif // ARDUINO_ARCH_AVR


    }
#endif
