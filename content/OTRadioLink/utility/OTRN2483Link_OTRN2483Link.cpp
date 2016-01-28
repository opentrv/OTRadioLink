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

Author(s) / Copyright (s): Deniz Erbilgin 2016
*/

#include "OTRN2483Link_OTRN2483Link.h"

namespace OTRN2483Link
{

OTRN2483Link::OTRN2483Link() : ser(8, 5), config(NULL), rxPin(8), txPin(5), resetPin(A2) {
	bAvailable = false;
	// Init OTSoftSerial
}

bool OTRN2483Link::begin() {
	char buffer[5];
	memset(buffer, 0, 5);
	// init resetPin
	pinMode(resetPin, OUTPUT);
	// Begin OTSoftSerial
	ser.begin();
	// Reset RN2483:fastDigitalWrite(resetPin, LOW); // reset pin
	fastDigitalWrite(resetPin, HIGH);
	fastDigitalWrite(txPin, LOW); // Set baudrate
	OTV0P2BASE::delay_ms(5);
	fastDigitalWrite(txPin, HIGH);
	ser.print('U');
	// Wait for response
	ser.print("sys get hweui\r\n");
	timedBlockingRead(buffer, sizeof(buffer));
	// Example of setting up LoRaWAN from http://thinginnovations.uk/getting-started-with-microchip-rn2483-lorawan-modules
//	  sendCmd("sys factoryRESET");
//	  sendCmd("sys get hweui");
//	  sendCmd("mac get deveui");
//
//	  // For TTN
//	  sendCmd("mac set devaddr AABBCCDD");  // Set own address
//	  sendCmd("mac set appskey 2B7E151628AED2A6ABF7158809CF4F3C");
//	  sendCmd("mac set nwkskey 2B7E151628AED2A6ABF7158809CF4F3C");
//	  sendCmd("mac set adr off");
//	  sendCmd("mac set rx2 3 869525000");
//	  sendCmd("mac join abp");
//	  sendCmd("mac get status");
//	  sendCmd("mac get devaddr");
	return true;
}

/**
 * @brief	End LoRaWAN connection
 */
bool OTRN2483Link::end()
{
	return true;
}


/**
 * @brief   Sends a raw frame
 * @param   buf	Send buffer. Should always be sent null terminated strings
 */
bool OTRN2483Link::sendRaw(const uint8_t* buf, uint8_t buflen,
		int8_t channel, TXpower power, bool listenAfter)
{
	return false;
}

/**
 * @brief   Puts message in queue to send on wakeup
 * @param   buf        pointer to buffer to send
 * @param   buflen    length of buffer to send
 * @param   channel    ignored
 * @param   Txpower    ignored
 * @retval  returns true if send process inited
 * @todo    This is OTSIM900Link documentation
 * @note    requires calling of poll() to check if message sent successfully
 */
bool OTRN2483Link::queueToSend(const uint8_t *buf, uint8_t buflen, int8_t channel, TXpower power)
{
	return false;
}

void OTRN2483Link::poll()
{

}

uint8_t OTRN2483Link::timedBlockingRead(char *data, uint8_t length)
{

	  // clear buffer, get time and init i to 0
	  memset(data, 0, length);
	  uint8_t i = 0;

	  i = ser.read((uint8_t *)data, length);

//	#ifdef OTSIM900LINK_DEBUG
	  OTV0P2BASE::serialPrintAndFlush(F("\n--Buffer Length: "));
	  OTV0P2BASE::serialPrintAndFlush(i);
	  OTV0P2BASE::serialPrintlnAndFlush();
//	#endif // OTSIM900LINK_DEBUG
	  return i;
}

/****************************** Unused Virtual methods ***************************/
void OTRN2483Link::getCapacity(uint8_t& queueRXMsgsMin,
		uint8_t& maxRXMsgLen, uint8_t& maxTXMsgLen) const {
    queueRXMsgsMin = 0;
    maxRXMsgLen = 0;
    maxTXMsgLen = 0;
}
uint8_t OTRN2483Link::getRXMsgsQueued() const {
    return 0;
}
const volatile uint8_t* OTRN2483Link::peekRXMsg(
		uint8_t& len) const {
    len = 0;
    return NULL;
}

} // namespace OTRN2483Link
