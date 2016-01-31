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

/**TEMPORARILY IN OTRadioLink AREA BEFORE BEING MOVED TO OWN LIBRARY. */

#include <util/atomic.h>
#include <OTV0p2Base.h>
#include <OTRadioLink.h>

#include "OTRFM23BLink_OTRFM23BLink.h"
#include "OTV0P2BASE_Sleep.h"

namespace OTRFM23BLink {

const uint8_t OTRFM23BLinkBase::StandardRegSettingsOOK[][2] =
  {

#if 0 // From FHT8V - keep it here for referenece (while testing, delete when finished)
    {0x6d,0xb}, // RF23B, good RF conditions.
    {6,0}, // Disable default chiprdy and por interrupts.
    {8,0}, // RFM22REG_OP_CTRL2: ANTDIVxxx, RXMPK, AUTOTX, ENLDM
    {0x30,0}, {0x33,6}, {0x34,8}, {0x35,0x10}, {0x36,0xaa}, {0x37,0xcc}, {0x38,0xcc}, {0x39,0xcc},

    {0x6e,40}, {0x6f,245}, // 5000bps, ie 200us/bit for FHT (6 for 1, 4 for 0).  10485 split across the registers, MSB first.
    {0x70,0x20}, // MOD CTRL 1: low bit rate (<30kbps), no Manchester encoding, no whitening.
    {0x71,0x21}, // MOD CTRL 2: OOK modulation.
    {0x72,0x20}, // Deviation GFSK. ; WAS EEPROM ($72,8) ; Deviation 5 kHz GFSK.
    {0x73,0}, {0x74,0}, // Frequency offset
// Channel 0 frequency = 868 MHz, 10 kHz channel steps, high band.
    {0x75,0x73}, {0x76,100}, {0x77,0}, // BAND_SELECT,FB(hz), CARRIER_FREQ0&CARRIER_FREQ1,FC(hz) where hz=868MHz
    {0x79,35}, // 868.35 MHz - FHT8V/FS20.
    {0x7a,1}, // One 10kHz channel step.
// RX-only
    {0x1c,0xc1}, {0x1d,0x40}, {0x1e,0xa}, {0x1f,3}, {0x20,0x96}, {0x21,0}, {0x22,0xda}, {0x23,0x74}, {0x24,0}, {0x25,0xdc},
    {0x2a,0x24},
    {0x2c,0x28}, {0x2d,0xfa}, {0x2e,0x29},
    {0x69,0x60}, // AGC enable: SGIN | AGCEN
    { 0xff, 0xff } // End of settings.
#endif
    { 0x05, 0x01 },
    { 0x06,    0 },
    { 0x07, 0x00 },
    { 0x08,    0 },
    { 0x0b, 0x15 },
    { 0x0c, 0x12 },
    { 0x1c, 0xc1 },
    { 0x1d, 0x40 },
    { 0x1f,    3 },
    { 0x20, 0x96 },
    { 0x21,    0 },
    { 0x22, 0xda },
    { 0x23, 0x74 },
    { 0x24,    0 },
    { 0x25, 0xdc },
    { 0x2c, 0x28 },
    { 0x2d, 0xfa },
    { 0x2e, 0x29 },
    { 0x2e, 0x29 },
    { 0x30, 0x00 },
    { 0x32, 0x0c },
    { 0x33, 0x06 },
    { 0x35, 0x10 },
    { 0x36, 0xaa },
    { 0x37, 0xcc },
    { 0x38, 0xcc },
    { 0x39, 0xcc },

    { 0x58,  0x0 }, // Milenko: I dont think it is needed

    { 0x69, 0x60 }, // AGC enable: SGIN | AGCEN
    { 0x6e, 0x28 }, // 5000bps, ie 200us/bit for FHT (6 for 1, 4 for 0).  10485 split across the registers, MSB first.
    { 0x6f, 0xf5 }, //
    { 0x70, 0x20 }, // MOD CTRL 1: low bit rate (<30kbps), no Manchester encoding, no whitening.
    { 0x71, 0x21 }, // MOD CTRL 2: OOK modulation.
    { 0x72, 0x20 }, // Deviation GFSK. ; WAS EEPROM ($72,8) ; Deviation 5 kHz GFSK.
    { 0x76, 0x64 }, 
    { 0x77, 0x00 }, 
    { 0x79, 0x23 }, 
    { 0x7a, 0x01 }, 

    { 0xff, 0xff } // End of settings.
  };

const uint8_t OTRFM23BLinkBase::StandardRegSettingsGFSK[][2] =
  {
    { 0x05, 0x07 },
    { 0x06, 0x40 },
    { 0x07, 0x01 },
    { 0x08,    0 },
    { 0x0b, 0x15 },
    { 0x0c, 0x12 },
    { 0x1c, 0x06 },
    { 0x1d, 0x44 },
    { 0x1f,    3 },
    { 0x20, 0x45 },
    { 0x21,    1 },
    { 0x22, 0xd7 },
    { 0x23, 0xdc },
    { 0x24,    7 },
    { 0x25, 0x6e },
    { 0x2c, 0x40 },
    { 0x2d, 0x0a },
    { 0x2e, 0x2d },

    { 0x30, 0x88 },
    { 0x32, 0x88 },
    { 0x33, 0x02 },
    { 0x35, 0x2a },
    { 0x36, 0x2d },
    { 0x37, 0xd4 },
    { 0x38, 0x00 },
    { 0x39, 0x00 },

    { 0x58, 0x80 }, // Milenko: I dont think it is needed

    { 0x69, 0x60 }, 
    { 0x6e, 0x0e },
    { 0x6f, 0xbf }, 
    { 0x70, 0x0c }, 
    { 0x71, 0x23 }, 
    { 0x72, 0x2e }, 
    { 0x76, 0x6b }, 
    { 0x77, 0x80 }, 
    { 0x79, 0x00 }, 
    { 0x7a, 0x00 }, 

    { 0xff, 0xff } // End of settings.
  };
// Set typical maximum frame length in bytes [1,63] to optimise radio behaviour.
// Too long may allow overruns, too short may make long-frame reception hard.
void OTRFM23BLinkBase::setMaxTypicalFrameBytes(const uint8_t _maxTypicalFrameBytes)
    {
    maxTypicalFrameBytes = max(1, min(_maxTypicalFrameBytes, 63));
    }

// Returns true iff RFM23 appears to be correctly connected.
bool OTRFM23BLinkBase::_checkConnected() const
    {
    const bool neededEnable = _upSPI_();
    bool isOK = false;
    const uint8_t rType = _readReg8Bit_(0); // May read as 0 if not connected at all.
    if(SUPPORTED_DEVICE_TYPE == rType)
        {
        const uint8_t rVersion = _readReg8Bit_(1);
        if(SUPPORTED_DEVICE_VERSION == rVersion)
            { isOK = true; }
        }
#if 0 && defined(V0P2BASE_DEBUG)
if(!isOK) { V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("RFM23 bad"); }
#endif
    if(neededEnable) { _downSPI_(); }
    return(isOK);
    }

// Configure the radio from a list of register/value pairs in readonly PROGMEM/Flash, terminating with an 0xff register value.
// NOTE: argument is not a pointer into SRAM, it is into PROGMEM!
void OTRFM23BLinkBase::_registerBlockSetup(const uint8_t registerValues[][2])
    {
    // Lock out interrupts.
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
        const bool neededEnable = _upSPI_();
        for( ; ; )
            {
            const uint8_t reg = pgm_read_byte(&(registerValues[0][0]));
            const uint8_t val = pgm_read_byte(&(registerValues[0][1]));
            if(0xff == reg) { break; }
#if 0 && defined(V0P2BASE_DEBUG)
            V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("RFM23 reg 0x");
            V0P2BASE_DEBUG_SERIAL_PRINTFMT(reg, HEX);
            V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING(" = 0x");
            V0P2BASE_DEBUG_SERIAL_PRINTFMT(val, HEX);
            V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif
            _writeReg8Bit_(reg, val);
            ++registerValues;
            }
        if(neededEnable) { _downSPI_(); }
        }
    }

// Clear TX FIFO.
// SPI must already be configured and running.
void OTRFM23BLinkBase::_clearTXFIFO()
    {
    _writeReg8Bit_(REG_OP_CTRL2, 1); // FFCLRTX
    _writeReg8Bit_(REG_OP_CTRL2, 0);
    }

// Clears the RFM23B TX FIFO and queues the supplied frame to send via the TX FIFO.
// This routine does not change the frame area.
// This uses an efficient burst write.
void OTRFM23BLinkBase::_queueFrameInTXFIFO(const uint8_t *bptr, uint8_t buflen)
    {
#if 0 && defined(V0P2BASE_DEBUG)
    if(0 == *bptr) { V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING("RFM22QueueCmdToFF: buffer uninitialised"); panic(); }
#endif
    // Lock out interrupts.
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
        const bool neededEnable = _upSPI_();
        // Clear the TX FIFO.
        _clearTXFIFO();

        // Select RFM23B for duration of batch/burst write.
        _SELECT_();
        _wr(REG_FIFO | 0x80); // Start burst write to TX FIFO.
        while(buflen-- > 0) { _wr(*bptr++); }
        // Burst write finished; deselect RFM23B.
        _DESELECT_();

        if(neededEnable) { _downSPI_(); }
        }
    }

// Transmit contents of on-chip TX FIFO: caller should revert to low-power standby mode (etc) if required.
// Returns true if packet apparently sent correctly/fully.
// Does not clear TX FIFO (so possible to re-send immediately).
bool OTRFM23BLinkBase::_TXFIFO()
    {
    const bool neededEnable = _upSPI_();

    // Lock out interrupts while fiddling with interrupts and starting the TX.
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
    //    // Enable interrupt on packet send ONLY.
    //    _writeReg8Bit_(REG_INT_ENABLE1, 4);
    //    _writeReg8Bit_(REG_INT_ENABLE2, 0);
        // Disable all interrupts (eg to avoid invoking the RX ISR).
        _writeReg8Bit_(REG_INT_ENABLE1, 0);
        _writeReg8Bit_(REG_INT_ENABLE2, 0);
        _clearInterrupts_();
        // Enable TX mode and transmit TX FIFO contents.
        _modeTX_();
        }

    // RFM23B data sheet claims up to 800uS from standby to TX; be conservative,
    //::OTV0P2BASE::_delay_x4(250); // Spin CPU for ~1ms; does not depend on timer1, etc.
    OTV0P2BASE_busy_spin_delay(1000);

    // Repeatedly nap until packet sent, with upper bound of ~120ms on TX time in case there is a problem.
    // (TX time is ~1.6ms per byte at 5000bps.)
    // DO NOT block interrupts while waiting for TX to complete!
    // Status is failed until RFM23B gives positive confirmation of frame sent.
    bool result = false;
    // Spin until TX complete or timeout.
    for(int i = MAX_TX_ms; --i >= 0; )
        {
        // Spin CPU for ~1ms; does not depend on timer1, delay(), millis(), etc, Arduino support.
//        ::OTV0P2BASE::_delay_x4(250);
        // RFM23B probably unlikely to exceed 80kbps, thus at least 100uS per byte, so no point sleeping much less.
        OTV0P2BASE_busy_spin_delay(100);
        // FIXME: don't have nap() support yet // nap(WDTO_15MS, true); // Sleep in low power mode for a short time waiting for bits to be sent...
        const uint8_t status = _readReg8Bit_(REG_INT_STATUS1); // TODO: could use nIRQ instead if available.
        if(status & 4) { result = true; break; } // Packet sent!
        }

    if(neededEnable) { _downSPI_(); }
    return(result);
    }

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
bool OTRFM23BLinkBase::sendRaw(const uint8_t *const buf, const uint8_t buflen, const int8_t channel, const TXpower power, const bool listenAfter)
    {
    // FIXME: currently ignores all hints.

    // Should not need to lock out interrupts while sending
    // as no poll()/ISR should start until this completes,
    // but will need to stop any RX in process,
    // eg to avoid TX FIFO buffer being zapped during RX handling.

    // Disable all interrupts (eg to avoid invoking the RX handler).
    _modeStandbyAndClearState_();

    _setChannel(channel);

    // Load the frame into the TX FIFO.
    _queueFrameInTXFIFO(buf, buflen);

    // Channel 0 is alsways GFSK
    // Move this into _TXFIFO
    const bool neededEnable = _upSPI_();

    // Check if packet handling in RFM23B is enabled and set packet length
    if ( _readReg8Bit_(REG_30_DATA_ACCESS_CONTROL) & RFM23B_ENPACTX )  {
       _writeReg8Bit_(REG_3E_PACKET_LENGTH, buflen); 
    }
    if(neededEnable) { _downSPI_(); }

    // Send the frame once.
    bool result = _TXFIFO();
    // For maximum 'power' attempt to resend the frame again after a short delay.
    if(power >= TXmax)
        {
        // Wait a little before retransmission.
#ifndef OTV0P2BASE_IDLE_NOT_RECOMMENDED
        ::OTV0P2BASE::_idleCPU(WDTO_15MS, false); // FIXME: make this a configurable delay.
#else
        ::OTV0P2BASE::nap(WDTO_15MS); // FIXME: make this a configurable delay.
//        ::OTV0P2BASE::delay_ms(15); // FIXME: seems a shame to burn cycles/juice here...
#endif

        // Resend the frame.
        if(!_TXFIFO()) { result = false; }
        }
    // TODO: listen-after-send if requested.

    // Revert to RX mode if listening, else go to standby to save energy.
    _dolisten();

    return(result);
    }

// Switch listening off, on to selected channel.
// listenChannel will have been set by time this is called.
// This always switches to standby mode first, then switches on RX as needed.
void OTRFM23BLinkBase::_dolisten()
    {
    // Unconditionally stop listening and go into low-power standby mode.
    _modeStandbyAndClearState_();

    // Nothing further to do if not listening.
    const int8_t lc = getListenChannel();
    if(-1 == lc) { return; }

    // Ensure on right channel.
    _setChannel(lc);

    // Disable interrupts while enabling them at RFM23B and entering RX mode.
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
        const bool neededEnable = _upSPI_();

        // Clear RX and TX FIFOs.
        _writeReg8Bit_(REG_OP_CTRL2, 3); // FFCLRRX | FFCLRTX
        _writeReg8Bit_(REG_OP_CTRL2, 0);

        // Set FIFO RX almost-full threshold as specified.
        _writeReg8Bit_(REG_RX_FIFO_CTRL, maxTypicalFrameBytes); // 55 is the default.

        // Enable requested RX-related interrupts.
        // Do this regardless of hardware interrupt support on the board.
        // Check if packet handling in RFM23B is enabled and enable interrupts accordingly.
        if ( _readReg8Bit_(REG_30_DATA_ACCESS_CONTROL) & RFM23B_ENPACRX )  {
           _writeReg8Bit_(REG_INT_ENABLE1, RFM23B_ENPKVALID); // enable all interrupts
           _writeReg8Bit_(REG_INT_ENABLE2, 0); // enable all interrupts
        }
        else {
           _writeReg8Bit_(REG_INT_ENABLE1, 0x10); // enrxffafull: Enable RX FIFO Almost Full.
           _writeReg8Bit_(REG_INT_ENABLE2, WAKE_ON_SYNC_RX ? 0x80 : 0); // enswdet: Enable Sync Word Detected.
       }

        // Clear any current interrupt/status.
        _clearInterrupts_();

        // Start listening.
        _modeRX_();

        if(neededEnable) { _downSPI_(); }
        }
    }

// Put RFM23 into standby, attempt to read bytes from FIFO into supplied buffer.
// Leaves RFM22 in low-power standby mode.
// Trailing bytes (more than were actually sent) undefined.
void OTRFM23BLinkBase::_RXFIFO(uint8_t *buf, const uint8_t bufSize)
    {
    // Lock out interrupts.
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
        const bool neededEnable = _upSPI_();

        _modeStandby_();

        // Do burst read from RX FIFO.
        _SELECT_();
        _io(REG_FIFO & 0x7F);
        for(int i = 0; i < bufSize; ++i)
            { *buf++ = _io(0); }
        _DESELECT_();

        // Clear RX and TX FIFOs simultaneously.
        _writeReg8Bit_(REG_OP_CTRL2, 3); // FFCLRRX | FFCLRTX
        _writeReg8Bit_(REG_OP_CTRL2, 0); // Needs both writes to clear.
        // Disable all interrupts.
        _writeReg8Bit_(REG_INT_ENABLE1, 0);
        _writeReg8Bit_(REG_INT_ENABLE2, 0); // TODO: possibly combine in burst write with previous...
    //    _writeReg16Bit0_(REG_INT_ENABLE1);
        // Clear any interrupts already/still pending...
        _clearInterrupts_();

        if(neededEnable) { _downSPI_(); }
        }
    }

// Configure radio for transmission via specified channel < nChannels; non-negative.
void OTRFM23BLinkBase::_setChannel(const uint8_t channel)
    {
      // Reject out-of-range channel requests.
      if(channel >= nChannels) { return; }

      // Nothing to do if already on the correct channel.
      if(_currentChannel == channel) { return; }

//      if (channel == 0)
//           _registerBlockSetup((regValPair_t *) StandardRegSettingsOOK);
//      else
//           _registerBlockSetup((regValPair_t *) StandardRegSettingsGFSK);

      // Set up registers for new config.
      _registerBlockSetup((regValPair_t *) (channelConfig[channel].config));

#if 0 && defined(MILENKO_DEBUG)
      V0P2BASE_DEBUG_SERIAL_PRINT("C:");
      V0P2BASE_DEBUG_SERIAL_PRINT(channel);
      V0P2BASE_DEBUG_SERIAL_PRINTLN();
      //readRegs((uint8_t)0,(uint8_t)0x7e);
#endif

      // Remember channel now in use.
      _currentChannel = channel;
    }

#if 0 && defined(MILENKO_DEBUG)
void OTRFM23BLinkBase::printHex(int val)  
    {
       if (val < 16)
         V0P2BASE_DEBUG_SERIAL_PRINT("0");
       V0P2BASE_DEBUG_SERIAL_PRINTFMT(val, HEX);
    }

void OTRFM23BLinkBase::readRegs(uint8_t from, uint8_t to, uint8_t noHeader)
   {
      uint8_t regVal;

      const bool neededEnable = _upSPI_();

      if ( noHeader == 0) {
         V0P2BASE_DEBUG_SERIAL_PRINTLN();
         V0P2BASE_DEBUG_SERIAL_PRINT("    00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
      }
      if ( from%16) {
         V0P2BASE_DEBUG_SERIAL_PRINTLN();
         printHex(from&0xf0);
         V0P2BASE_DEBUG_SERIAL_PRINT(":");
      }
      uint8_t i;
      for ( i = 0; i <= from%16; i++) V0P2BASE_DEBUG_SERIAL_PRINT("   ");
      for ( i = from; i <= to; i++) {
         regVal = _readReg8Bit_(i); 
         if (i % 16 == 0) {
            V0P2BASE_DEBUG_SERIAL_PRINTLN();
            printHex(i);
            V0P2BASE_DEBUG_SERIAL_PRINT(":");
         }
         V0P2BASE_DEBUG_SERIAL_PRINT(" ");
         printHex(regVal);
      }
      V0P2BASE_DEBUG_SERIAL_PRINTLN();

      if(neededEnable) { _downSPI_(); }
}
#endif

// Begin access to (initialise) this radio link if applicable and not already begun.
// Returns true if it successfully began, false otherwise.
// Allows logic to end() if required at the end of a block, etc.
bool OTRFM23BLinkBase::begin()
    {
    //if(1 != nChannels) { return(false); } // Can only handle a single channel.
    if(!_checkConnected()) { return(false); }
    // Set registers for default (0) channel.
    _registerBlockSetup((regValPair_t *) (channelConfig[0].config));
    _modeStandbyAndClearState_();
    return(true);
    }

// End access to this radio link if applicable and not already ended.
// Returns true if it needed to be ended.
// Shuts down radio in safe low-power state.
bool OTRFM23BLinkBase::end()
    {
    _modeStandbyAndClearState_();
    return(true);
    }

}
