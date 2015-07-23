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

#include "OTRFM23BLink_OTRFM23BLink.h"

#include <util/atomic.h>

/**TEMPORARILY IN OTRadioLink AREA BEFORE BEING MOVED TO OWN LIBRARY. */

namespace OTRFM23BLink {

// Set typical maximum frame length in bytes [1--63] to optimise radio behaviour.
// Too long may allow overruns, too short may make long-frame reception hard.
void OTRFM23BLinkBase::setMaxTypicalFrameBytes(const uint8_t _maxTypicalFrameBytes)
    {
    maxTypicalFrameBytes = max(1, min(X, 63));
    }

// Returns true iff RFM23 appears to be correctly connected.
bool OTRFM23BLinkBase::_checkConnected()
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
#if 0 && defined(DEBUG)
if(!isOK) { DEBUG_SERIAL_PRINTLN_FLASHSTRING("RFM23 bad"); }
#endif
    if(neededEnable) { _downSPI_(); }
    return(isOK);
    }

// Configure the radio from a list of register/value pairs in readonly PROGMEM/Flash, terminating with an 0xff register value.
// NOTE: argument is not a pointer into SRAM, it is into PROGMEM!
void OTRFM23BLinkBase::_registerBlockSetup(const uint8_t registerValues[][2])
    {
    const bool neededEnable = _upSPI_();
    for( ; ; )
        {
        const uint8_t reg = pgm_read_byte(&(registerValues[0][0]));
        const uint8_t val = pgm_read_byte(&(registerValues[0][1]));
        if(0xff == reg) { break; }
#if 0 && defined(DEBUG)
        DEBUG_SERIAL_PRINT_FLASHSTRING("RFM23 reg 0x");
        DEBUG_SERIAL_PRINTFMT(reg, HEX);
        DEBUG_SERIAL_PRINT_FLASHSTRING(" = 0x");
        DEBUG_SERIAL_PRINTFMT(val, HEX);
        DEBUG_SERIAL_PRINTLN();
#endif
        _writeReg8Bit_(reg, val);
        ++registerValues;
        }
    if(neededEnable) { _downSPI_(); }
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
#if 0 && defined(DEBUG)
    if(0 == *bptr) { DEBUG_SERIAL_PRINTLN_FLASHSTRING("RFM22QueueCmdToFF: buffer uninitialised"); panic(); }
#endif
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

// Transmit contents of on-chip TX FIFO: caller should revert to low-power standby mode (etc) if required.
// Returns true if packet apparently sent correctly/fully.
// Does not clear TX FIFO (so possible to re-send immediately).
bool OTRFM23BLinkBase::_TXFIFO()
    {
    const bool neededEnable = _upSPI_();
    // Enable interrupt on packet send ONLY.
    _writeReg8Bit_(REG_INT_ENABLE1, 4);
    _writeReg8Bit_(REG_INT_ENABLE2, 0);
    _clearInterrupts_();
    _modeTX_(); // Enable TX mode and transmit TX FIFO contents.

    // Repeatedly nap until packet sent, with upper bound of ~120ms on TX time in case there is a problem.
    // TX time is ~1.6ms per byte at 5000bps.
    bool result = false; // Usual case is success.
    // for(int8_t i = 8; --i >= 0; )
    for( ; ; ) // FIXME: no timeout criterion yet
        {
        // FIXME: don't have nap() support yet // nap(WDTO_15MS); // Sleep in low power mode for a short time waiting for bits to be sent...
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
    // FIXME: ignores channel entirely.
    // FIXME: currently ignores all hints.

    // Should not need to lock out interrupts while sending
    // as no poll()/ISR should start until this completes.

    // Load the frame into the TX FIFO.
    _queueFrameInTXFIFO(buf, buflen);
    // Send the frame once.
    bool result = _TXFIFO();
//        if(power >= TXmax)
//            {
//            nap(WDTO_15MS); // FIXME: no nap() support yet // Sleeping with interrupts disabled?
//            // Resend the frame.
//            if(!_TXFIFO()) { result = false; }
//            }
    // TODO: listen-after-send if requested.

    // Revert to RX mode if listening, else go to standby to save energy.
    _dolisten();

    return(result);
    }

// Switch listening off, on on to selected channel.
// listenChannel will have been set by time this is called.
// This always switches to standby mode first, then switches on RX as needed.
void OTRFM23BLinkBase::_dolisten()
    {
    // Unconditionally stop listening and go into low-power standby mode.
    _modeStandbyAndClearState_();

    // Nothing further to do if not listening.
    const int8_t lc = getListenChannel();
    if(-1 == lc) { return; }

    // TODO: ignores channel.

    // Ensure listening.
//RFM22SetUpRX(MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE, true, true); // Set to RX longest-possible valid FS20 encoded frame.
//// Create stream of bytes to be transmitted to FHT80V at 200us per bit, msbit of each byte first.
//// Byte stream is terminated by 0xff byte which is not a possible valid encoded byte.
//// On entry the populated FHT8V command struct is passed by pointer.
//// On exit, the memory block starting at buffer contains the low-byte, msbit-first bit, 0xff terminated TX sequence.
//// The maximum and minimum possible encoded message sizes are 35 (all zero bytes) and 45 (all 0xff bytes) bytes long.
//// Note that a buffer space of at least 46 bytes is needed to accommodate the longest-possible encoded message plus terminator.
//// Returns pointer to the terminating 0xff on exit.
//uint8_t *FHT8VCreate200usBitStreamBptr(uint8_t *bptr, const fht8v_msg_t *command);
//#define MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE 46 // For longest-possible encoded command plus terminating 0xff.

    // Disable interrupts while enabling them at RFM23B and entering RX mode.
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
        const bool neededEnable = _upSPI_();

        // Clear RX and TX FIFOs.
        _writeReg8Bit_(REG_OP_CTRL2, 3); // FFCLRRX | FFCLRTX
        _writeReg8Bit_(REG_OP_CTRL2, 0);

        // Set FIFO RX almost-full threshold as specified.
    //    _RFM22WriteReg8Bit(RFM22REG_RX_FIFO_CTRL, min(nearlyFullThreshold, 63));
        _writeReg8Bit_(REG_RX_FIFO_CTRL, maxTypicalFrameBytes); // 55 is the default.

        // Enable requested RX-related interrupts.
        // Do this regardless of hardware interrupt support on the board.
        _writeReg8Bit_(REG_INT_ENABLE1, 0x10); // enrxffafull: Enable RX FIFO Almost Full.
        _writeReg8Bit_(REG_INT_ENABLE2, WAKE_ON_SYNC_RX ? 0x80 : 0); // enswdet: Enable Sync Word Detected.

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
    const bool neededEnable = _upSPI_();

    _modeStandby_();

    // Do burst read from RX FIFO.
    _SELECT_();
    _io(REG_FIFO & 0x7F);
    for(int i = 0; i < bufSize; ++i)
        { *buf++ = _io(0);  }
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

// Fetches the first (oldest) queued RX message, returning its length, or 0 if no message waiting.
// If the waiting message is too long it is truncated to fit,
// so allocating a buffer at least one longer than any valid message
// should indicate an oversize inbound message.
uint8_t OTRFM23BLinkBase::getRXMsg(uint8_t *buf, uint8_t buflen)
    {
    // Argument validation.
    if(NULL == buf) { return(0); }
    if(0 == buflen) { return(0); }

    // Lock out interrupts to safely access the queue/buffers.
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
//      // Data structures that make up the queue...
//        // Current count of received messages queued.
//        // Marked volatile for ISR-/thread- safe access without a lock.
//        volatile uint8_t queuedRXedMessageCount;
//
//        // 1-deep RX queue and buffer used to accept data during RX.
//        // Marked as volatile for ISR-/thread- safe (sometimes lock-free) access.
//        volatile uint8_t lengthRX; // Non-zero when a frame is waiting.
//        volatile uint8_t bufferRX[MaxRXMsgLen];

        if(0 == queuedRXedMessageCount) { return(0); }
        if(0 == lengthRX) { return(0); }

        // Copy into caller's buffer up to its capacity.
        const uint8_t len = min(buflen, lengthRX);
        memcpy(buf, (const uint8_t *)bufferRX, len);

        // Update the data structures to mark the queue as now empty.
        lengthRX = 0;
        queuedRXedMessageCount = 0;
        bufferRX[0] = 0; // Help mark frame as empty; could fully zero for security.

        return(len);
        }

    return(0);
    }


// Begin access to (initialise) this radio link if applicable and not already begun.
// Returns true if it successfully began, false otherwise.
// Allows logic to end() if required at the end of a block, etc.
bool OTRFM23BLinkBase::begin()
    {
//  // Check that the radio is correctly connected; panic if not...
//  if(!RFM22CheckConnected()) { panic(); }
//  // Configure the radio.
//  RFM22RegisterBlockSetup(FHT8V_RFM22_Reg_Values);
//  // Put the radio in low-power standby mode.
//  RFM22ModeStandbyAndClearState();
    if(1 != nChannels) { return(false); } // Can only handle a single channel.
    if(!_checkConnected()) { return(false); }
    _registerBlockSetup((const regValPair_t *)(channelConfig->config));
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
