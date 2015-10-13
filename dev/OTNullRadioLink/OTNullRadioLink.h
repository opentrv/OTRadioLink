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

namespace OTNullRadioLink{
/**
 * @brief	This is a skeleton class that extends OTRadiolink and does nothing
 */
class OTNullRadioLink : public OTRadioLink::OTRadioLink
{
/****************** Interface *******************/
public:
	OTNullRadioLink(uint16_t _baud);
	bool begin()
	{
		Serial.println("NullRadio");
		return (true);};
	//virtual void preinit(const void *preconfig)
	//virtual void panicShutdown(); // original: { preinit(NULL); }
	void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen) const;
	uint8_t getRXMsgsQueued() const;
	const volatile uint8_t *peekRXMsg(uint8_t &len) const;
	void removeRXMsg();
	bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false);
	//bool queueToSend(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal);
	void poll() {};
	//virtual bool handleInterruptSimple(); // original: { return(false); }
	//virtual bool end(); // original: { return(false); }
public:
	const uint16_t baud;

	void _dolisten() {};
};

}  //OTNullRadioLink
#endif /* DEV_OTNULLRADIOLINK_OTNULLRADIOLINK_H_ */
