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

namespace OTRFM23BLink
    {


//// Returns true iff RFM23 appears to be correctly connected.
//bool OTRFM23BLinkBase::checkConnected()
//  {
//  const bool neededEnable = upSPI();
//  bool isOK = false;
////  const uint8_t rType = _readReg8Bit(0); // May read as 0 if not connected at all.
////  if(RFM22_SUPPORTED_DEVICE_TYPE == rType)
////    {
////    const uint8_t rVersion = _readReg8Bit(1);
////    if(RFM22_SUPPORTED_DEVICE_VERSION == rVersion)
////      { isOK = true; }
////#if 0 && defined(DEBUG)
////    else
////      {
////      DEBUG_SERIAL_PRINT_FLASHSTRING("RFM22 bad version: ");
////      DEBUG_SERIAL_PRINTFMT(rVersion, HEX);
////      DEBUG_SERIAL_PRINTLN();
////      }
////#endif
////    }
////#if 0 && defined(DEBUG)
////  else
////    {
////    DEBUG_SERIAL_PRINT_FLASHSTRING("RFM22 bad type: ");
////    DEBUG_SERIAL_PRINTFMT(rType, HEX);
////    DEBUG_SERIAL_PRINTLN();
////    }
////#endif
////#if 1 && defined(DEBUG)
////  if(!isOK) { DEBUG_SERIAL_PRINTLN_FLASHSTRING("RFM22 bad"); }
////#endif
//  if(neededEnable) { downSPI(); }
//  return(isOK);
//  }

    }
