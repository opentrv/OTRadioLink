/*
 * OTNullRadioLink.h
 *
 *  Created on: 7 Oct 2015
 *      Author: Denzo
 */

#ifndef DEV_OTNULLRADIOLINK_OTNULLRADIOLINK_H_
#define DEV_OTNULLRADIOLINK_OTNULLRADIOLINK_H_

#include<Arduino.h>
#include<SoftwareSerial.h>

template<uint8_t rxPin, uint8_t txPin>
class Test
{
private:
	SoftwareSerial s;
public:
	Test() : s(rxPin, txPin) {};
};



#endif /* DEV_OTNULLRADIOLINK_OTNULLRADIOLINK_H_ */
