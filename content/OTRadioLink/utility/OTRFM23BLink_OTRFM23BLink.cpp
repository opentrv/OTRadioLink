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
                           Mike Stirling 2013 (RFM23B settings)
*/

/**TEMPORARILY IN OTRadioLink AREA BEFORE BEING MOVED TO OWN LIBRARY. */

#include <util/atomic.h>
#include <OTV0p2Base.h>
#include <OTRadioLink.h>

#include "OTRFM23BLink_OTRFM23BLink.h"
#include "OTV0P2BASE_Sleep.h"

namespace OTRFM23BLink {


// Set typical maximum frame length in bytes [1,63] to optimise radio behaviour.
// Too long may allow overruns, too short may make long-frame reception hard.
void OTRFM23BLinkBase::setMaxTypicalFrameBytes(const uint8_t _maxTypicalFrameBytes)
    {
    maxTypicalFrameBytes = max(1, min(_maxTypicalFrameBytes, 63));
    }

// Returns true iff RFM23 appears to be correctly connected.
bool OTRFM23BLinkBase::_checkConnected() const
    {
//    // Give radio time to start up.
//    OTV0P2BASE::nap(WDTO_15MS);

    const bool neededEnable = _upSPI_();

//    // If SPI was already up, power down, wait and power up again.
//    if(!neededEnable)
//      {
//      _downSPI_();
//      OTV0P2BASE::nap(WDTO_15MS);
//      _upSPI_();
//      }

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
        // FIXME: RFM23B probably unlikely to exceed 80kbps, thus at least 100uS per byte, so no point sleeping much less.
        OTV0P2BASE_busy_spin_delay(1000);
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

    // Transmit on the channel specified.
    _setChannel(channel);

//    // Disable all interrupts (eg to avoid invoking the RX handler).
//    _modeStandbyAndClearState_();

    // Load the frame into the TX FIFO.
    _queueFrameInTXFIFO(buf, buflen);

    // If in packet-handling mode then set the frame length.
    const bool neededEnable = _upSPI_();
    // Check if packet handling in RFM23B is enabled and set packet length
    if(_readReg8Bit_(REG_30_DATA_ACCESS_CONTROL) & RFM23B_ENPACTX)
       { _writeReg8Bit_(REG_3E_PACKET_LENGTH, buflen); }
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

//    // Unconditionally stop listening and go into low-power standby mode.
//    _modeStandbyAndClearState_();

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
           _writeReg8Bit_(REG_INT_ENABLE1, RFM23B_ENPKVALID);
           _writeReg8Bit_(REG_INT_ENABLE2, 0); 
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
    // Nothing to do if already on the correct channel.
    if(_currentChannel == channel) { return; }

    // Reject out-of-range channel requests.
    if(channel >= nChannels) { return; }

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
//    V0P2BASE_DEBUG_SERIAL_PRINT('n');
//    V0P2BASE_DEBUG_SERIAL_PRINT(nChannels);
//    V0P2BASE_DEBUG_SERIAL_PRINTLN();
//    V0P2BASE_DEBUG_SERIAL_PRINT('p');
//    V0P2BASE_DEBUG_SERIAL_PRINTFMT((intptr_t)StandardRegSettingsGFSK, HEX);
//    V0P2BASE_DEBUG_SERIAL_PRINTLN();
//    V0P2BASE_DEBUG_SERIAL_PRINTFMT((intptr_t)channelConfig[0].config, HEX);
//    V0P2BASE_DEBUG_SERIAL_PRINTLN();
//    V0P2BASE_DEBUG_SERIAL_PRINT('p');
//    V0P2BASE_DEBUG_SERIAL_PRINTFMT((intptr_t)StandardRegSettingsOOK, HEX);
//    V0P2BASE_DEBUG_SERIAL_PRINTLN();
//    V0P2BASE_DEBUG_SERIAL_PRINTFMT((intptr_t)channelConfig[1].config, HEX);
//    V0P2BASE_DEBUG_SERIAL_PRINTLN();

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





// Library of common RFM23B configurations.
// Only link in (refer to) those required at run-time.

// Minimal register settings for FS20 (FHT8V) compatible 868.35MHz (EU band 48) OOK 5kbps carrier, no packet handler.
// Provide RFM22/RFM23 register settings for use with FHT8V in Flash memory.
// Consists of a sequence of (reg#,value) pairs terminated with a 0xff register number.  The reg#s are <128, ie top bit clear.
// Magic numbers c/o Mike Stirling!
// Should match the RFM23_Reg_Values_t type for the RFM23B.
// Note that this assumes default register settings in the RFM23B when powered up.
const uint8_t FHT8V_RFM23_Reg_Values[][2] PROGMEM =
  {
  // Putting TX power setting first to help with dynamic adjustment.
// From AN440: The output power is configurable from +13 dBm to -8 dBm (Si4430/31), and from +20 dBM to -1 dBM (Si4432) in ~3 dB steps. txpow[2:0]=000 corresponds to min output power, while txpow[2:0]=111 corresponds to max output power.
// The maximum legal ERP (not TX output power) on 868.35 MHz is 25 mW with a 1% duty cycle (see IR2030/1/16).
//EEPROM ($6d,%00001111) ; RFM22REG_TX_POWER: Maximum TX power: 100mW for RFM22; not legal in UK/EU on RFM22 for this band.
//EEPROM ($6d,%00001000) ; RFM22REG_TX_POWER: Minimum TX power (-1dBm).
//#ifndef RFM22_IS_ACTUALLY_RFM23
//    #ifndef RFM22_GOOD_RF_ENV
//    {0x6d,0xd}, // RFM22REG_TX_POWER: RFM22 +14dBm ~25mW ERP with 1/4-wave antenna.
//    #else // Tone down for good RF backplane, etc.
//    {0x6d,0x9},
//    #endif
//#else
//    #ifndef RFM22_GOOD_RF_ENV
//    {0x6d,0xf}, // RFM22REG_TX_POWER: RFM23 max power (+13dBm) for ERP ~25mW with 1/4-wave antenna.
//    #else // Tone down for good RF backplane, etc.
    {0x6d,0xb}, // RF23B, good RF conditions.
//    #endif
//#endif

    {6,0}, // Disable default chiprdy and por interrupts.
    {8,0}, // RFM22REG_OP_CTRL2: ANTDIVxxx, RXMPK, AUTOTX, ENLDM

//#ifndef RFM22_IS_ACTUALLY_RFM23
//// For RFM22 with RXANT tied to GPIO0, and TXANT tied to GPIO1...
//    {0xb,0x15}, {0xc,0x12}, // Can be omitted FOR RFM23.
//#endif

// 0x30 = 0x00 - turn off packet handling
// 0x33 = 0x06 - set 4 byte sync
// 0x34 = 0x08 - set 4 byte preamble
// 0x35 = 0x10 - set preamble threshold (RX) 2 nybbles / 1 bytes of preamble.
// 0x36-0x39 = 0xaacccccc - set sync word, using end of RFM22-pre-preamble and start of FHT8V preamble.
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
//#ifdef USE_MODULE_FHT8VSIMPLE_RX // RX-specific settings, again c/o Mike S.
    {0x1c,0xc1}, {0x1d,0x40}, {0x1e,0xa}, {0x1f,3}, {0x20,0x96}, {0x21,0}, {0x22,0xda}, {0x23,0x74}, {0x24,0}, {0x25,0xdc},
    {0x2a,0x24},
    {0x2c,0x28}, {0x2d,0xfa}, {0x2e,0x29},
    {0x69,0x60}, // AGC enable: SGIN | AGCEN
//#endif

    { 0xff, 0xff } // End of settings.
  };
//#endif // defined(USE_MODULE_RFM22RADIOSIMPLE)

#if 0 // DHD20130226 dump
     00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
00 : 08 06 20 20 00 00 00 00 00 7F 06 15 12 00 00 00
01 : 00 00 20 00 03 00 01 00 00 01 14 00 C1 40 0A 03
02 : 96 00 DA 74 00 DC 00 1E 00 00 24 00 28 FA 29 08
03 : 00 00 0C 06 08 10 AA CC CC CC 00 00 00 00 00 00
04 : 00 00 00 FF FF FF FF 00 00 00 00 FF 08 08 08 10
05 : 00 00 DF 52 20 64 00 01 87 00 01 00 0E 00 00 00
06 : A0 00 24 00 00 81 02 1F 03 60 9D 00 01 0B 28 F5
07 : 20 21 20 00 00 73 64 00 19 23 01 03 37 04 37
#endif

// Full register settings for FS20 (FHT8V) compatible 868.35MHz (EU band 48) OOK 5kbps carrier, no packet handler.
// Full config including all default values, so safe for dynamic switching.
const uint8_t StandardRegSettingsOOK5000[][2] PROGMEM =
  {
#if 0 // From FHT8V - keep it here for reference (while testing, delete when finished)
      // Putting TX power setting first to help with dynamic adjustment.
// From AN440: The output power is configurable from +13 dBm to -8 dBm (Si4430/31), and from +20 dBM to -1 dBM (Si4432) in ~3 dB steps. txpow[2:0]=000 corresponds to min output power, while txpow[2:0]=111 corresponds to max output power.
// The maximum legal ERP (not TX output power) on 868.35 MHz is 25 mW with a 1% duty cycle (see IR2030/1/16).
//EEPROM ($6d,%00001111) ; RFM22REG_TX_POWER: Maximum TX power: 100mW for RFM22; not legal in UK/EU on RFM22 for this band.
//EEPROM ($6d,%00001000) ; RFM22REG_TX_POWER: Minimum TX power (-1dBm).
//#ifndef RFM22_IS_ACTUALLY_RFM23
//    #ifndef RFM22_GOOD_RF_ENV
//    {0x6d,0xd}, // RFM22REG_TX_POWER: RFM22 +14dBm ~25mW ERP with 1/4-wave antenna.
//    #else // Tone down for good RF backplane, etc.
//    {0x6d,0x9},
//    #endif
//#else
//    #ifndef RFM22_GOOD_RF_ENV
//    {0x6d,0xf}, // RFM22REG_TX_POWER: RFM23 max power (+13dBm) for ERP ~25mW with 1/4-wave antenna.
//    #else // Tone down for good RF backplane, etc.
    {0x6d,0xb}, // RF23B, good RF conditions.
//    #endif
//#endif

    {6,0}, // Disable default chiprdy and por interrupts.
    {8,0}, // RFM22REG_OP_CTRL2: ANTDIVxxx, RXMPK, AUTOTX, ENLDM

//#ifndef RFM22_IS_ACTUALLY_RFM23
//// For RFM22 with RXANT tied to GPIO0, and TXANT tied to GPIO1...
//    {0xb,0x15}, {0xc,0x12}, // Can be omitted FOR RFM23.
//#endif

// 0x30 = 0x00 - turn off packet handling
// 0x33 = 0x06 - set 4 byte sync
// 0x34 = 0x08 - set 4 byte preamble
// 0x35 = 0x10 - set preamble threshold (RX) 2 nybbles / 1 bytes of preamble.
// 0x36-0x39 = 0xaacccccc - set sync word, using end of RFM22-pre-preamble and start of FHT8V preamble.
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
//#ifdef USE_MODULE_FHT8VSIMPLE_RX // RX-specific settings, again c/o Mike S.
    {0x1c,0xc1}, {0x1d,0x40}, {0x1e,0xa}, {0x1f,3}, {0x20,0x96}, {0x21,0}, {0x22,0xda}, {0x23,0x74}, {0x24,0}, {0x25,0xdc},
    {0x2a,0x24},
    {0x2c,0x28}, {0x2d,0xfa}, {0x2e,0x29},
    {0x69,0x60}, // AGC enable: SGIN | AGCEN
#endif

//    Reg , val       Default R/W   Function/Desc                        Comment
//   0x00,  N/A        0x00    R  - Device Type
//   0x01,  N/A        0x06    R  - Device Version
//   0x02,  N/A         --     R  - Device Status
//   0x03,  N/A         --     R  - Interrupt Status 1
//   0x04,  N/A         --     R  - Interrupt Status 2
   { 0x05,    1 }, //  0x00   R/W - Interrupt Enable 1:                  ICRCERROR
   { 0x06,    0 }, //  0x03   R/W - Interrupt Enable 2
   { 0x07,    1 }, //  0x01   R/W - Operating &Function Control 1:       XTON
   { 0x08,    0 }, //  0x00   R/W - Operating &Function Control 2:
   { 0x09, 0x7f }, //  0x7F   R/W - Crystal Oscillator Load Capacitance:
   { 0x0a,    6 }, //  0x06   R/W - Microcontr Output Clock:             4MHz on DIO2
   { 0x0b, 0x15 }, //  0x00   R/W - GPIO0 Configuration:                 GPIO0=RX State
   { 0x0c, 0x12 }, //  0x00   R/W - GPIO1 Configuration:                 GPIO1=TX State
   { 0x0d,    0 }, //  0x00   R/W - GPIO2 Configuration:
   { 0x0e,    0 }, //  0x00   R/W - I/O Port Configuration:
   { 0x0f,    0 }, //  0x00   R/W - ADC Configuration:
   { 0x10,    0 }, //  0x00   R/W - ADC Sensor Amplifier:
//   0x11,  N/A         --     R  - ADC Value:
   { 0x12, 0x20 }, //  0x20   R/W - Temperature Sensor Control:
   { 0x13,    0 }, //  0x00   R/W - Temperature Value Offset:
   { 0x14,    3 }, //  0x03   R/W - Wake-Up Timer Period 1:
   { 0x15,    0 }, //  0x00   R/W - Wake-Up Timer Period 2:
   { 0x16,    1 }, //  0x01   R/W - Wake-Up Timer Period 3:
//   0x17,  N/A         --     R  -  Wake-Up Timer Value 1:
//   0x18,  N/A         --     R  -  Wake-Up Timer Value 2:
   { 0x19, 0x01 }, //  0x01   R/W - Low-Duty Cycle Mode Duration:
   { 0x1a, 0x14 }, //  0x14   R/W - Low Battery Detector Thr0xesold:
//   0x1b,  N/A         --     R  - Battery Voltage Level:
   { 0x1c, 0xc1 }, //  0x01   R/W - IF Filter Bandwidth:                 BW=4,9 kHz ?
   { 0x1d, 0x40 }, //  0x44   R/W - AFC Loop Gearshift Override:         ENAFC
   { 0x1e, 0x0a }, //  0x0a   R/W - AFC Timing Control:
   { 0x1f,    3 }, //  0x03   R/W - Clock Recovery Gearshift Override:
   { 0x20, 0x96 }, //  0x64   R/W - Clock Recovery Oversampling Rate:
   { 0x21,    0 }, //  0x01   R/W - Clock Recovery Offset 2:
   { 0x22, 0xda }, //  0x47   R/W - Clock Recovery Offset 1:
   { 0x23, 0x74 }, //  0xae   R/W - Clock Recovery Offset 0:
   { 0x24, 0x00 }, //  0x02   R/W - Clock Recovery Timing Loop Gain 1:
   { 0x25, 0xdc }, //  0x8f   R/W - Clock Recovery Timing Loop Gain 0:
//   0x26,  N/A         --     R  - Received Signal Strenght Indicator:
   { 0x27, 0x1e }, //  0x1e   R/W - RSSI Threshold for Clear Channel Indicator:
//   0x28,  N/A         --     R  - Antenna Diversity Register 1:
//   0x29,  N/A         --     R  - Antenna Diversity Register 2:
   { 0x2a, 0x24 }, //  0x00   R/W - AFC Limiter value:
//   0x2b,  N/A         --     R  - AFC Correction Read:
   { 0x2c, 0x28 }, //  0x18   R/W - OOK Counter Value 1:
   { 0x2d, 0xfa }, //  0xbc   R/W - OOK Counter Value 2:
   { 0x2e, 0x29 }, //  0x26   R/W - Slicer Peak Hold Reserved:
//   0x2f,  N/A         --          RESERVED
   { 0x30,    0 }, //  0x8d   R/W - Data Access Control:                 Packet handler disabled Rx & Txi, CRC disabled
//   0x31,  N/A         --     R  - EzMAC status:
   { 0x32, 0x0c }, //  0x0c   R/W - Header Control 1:
   { 0x33,    6 }, //  0x22   R/W - Header Control 2:                    4 bytes syn, no header
   { 0x34, 0x08 }, //  0x08   R/W - Preamble Length:                    32 bit preamble preamble
   { 0x35, 0x10 }, //  0x2a   R/W - Preamble Detection Control:          8 bit preabmle detection
   { 0x36, 0xaa }, //  0x2d   R/W - Sync Word 3:
   { 0x37, 0xcc }, //  0xd4   R/W - Sync Word 2:
   { 0x38, 0xcc }, //  0x00   R/W - Sync Word 1:
   { 0x39, 0xcc }, //  0x00   R/W - Sync Word 0:
   { 0x3a,    0 }, //  0x00   R/W - Transmit Header 3:
   { 0x3b,    0 }, //  0x00   R/W - Transmit Header 2:
   { 0x3c,    0 }, //  0x00   R/W - Transmit Header 1:
   { 0x3d,    0 }, //  0x00   R/W - Transmit Header 0:
   { 0x3e,    0 }, //  0x00   R/W - Transmit Packet Length:
   { 0x3f,    0 }, //  0x00   R/W - Check Header 3:
   { 0x40,    0 }, //  0x00   R/W - Check Header 2:
   { 0x41,    0 }, //  0x00   R/W - Check Header 1:
   { 0x42,    0 }, //  0x00   R/W - Check Header 0:
   { 0x43, 0xff }, //  0xff   R/W - Header Enable 3:
   { 0x44, 0xff }, //  0xff   R/W - Header Enable 2:
   { 0x45, 0xff }, //  0xff   R/W - Header Enable 1:
   { 0x46, 0xff }, //  0xff   R/W - Header Enable 0:
//   0x47,  N/A         --     R  - Received Header 3:
//   0x48,  N/A         --     R  - Received Header 2:
//   0x49,  N/A         --     R  - Received Header 1:
//   0x4a,  N/A         --     R  - Received Header 0:
//   0x4b,  N/A         --     R  - Received Packet Length:
//   0x4c-0x4e                      RESERVED
   { 0x4F, 0x10 }, //  0x10   R/W - ADC8 Control:
//   0x50-0x5f                      RESERVED
   { 0x60, 0xa0 }, //  0xa0   R/W - Channel Filter Coecfficient Address:
//   0x61,  N/A                     RESERVED
   { 0x62, 0x24 }, //  0x24   R/W - Crystal Oscillator/Power-on-Reset Control
//   0x63-0x68                      RESERVED
   { 0x69, 0x60 }, //  0x20   R/W - AGC Override:                         SGIN=1, AGCEN=1. PGA=0
//   0x6a-0x6c                      RESERVED
   { 0x6d, 0x0b }, //  0x18   R/W - TX Power:                             LNA_SW=1, TXPOW=3
   { 0x6e, 0x28 }, //  0x0A   R/W - TX Data Rate 1:                       5000 Hz
   { 0x6f, 0xf5 }, //  0x3D   R/W - TX Data Rate 0:
   { 0x70, 0x20 }, //  0x0c   R/W - Modulation Mode Control 1:            TXDTRTSCALE=1
   { 0x71, 0x21 }, //  0x00   R/W - Modulation Mode Control 2:            Source=FIFO, Modulation=OOK
   { 0x72, 0x20 }, //  0x20   R/W - Frequency Deviation:                  Fdev=20000kHz
   { 0x73,    0 }, //  0x00   R/W - Frequency Offset 1:
   { 0x74,    0 }, //  0x00   R/W - Frequency Offset 2:
   { 0x75, 0x73 }, //  0x75   R/W - Frequency Band Select:
   { 0x76, 0x64 }, //  0xbb   R/W - Nominal Carrier Frequency 1:
   { 0x77, 0x00 }, //  0x80   R/W - Nominal Carrier Frequency 0:          868,35 MHz
//   0x78,  N/A                     RESERVED
   { 0x79, 0x23 }, //  0x00   R/W - Frequency Hopping Channel Select:
   { 0x7a, 0x01 }, //  0x00   R/W - Frequency Hopping Step Size:
//   0x7b,  N/A                     RESERVED
   { 0x7c, 0x37 }, //  0x37   R/W - TX FIFO Control 1:
   { 0x7d,    4 }, //  0x04   R/W - TX FIFO Control 2:
   { 0x7e, 0x37 }, //  0x37   R/W - RX FIFO Control:
//   0x7f   N/A               R/W - FIFO Access
    { 0xff, 0xff } // End of settings.
  };

// Full register settings for 868.5MHz (EU band 48) GFSK 57.6kbps.
// Full config including all default values, so safe for dynamic switching.
const uint8_t StandardRegSettingsGFSK57600[][2] PROGMEM =
  {
//   Reg ,  Val       Default R/W   Function/Desc                       Comment
//
//   0x00,  N/A        0x08    R  - Device Type
//   0x01,  N/A        0x06    R  - Device Version
//   0x02,  N/A         --     R  - Device Status
//   0x03,  N/A         --     R  - Interrupt Status 1
//   0x04,  N/A         --     R  - Interrupt Status 2
   { 0x05,    0 }, //  0x00   R/W - Interrupt Enable 1:                  Interrupts are enabled in FW later on
   { 0x06,    0 }, //  0x03   R/W - Interrupt Enable 2
   { 0x07,    1 }, //  0x01   R/W - Operating &Function Control 1:       XTON
   { 0x08,    0 }, //  0x00   R/W - Operating &Function Control 2:
   { 0x09, 0x7f }, //  0x7F   R/W - Crystal Oscillator Load Capacitance:
   { 0x0a,    6 }, //  0x06   R/W - Microcontr Output Clock:             4MHz on DIO2
   { 0x0b, 0x15 }, //  0x00   R/W - GPIO0 Configuration:                 GPIO0=RX State
   { 0x0c, 0x12 }, //  0x00   R/W - GPIO1 Configuration:                 GPIO1=TX State
   { 0x0d,    0 }, //  0x00   R/W - GPIO2 Configuration:
   { 0x0e,    0 }, //  0x00   R/W - I/O Port Configuration:
   { 0x0f,    0 }, //  0x00   R/W - ADC Configuration:
   { 0x10,    0 }, //  0x00   R/W - ADC Sensor Amplifier:
//   0x11,  N/A         --     R  - ADC Value:
   { 0x12, 0x20 }, //  0x20   R/W - Temperature Sensor Control:
   { 0x13,    0 }, //  0x00   R/W - Temperature Value Offset:
   { 0x14,    3 }, //  0x03   R/W - Wake-Up Timer Period 1:
   { 0x15,    0 }, //  0x00   R/W - Wake-Up Timer Period 2:
   { 0x16,    1 }, //  0x01   R/W - Wake-Up Timer Period 3:
//   0x17,  N/A         --     R  -  Wake-Up Timer Value 1:
//   0x18,  N/A         --     R  -  Wake-Up Timer Value 2:
   { 0x19,    1 }, //  0x01   R/W - Low-Duty Cycle Mode Duration:
   { 0x1a, 0x14 }, //  0x14   R/W - Low Battery Detector Thr0xesold:
//   0x1b,  N/A         --     R  - Battery Voltage Level:
   { 0x1c,    6 }, //  0x01   R/W - IF Filter Bandwidth:                 BW=127,9 kHz
   { 0x1d, 0x44 }, //  0x44   R/W - AFC Loop Gea0xrsift Override:
   { 0x1e, 0x0a }, //  0x0a   R/W - AFC Timing Control:
   { 0x1f,    3 }, //  0x03   R/W - Clock Recovery Gearshift Override:
   { 0x20, 0x45 }, //  0x64   R/W - Clock Recovery Oversampling Ratio:
   { 0x21,    1 }, //  0x01   R/W - Clock Recovery Offset 2:
   { 0x22, 0xd7 }, //  0x47   R/W - Clock Recovery Offset 1:
   { 0x23, 0xdc }, //  0xae   R/W - Clock Recovery Offset 0:
   { 0x24, 0x07 }, //  0x02   R/W - Clock Recovery Timing Loop Gain 1:
   { 0x25, 0x6e }, //  0x8f   R/W - Clock Recovery Timing Loop Gain 0:
//   0x26,  N/A         --     R  - Received Signal Strenght Indicator:
   { 0x27, 0x1e }, //  0x1e   R/W - RSSI Threshold for Clear Channel Indicator:
//   0x28,  N/A         --     R  - Antenna Diversity Register 1:
//   0x29,  N/A         --     R  - Antenna Diversity Register 2:
   { 0x2a, 0x28 }, //  0x00   R/W - AFC Limiter:
//   0x2b,  N/A         --     R  - AFC Correction Read:
   { 0x2c, 0x40 }, //  0x18   R/W - OOK Counter Value 1:
   { 0x2d, 0x0a }, //  0xbc   R/W - OOK Counter Value 2:
   { 0x2e, 0x2d }, //  0x26   R/W - Slicer Peak Hold Reserved:
//   0x2f,  N/A         --          RESERVED
   { 0x30, 0x88 }, //  0x8d   R/W - Data Access Control:                 Packet mode enabled Rx & Tx
//   0x31,  N/A         --     R  - EzMAC status:
   { 0x32, 0x00 }, //  0x0c   R/W - Header Control 1:                    No header = 0x00
   { 0x33,    2 }, //  0x22   R/W - Header Control 2:                    2 bytes syn, no header
   { 0x34, 0x0a }, //  0x08   R/W - Preamble Length:                    40 bit preamble preamble
   { 0x35, 0x2a }, //  0x2a   R/W - Preamble Detection Control:         20 bit preabmle detection
   { 0x36, 0x2d }, //  0x2d   R/W - Sync Word 3:
   { 0x37, 0xd4 }, //  0xd4   R/W - Sync Word 2:
   { 0x38,    0 }, //  0x00   R/W - Sync Word 1:
   { 0x39,    0 }, //  0x00   R/W - Sync Word 0:
   { 0x3a,    0 }, //  0x00   R/W - Transmit Header 3:
   { 0x3b,    0 }, //  0x00   R/W - Transmit Header 2:
   { 0x3c,    0 }, //  0x00   R/W - Transmit Header 1:
   { 0x3d,    0 }, //  0x00   R/W - Transmit Header 0:
   { 0x3e,    0 }, //  0x00   R/W - Transmit Packet Length:
   { 0x3f,    0 }, //  0x00   R/W - Check Header 3:
   { 0x40,    0 }, //  0x00   R/W - Check Header 2:
   { 0x41,    0 }, //  0x00   R/W - Check Header 1:
   { 0x42,    0 }, //  0x00   R/W - Check Header 0:
   { 0x43, 0xff }, //  0xff   R/W - Header Enable 3:
   { 0x44, 0xff }, //  0xff   R/W - Header Enable 2:
   { 0x45, 0xff }, //  0xff   R/W - Header Enable 1:
   { 0x46, 0xff }, //  0xff   R/W - Header Enable 0:
//   0x47,  N/A         --     R  - Received Header 3:
//   0x48,  N/A         --     R  - Received Header 2:
//   0x49,  N/A         --     R  - Received Header 1:
//   0x4a,  N/A         --     R  - Received Header 0:
//   0x4b,  N/A         --     R  - Received Packet Length:
//   0x4c-0x4E                      RESERVED
   { 0x4f, 0x10 }, //  0x10   R/W - ADC8 Control:
//   0x50-0x5f                      RESERVED
   { 0x60, 0xa0 }, //  0xa0   R/W - Channel Filter Coecfficient Address:
//   0x61,  N/A                     RESERVED
   { 0x62, 0x24 }, //  0x24   R/W - Crystal Oscillator/Power-on-Reset Control
//   0x63-0x68                      RESERVED
   { 0x69, 0x60 }, //  0x20   R/W - AGC Override:                         SGIN=1, AGCEN=1
//   0x6a-0x6c                      RESERVED
   { 0x6d, 0x0b }, //  0x18   R/W - TX Power:                             LNA=1 for direct tie, TxPwr=3
   { 0x6e, 0x0e }, //  0x0A   R/W - TX Data Rate 1:                       57602 Hz
   { 0x6f, 0xbf }, //  0x3D   R/W - TX Data Rate 0:
   { 0x70, 0x0c }, //  0x0c   R/W - Modulation Mode Control 1:            Manchester Pream Polarity = 1
   { 0x71, 0x23 }, //  0x00   R/W - Modulation Mode Control 2:            Source=FIFO, Modulation=GFSK
   { 0x72, 0x2e }, //  0x20   R/W - Frequency Deviation:                  Fdev=28750Hz
   { 0x73,    0 }, //  0x00   R/W - Frequency Offset 1:
   { 0x74,    0 }, //  0x00   R/W - Frequency Offset 2:
   { 0x75, 0x73 }, //  0x75   R/W - Frequency Band Select:
   { 0x76, 0x6a }, //  0xbb   R/W - Nominal Carrier Frequency 1:
   { 0x77, 0x40 }, //  0x80   R/W - Nominal Carrier Frequency 0:          868,5MHz
//   0x78,  N/A                     RESERVED
   { 0x79,    0 }, //  0x00   R/W - Frequency Hopping Channel Select:
   { 0x7a,    0 }, //  0x00   R/W - Frequency Hopping Step Size:
//   0x7b,  N/A                     RESERVED
   { 0x7c, 0x37 }, //  0x37   R/W - TX FIFO Control 1:
   { 0x7d,    4 }, //  0x04   R/W - TX FIFO Control 2:
   { 0x7e, 0x37 }, //  0x37   R/W - RX FIFO Control:
//   0x7F   N/A               R/W - FIFO Access
   { 0xff, 0xff } // End of settings.
  };

// Full register settings for JeeLabsi/OEM compatible communications:
// with following parameters:
// 868.0MHz (EU band 48) FSK 49.261kHz
// Full config including all default values, so safe for dynamic switching.
const uint8_t StandardRegSettingsJeeLabs[][2] PROGMEM =
  {
//   Reg ,  Val       Default R/W   Function/Desc                       Comment
//
//   0x00,  N/A        0x08    R  - Device Type
//   0x01,  N/A        0x06    R  - Device Version
//   0x02,  N/A         --     R  - Device Status
//   0x03,  N/A         --     R  - Interrupt Status 1
//   0x04,  N/A         --     R  - Interrupt Status 2
   { 0x05,    0 }, //  0x00   R/W - Interrupt Enable 1:                  Interrupts are enabled in FW later on
   { 0x06,    0 }, //  0x03   R/W - Interrupt Enable 2
   { 0x07,    1 }, //  0x01   R/W - Operating &Function Control 1:       XTON
   { 0x08,    0 }, //  0x00   R/W - Operating &Function Control 2:
   { 0x09, 0x7f }, //  0x7F   R/W - Crystal Oscillator Load Capacitance:
   { 0x0a,    6 }, //  0x06   R/W - Microcontr Output Clock:             4MHz on DIO2
   { 0x0b, 0x15 }, //  0x00   R/W - GPIO0 Configuration:                 GPIO0=RX State
   { 0x0c, 0x12 }, //  0x00   R/W - GPIO1 Configuration:                 GPIO1=TX State
   { 0x0d,    0 }, //  0x00   R/W - GPIO2 Configuration:
   { 0x0e,    0 }, //  0x00   R/W - I/O Port Configuration:
   { 0x0f,    0 }, //  0x00   R/W - ADC Configuration:
   { 0x10,    0 }, //  0x00   R/W - ADC Sensor Amplifier:
//   0x11,  N/A         --     R  - ADC Value:
   { 0x12, 0x20 }, //  0x20   R/W - Temperature Sensor Control:
   { 0x13,    0 }, //  0x00   R/W - Temperature Value Offset:
   { 0x14,    3 }, //  0x03   R/W - Wake-Up Timer Period 1:
   { 0x15,    0 }, //  0x00   R/W - Wake-Up Timer Period 2:
   { 0x16,    1 }, //  0x01   R/W - Wake-Up Timer Period 3:
//   0x17,  N/A         --     R  -  Wake-Up Timer Value 1:
//   0x18,  N/A         --     R  -  Wake-Up Timer Value 2:
   { 0x19,    1 }, //  0x01   R/W - Low-Duty Cycle Mode Duration:
   { 0x1a, 0x14 }, //  0x14   R/W - Low Battery Detector Thr0xesold:
//   0x1b,  N/A         --     R  - Battery Voltage Level:
   { 0x1c, 0x9b }, //  0x01   R/W - IF Filter Bandwidth:                 BW=125,0 kHz
   { 0x1d, 0x44 }, //  0x44   R/W - AFC Loop Gea0xrsift Override:
   { 0x1e, 0x0a }, //  0x0a   R/W - AFC Timing Control:
   { 0x1f,    3 }, //  0x03   R/W - Clock Recovery Gearshift Override:
   { 0x20, 0x7a }, //  0x64   R/W - Clock Recovery Oversampling Ratio:
   { 0x21,    1 }, //  0x01   R/W - Clock Recovery Offset 2:
   { 0x22, 0x0d }, //  0x47   R/W - Clock Recovery Offset 1:
   { 0x23, 0x08 }, //  0xae   R/W - Clock Recovery Offset 0:
   { 0x24, 0x01 }, //  0x02   R/W - Clock Recovery Timing Loop Gain 1:
   { 0x25, 0x28 }, //  0x8f   R/W - Clock Recovery Timing Loop Gain 0:
//   0x26,  N/A         --     R  - Received Signal Strenght Indicator:
   { 0x27, 0x1e }, //  0x1e   R/W - RSSI Threshold for Clear Channel Indicator:
//   0x28,  N/A         --     R  - Antenna Diversity Register 1:
//   0x29,  N/A         --     R  - Antenna Diversity Register 2:
   { 0x2a, 0x28 }, //  0x00   R/W - AFC Limiter:
//   0x2b,  N/A         --     R  - AFC Correction Read:
   { 0x2c, 0x28 }, //  0x18   R/W - OOK Counter Value 1:
   { 0x2d, 0x19 }, //  0xbc   R/W - OOK Counter Value 2:
   { 0x2e, 0x27 }, //  0x26   R/W - Slicer Peak Hold Reserved:
//   0x2f,  N/A         --          RESERVED
   { 0x30, 0x88 }, //  0x8d   R/W - Data Access Control:                 Packet mode enabled Rx & Tx
//   0x31,  N/A         --     R  - EzMAC status:
   { 0x32, 0x00 }, //  0x0c   R/W - Header Control 1:                    No header = 0x00
   { 0x33, 8    }, //  0x22   R/W - Header Control 2:                   fix packet length, 1 byte syn, no header
   { 0x34, 0x0a }, //  0x08   R/W - Preamble Length:                    40 bit preamble preamble
   { 0x35, 0x2a }, //  0x2a   R/W - Preamble Detection Control:         20 bit preabmle detection
   { 0x36, 0x2d }, //  0x2d   R/W - Sync Word 3:
   { 0x37, 0xd4 }, //  0xd4   R/W - Sync Word 2:
   { 0x38,    0 }, //  0x00   R/W - Sync Word 1:
   { 0x39,    0 }, //  0x00   R/W - Sync Word 0:
   { 0x3a,    0 }, //  0x00   R/W - Transmit Header 3:
   { 0x3b,    0 }, //  0x00   R/W - Transmit Header 2:
   { 0x3c,    0 }, //  0x00   R/W - Transmit Header 1:
   { 0x3d,    0 }, //  0x00   R/W - Transmit Header 0:
   { 0x3e,   60 }, //  0x00   R/W - Transmit Packet Length:             Receive full FIFO
   { 0x3f,    0 }, //  0x00   R/W - Check Header 3:
   { 0x40,    0 }, //  0x00   R/W - Check Header 2:
   { 0x41,    0 }, //  0x00   R/W - Check Header 1:
   { 0x42,    0 }, //  0x00   R/W - Check Header 0:
   { 0x43, 0xff }, //  0xff   R/W - Header Enable 3:
   { 0x44, 0xff }, //  0xff   R/W - Header Enable 2:
   { 0x45, 0xff }, //  0xff   R/W - Header Enable 1:
   { 0x46, 0xff }, //  0xff   R/W - Header Enable 0:
//   0x47,  N/A         --     R  - Received Header 3:
//   0x48,  N/A         --     R  - Received Header 2:
//   0x49,  N/A         --     R  - Received Header 1:
//   0x4a,  N/A         --     R  - Received Header 0:
//   0x4b,  N/A         --     R  - Received Packet Length:
//   0x4c-0x4E                      RESERVED
   { 0x4f, 0x10 }, //  0x10   R/W - ADC8 Control:
//   0x50-0x5f                      RESERVED
   { 0x60, 0xa0 }, //  0xa0   R/W - Channel Filter Coecfficient Address:
//   0x61,  N/A                     RESERVED
   { 0x62, 0x24 }, //  0x24   R/W - Crystal Oscillator/Power-on-Reset Control
//   0x63-0x68                      RESERVED
   { 0x69, 0x60 }, //  0x20   R/W - AGC Override:                         SGIN=1, AGCEN=1
//   0x6a-0x6c                      RESERVED
   { 0x6d, 0x0b }, //  0x18   R/W - TX Power:                             LNA=1 for direct tie, TxPwr=3
   { 0x6e, 0x0c }, //  0x0A   R/W - TX Data Rate 1:                       49260 Hz
   { 0x6f, 0x9c }, //  0x3D   R/W - TX Data Rate 0:
   { 0x70, 0x0c }, //  0x0c   R/W - Modulation Mode Control 1:            Manchester Pream Polarity = 1
   { 0x71, 0x22 }, //  0x00   R/W - Modulation Mode Control 2:            Source=FIFO, Modulation=FSK
   { 0x72, 0x90 }, //  0x20   R/W - Frequency Deviation:                  Fdev=90kHz
   { 0x73,    0 }, //  0x00   R/W - Frequency Offset 1:
   { 0x74,    0 }, //  0x00   R/W - Frequency Offset 2:
   { 0x75, 0x73 }, //  0x75   R/W - Frequency Band Select:
   { 0x76, 0x64 }, //  0xbb   R/W - Nominal Carrier Frequency 1:
   { 0x77, 0x00 }, //  0x80   R/W - Nominal Carrier Frequency 0:          868,0MHz
//   0x78,  N/A                     RESERVED
   { 0x79,    0 }, //  0x00   R/W - Frequency Hopping Channel Select:
   { 0x7a,    0 }, //  0x00   R/W - Frequency Hopping Step Size:
//   0x7b,  N/A                     RESERVED
   { 0x7c, 0x37 }, //  0x37   R/W - TX FIFO Control 1:
   { 0x7d,    4 }, //  0x04   R/W - TX FIFO Control 2:
   { 0x7e, 0x37 }, //  0x37   R/W - RX FIFO Control:
//   0x7F   N/A               R/W - FIFO Access
   { 0xff, 0xff } // End of settings.
  };


}
