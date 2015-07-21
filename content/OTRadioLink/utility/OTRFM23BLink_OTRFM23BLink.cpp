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

/**TEMPORARILY IN TEST SKETCH BEFORE BEING MOVED TO OWN LIBRARY. */

namespace OTRFM23BLink {


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

// Begin access to (initialise) this radio link if applicable and not already begun.
// Returns true if it successfully began, false otherwise.
// Allows logic to end() if required at the end of a block, etc.
// Defaults to do nothing (and return false).
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
