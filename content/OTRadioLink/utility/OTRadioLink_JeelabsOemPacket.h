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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2016
*/

/*
 * Class for encoding and decoding JeeLabs/Open Energy Monitor packets.
 * 
 * For information on JeeLabs communications see:  
 * 
 * http://jeelabs.org/2011/06/10/rf12-broadcasts-and-acks/index.html
 *
 * On receive OTRRFM23B driver automatically processes and strips preamble and first SYN byte,i
 * and returns the rest of the packet with following structure: 
 *  
 *   | GroupID | HDR | len | Payload              | CRC |
 * 
 * Decode makes sure that packet is intended for our Group 
 * If dest bit (part of header byte) is set, Node ID is checked as well. 
 * Finally, CRC is checked, and payload is copied to the beggining of the buffer, while
 * header flags and nodeID variables are set accordingly.
 *
 * On transmit, operation is exactly opposite. Payload is received in buffer, while payload length and
 * other information that are needed to format the packet are passed as paramteres.
 * Method moves payload to the right place, formats packet header and add CRC.
 * 
 * Preamble and first syn byte are added by packet handler in OTRFM23B. 
 * 
 */

#ifndef ARDUINO_LIB_OTRADIOLINK_JEELABSOEMPACKET_H
#define ARDUINO_LIB_OTRADIOLINK_JEELABSOEMPACKET_H

#include <stdint.h>
#include <OTV0p2Base.h>
#include <OTRadioLink.h>


namespace OTRadioLink
    {

     class JeelabsOemPacket 
        {
        private:
        uint8_t _nodeID;
        uint8_t _groupID;
        uint16_t calcCrc(const uint8_t* buf, uint8_t len);
        public:
        // Default node ID chosen arbitrarily, group ID is JeeLabs default
        JeelabsOemPacket() { _nodeID = 5; _groupID = 100; }; 
        uint8_t setNodeAndGroupID(const uint8_t nodeID, const uint8_t groupID);
        uint8_t encode( uint8_t * const buf,  const uint8_t buflen,  const uint8_t nodeID = 0,  const bool dest = false,  const bool ackReq = false,  const bool ackConf = false);
        uint8_t decode( uint8_t *  const buf, uint8_t * const buflen,  uint8_t *nodeID,  bool * const dest,  bool * const ackReq,  bool * const ackConf);
        };
    }
#endif
