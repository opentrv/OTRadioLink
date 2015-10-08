/*
 * OTNullRadioLink.h
 *
 *  Created on: 7 Oct 2015
 *      Author: Denzo
 */

#ifndef DEV_OTNULLRADIOLINK_OTNULLRADIOLINK_H_
#define DEV_OTNULLRADIOLINK_OTNULLRADIOLINK_H_

#include <Arduino.h>
#include <OTRadioLink.h>


/**
 * @brief	This is a skeleton class that extends OTRadiolink and does nothing
 */
class OTNullRadioLink : public OTRadioLink::OTRadioLink
{
/****************** Interface *******************/
public:
	OTNullRadioLink();

	//virtual void preinit(const void *preconfig)
	//virtual void panicShutdown(); // original: { preinit(NULL); }
	void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen);
	uint8_t getRXMsgsQueued();
	const volatile uint8_t *peekRXMsg(uint8_t &len);
	void removeRXMsg();
	bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false);
	//virtual void poll();
	//virtual bool handleInterruptSimple(); // original: { return(false); }
	//virtual bool end(); // original: { return(false); }
};

#endif /* DEV_OTNULLRADIOLINK_OTNULLRADIOLINK_H_ */
