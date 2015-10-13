/*
 * OTNullRadioLink.cpp
 *
 *  Created on: 7 Oct 2015
 *      Author: Denzo
 */

#include "OTNullRadioLink.h"

namespace OTNullRadioLink {

OTNullRadioLink::OTNullRadioLink(uint16_t _baud) : baud(_baud) {}


void OTNullRadioLink::getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen) const
{
	queueRXMsgsMin = 0;
	maxRXMsgLen = 0;
	maxTXMsgLen = 0;
}

uint8_t OTNullRadioLink::getRXMsgsQueued() const
{
	return 0;
}

const volatile uint8_t *OTNullRadioLink::peekRXMsg(uint8_t &len) const
{
	len = 0;
	return 0;
}
void OTNullRadioLink::removeRXMsg()
{

}

bool OTNullRadioLink::sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel, TXpower power, bool listenAfter)
{
	//char buffer[buflen+1];
	//memcpy(buffer, (const char *)buf, buflen);
	//buffer[buflen] = 0;
	Serial.println("--Start Message:");
	Serial.write(buf, buflen);
	Serial.println("\n--End Message");
	return true;
}

/*bool queueToSend(const uint8_t *buf, uint8_t buflen, int8_t channel, TXpower power)
{
	return false;
}*/

} // OTNullRadioLink
