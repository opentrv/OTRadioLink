<blockquote>
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

Author(s) / Copyright (s): Milenko Alcin 2016
                           Damon Hart-Davis 2016
</blockquote>

#### JeeLabs / Open Energy Monitor OpenTRV channel and packet encoder/decoder

Possible receiver unit: RFM69CW > USB serial via ATmega328 in USB stick format: http://www.digitalsmarties.net/products/jeelink

The goal is to extend OTRadioLink library with channel that allows OpenTRV devices to communicate with Jeelabs,  OEM  and all other devices that use Jeelabs radio communications library. Library hides specifics of communication and packet format from the user, and eiter takes takes or returns complete payload from/to the application. In addition to payload, user has several flags available that might needed to acheve special goals. These flags are:

* **dest** - if dest = true, we are sending o specific node. The default is  dest = false, meaning that packet is broadcat
* **ackReq** - if this flags is set, the recipient should confirm reception of our message
* **ackConf** - if flag is set, we the packet is confirmation of previously received packet
* **nodeId** -  defines the node we are sedning to. Needs to be set together with dest. If packet is broadcast, this parameter is ignored and nodeID set using setNodeAndGroupID method is used.

Packet format and flags are described in: http://jeelabs.org/2011/06/10/rf12-broadcasts-and-acks/index.html

**Implementation consists of two parts:** 

* **RFM23BLink part** with register definition for new channel and several changes of existin RFM23BLink to support fixed packet length RFM23B mode.
* **JeelabsOemPacket class**, introduces encoding/decoding functionality, a callback method callable by interrupt routine, methods for setting and getting our own node and group IDs. 

One of the design requirements was to keep existing RFM23BLink API unchanged. This requirement dictated the way radio module is configured. Selected mode is packet, in a way where packet handler takes care of preamble and first SYN byte (0x2d), while the rest of Jeelabs/OEM packet format (second byte of SYN, header byte, length byte and CRC) are handled by SW. The most obvious consequence of this requirement was that although Jeelabs/OEM packet is of variable length, and we have length information available in the packet, radio modules packet handler could not use it, so we have to receive packets as if they are fixed length. In case of fixed packet, packet handler of RFM23B, once after it detects beginning of packet and starts reception, can not detect the ending and stop, but it receives and stores received info in buffer until some other criteria is fulfilled, regardless of whether it is regular data or noise picked on a line. In our case this criteria is maximum length of message. After maximum number of bytes for the message is received in FIFO, RFM23B receiver generates interrupt, and we have to read and temporarily store all the data bytes and  later decide what to do with it. The problem with that solution is that we need to keep relatively large buffer busy until suitable moment when the application SW processes it and applies decoder to the received buffer. 

In order to overcome this issue, OTRFM23BLink’s  receive  interrupt routine already implements a callback function, since OOK reception has similar issues. Function is called immediately after  packet is received, and decides whether packet is worth keeping and what is its actual size. Currently this filter function is not set automatically when setting receive channel, but user has to take care of it. 

Since filter routine is not installed automatically, we had to make sure that everything works even if filter is not installed. This again comes with certain price - some of the operations done by filter need to be repeated when decoding packet to make sure decoding will work correctly in situations when filter is not installed.

Transmit side required minimal change. Length of the packet to transmit had to be written in different register depending on whether packet handler is in fixed or variable length mode. 

Interrupt routine is also had to be changed for the same reason in order to get correct number of bytes to read depending on packet handler mode.
