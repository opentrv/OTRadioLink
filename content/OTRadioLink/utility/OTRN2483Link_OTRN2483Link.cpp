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

OTRN2483Link::OTRN2483Link::OTRN2483Link() {
}

bool OTRN2483Link::OTRN2483Link::begin() {
}

/**
 * @brief   Sends a raw frame
 * @param   buf	Send buffer. Should always be sent null terminated strings
 */
bool OTRN2483Link::OTRN2483Link::sendRaw(const uint8_t* buf, uint8_t buflen,
		int8_t channel, TXpower power, bool listenAfter) {
}



/****************************** Unused Virtual methods ***************************/
void OTRN2483Link::OTRN2483Link::getCapacity(uint8_t& queueRXMsgsMin,
		uint8_t& maxRXMsgLen, uint8_t& maxTXMsgLen) const {
    queueRXMsgsMin = 0;
    maxRXMsgLen = 0;
    maxTXMsgLen = 0;
}
uint8_t OTRN2483Link::OTRN2483Link::getRXMsgsQueued() const {
    return 0;
}
const volatile uint8_t* OTRN2483Link::OTRN2483Link::peekRXMsg(
		uint8_t& len) const {
    len = 0;
    return NULL;
}

} // namespace OTRN2483Link
