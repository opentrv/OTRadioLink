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

#include "OTRadioLink_OTNullRadioLink.h"

namespace OTRadioLink {

OTNullRadioLink::OTNullRadioLink() {}


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
	return NULL;
}
void OTNullRadioLink::removeRXMsg()
{

}

/**
 * @brief	If debug enabled, prints a string to serial
 * @todo	Work out what to do about debug config
 * @param	buf		Pointer to a buffer
 */
bool OTNullRadioLink::sendRaw(const uint8_t *buf, uint8_t buflen, int8_t , TXpower , bool )
{
	const char *pBuf = (const char *) buf;
	// print if in debug mode
	DEBUG_SERIAL_PRINT_FLASHSTRING("Radio: ");
	for (uint8_t i = 0; i < buflen; i++) {
		DEBUG_SERIAL_PRINT(*pBuf);
		pBuf++;
	}
	DEBUG_SERIAL_PRINTLN();
	return true;
}

} // OTNullRadioLink
