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

#include <string.h>
#include <OTRadioLink.h>
#include <OTV0p2Base.h>

#include "OTRadioLink_JeelabsOemPacket.h"
#include <util/crc16.h>


namespace OTRadioLink
    {
/*
 * Encode JeeLabs packet:
 *    buf     - holds payload, after enocding it points to the beggining of fomratted packet
 *    buflen  - length of payload
 *    nodeID  - if dest == 1, node ID is the address of the node we are seding to 
 *            - if dest == 0, value ignored, the default node ID will be used (see setNodeAndGroupId) 
 *    dest    - if set we are sending to specific node, otherwise we are broadcasting
 *    acqReq  - requesting acknowledgement
 *    ackConf - acknowledging previously received packet
 */
uint8_t JeelabsOemPacket::encode( uint8_t * const buf,  const uint8_t buflen,  const uint8_t nodeID,  const bool dest,  const bool ackReq,  const bool ackConf)
    {

       // Check if node is out of range
       if (nodeID > 31 ) return false; 

       // Move payload to the right place
       memmove(&buf[3],buf,buflen);

       // Format packet header
       buf[0] = _groupID;
       buf[1] = ackConf ? 0x80 : 0 | dest ? 0x40 : 0 | ackReq ? 0x20 : 0 | dest ? nodeID : _nodeID;
       buf[2] = buflen;

       uint16_t crc = calcCrc(buf, buflen+3);
       buf[buflen+3] = crc;
       buf[buflen+4] = crc>>8;
    
       return buflen+5;

    }

/*
 * Decode JeeLabs packet:
 *    buf     - holds packet, after deocding it points to the beggining of payload
 *    buflen  - length of packet
 *    nodeID  - if dest == 0, node ID of the sender
 *            - if dest == 1, our node ID (the same as set in setNodeAndGroupID)
 *    dest    - == 1 intended for us
              - == 0 broadcast packet
 *    acqReq  - acknowledgement requested
 *    ackConf - this packet is acknowledgement
 */
uint8_t JeelabsOemPacket::decode( uint8_t *  const buf, uint8_t * const buflen,  uint8_t *nodeID,  bool * const dest,  bool * const ackReq,  bool * const ackConf)
    {

       // Check if for our group
       if (buf[0] != _groupID) return -1; 
       // CRC OK?
       if ( calcCrc(buf, buf[2]+5))  return -2; 

       // Decode heder and if not broadcast, check if for us
       *nodeID  = buf[1] & 0x1f;
       *dest    = buf[1] & 0x40 ? true : false;  

       if ( *dest && (*nodeID != _nodeID) ) return -3;// not for us. Ignore

       *ackConf = buf[1] & 0x80 ? true : false;  
       *ackReq  = buf[1] & 0x20 ? true : false;  
       *buflen  = buf[2];
       
       // Move payload to the beggining of the buffer
       memmove(buf, &buf[3], *buflen);

       return *buflen;

    }

/*
 * Configure our own group and node ID.
 */
uint8_t JeelabsOemPacket::setNodeAndGroupID(const uint8_t nodeID, const uint8_t groupID) 
    {
       if (nodeID > 31 ) return false;
       _nodeID = nodeID;
       _groupID = groupID;
    }


/*
 * Calculste CRC. Used both in send and receive. Uses default crc routring from 
 * AVR standard clib
 */
uint16_t JeelabsOemPacket::calcCrc(const uint8_t* buf, uint8_t len) 
    {
       uint16_t crc = ~0;
       while (len--) 
         crc = _crc16_update( crc, *buf++);

       return crc;
    }
}
