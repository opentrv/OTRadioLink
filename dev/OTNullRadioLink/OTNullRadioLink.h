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

Author(s) / Copyright (s): Deniz Erbilgin 2015
*/

#ifndef OTNULLRADIOLINK_OTNULLRADIOLINK_H_
#define OTNULLRADIOLINK_OTNULLRADIOLINK_H_

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
	OTNullRadioLink();
	bool begin() {return (true);};
	void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen) const;
	uint8_t getRXMsgsQueued() const;
	const volatile uint8_t *peekRXMsg(uint8_t &len) const;
	void removeRXMsg();
	bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false);
public:
	void _dolisten() {};
};

}  //OTNullRadioLink
#endif /* OTNULLRADIOLINK_OTNULLRADIOLINK_H_ */
