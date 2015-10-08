/*
 * OTNullRadioLink.cpp
 *
 *  Created on: 7 Oct 2015
 *      Author: Denzo
 */

#include "OTNullRadioLink.h"

OTNullRadioLink::OTNullRadioLink() {}

void OTNullRadioLink::getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen)
{
	queueRXMsgsMin = 0;
	maxRXMsgLen = 0;
	maxTXMsgLen = 0;
}

uint8_t OTNullRadioLink::getRXMsgsQueued()
{
	return 0;
}

const volatile uint8_t *OTNullRadioLink::peekRXMsg(uint8_t &len)
{
	len = 0;
	return 0;
}
void OTNullRadioLink::removeRXMsg()
{

}

bool OTNullRadioLink::sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel, TXpower power, bool listenAfter)
{
	return false;
}
