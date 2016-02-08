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
// TODO proper constructor

OTRN2483Link::OTRN2483Link(uint8_t _nRstPin, uint8_t rxPin, uint8_t txPin) : config(NULL), ser(rxPin, txPin), nRstPin(_nRstPin) {
	bAvailable = false;
	// Init OTSoftSerial
}

bool OTRN2483Link::begin() {
	char buffer[5];
	memset(buffer, 0, 5);
	// init resetPin
	pinMode(nRstPin, INPUT);	// TODO This is shorting on my board
	// Begin OTSoftSerial
	ser.begin();
	// Reset
//	print("sys factoryRESET\r\n");
//    factoryReset();
	// set baudrate
	setBaud();
	// todo check RN2483 is present and communicative here

	// Set up for TTN
//	// Set Device Address
    setDevAddr(NULL); // TODO not needed if saved to EEPROM
//
//	// Set keys
    setKeys(NULL, NULL); // TODO not needed if saved to EEPROM

    // join network
    joinABP();

	// get status (returns 0001 when it works for me)
    getStatus();
	// Send
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
 * @param   buf	Send buffer.
 */
bool OTRN2483Link::sendRaw(const uint8_t* buf, uint8_t buflen,
		int8_t channel, TXpower power, bool listenAfter)
{
//	char dataBuf[32];
//	memset(dataBuf, 0, sizeof(dataBuf));

	uint8_t outputBuf[buflen * 2];
	getHex(buf, outputBuf, sizeof(outputBuf));
	print(MAC_START);
	print(MAC_SEND);
	write((const char *)outputBuf, sizeof(outputBuf));
	print(RN2483_END);

//	timedBlockingRead(dataBuf, sizeof(dataBuf));
//	OTV0P2BASE::serialPrintAndFlush(dataBuf);

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

	#ifdef OTRN2483LINK_DEBUG
	  OTV0P2BASE::serialPrintAndFlush(F("\n--Buffer Length: "));
	  OTV0P2BASE::serialPrintAndFlush(i);
	  OTV0P2BASE::serialPrintlnAndFlush();
	#endif // OTRN2483LINK_DEBUG
	  return i;
}

/**
 * @brief   Write a buffer to the RN2483
 * @param   data    Buffer to write
 * @param   length  Length of data buffer
 */
void OTRN2483Link::write(const char *data, uint8_t length)
{
	ser.write(data, length);
}

/**
 * @brief   Prints a single character to RN2483
 * @param   data character to print
 */
void OTRN2483Link::print(const char data)
{
	ser.print(data);
}
/**
 * @brief   Prints a string to the RN2483
 * @param   pointer to a \0 terminated char string
 */
void OTRN2483Link::print(const char *string)
{
	ser.print(string);
}

/**
 * @brief   Sends a 5 ms break
 */
void OTRN2483Link::setBaud()
{
	ser.sendBreak();
	print('U'); // send syncro character
}

/**
 * @brief   reset device and eeprom to factory defaults
 * @note    Currently using software reset as there is a short on my REV14
 * @fixme   Currently non-functional
 */
void OTRN2483Link::factoryReset()
{
	print(SYS_START);
//	print(SYS_RESET);
	print(RN2483_END);
}

/**
 * @brief   reset device
 * @note    Currently using software reset as there is a short on my REV14
 */
void OTRN2483Link::reset()
{
	print(SYS_START);
	print(SYS_RESET);
	print(RN2483_END);

//	fastDigitalWrite(resetPin, LOW); // reset pin
//	//todo delay here
//	fastDigitalWrite(resetPin, HIGH);
}

/**
 * @brief   sets device address
 * @param   pointer to 4 byte buffer containing address
 * @note    OpenTRV has temporarily reserved the block 02:01:11:xx
 *          and is using addresses 00-04 (as of 2016-01-29)
 * @todo    Confirm this is in hex
 */
void OTRN2483Link::setDevAddr(const uint8_t *address)
{
	print(MAC_START);
	print(RN2483_SET);
	print(MAC_DEVADDR);
	print("02011104"); // TODO this will be stored as number in config
//	print(address);
	print(RN2483_END);
}
/**
 * @brief   sets LoRa keys
 * @param   appKey  pointer to a 16 byte buffer containing application key.
 *                  This is specific to the OpenTRV server and should be kept secret.
 * @param   networkKey  pointer to a 16 byte buffer containing the network key.
 *                      This should be the Thing Network key and can be made public.
 *                      The key is: 2B7E151628AED2A6ABF7158809CF4F3C
 * @note    The RN2483 takes numbers as HEX values.
 */
void OTRN2483Link::setKeys(const uint8_t *appKey, const uint8_t *networkKey)
{
	print(MAC_START);
	print(RN2483_SET);
	print(MAC_APPSKEY);
	print("2B7E151628AED2A6ABF7158809CF4F3C"); // TODO this will be stored as number in config
	print(RN2483_END);

	print(MAC_START);
	print(RN2483_SET);
	print(MAC_NWKSKEY);
	print("2B7E151628AED2A6ABF7158809CF4F3C"); // TODO this will be stored as number in config
	print(RN2483_END);
}
/**
 * @brief   Sets adaptive data rate off and activates connection by personalisation
 * @todo    Move adaptive data rate out and work out if this is what we want
 */
void OTRN2483Link::joinABP()
{
	print(MAC_START);
	print(RN2483_SET);
	print(MAC_ADR_OFF); // Adaptive data rate
	print(RN2483_END);

	print(MAC_START);
	print(MAC_JOINABP); // Join by ABP (activation by personalisation)
	print(RN2483_END);
}
/**
 * @brief   Return status
 * @retval
 * @todo    Find out what status messages mean and document.
 *          Do logic
 */
bool OTRN2483Link::getStatus()
{
	print(MAC_START);
	print(RN2483_GET);
	print(MAC_STATUS);
	print(RN2483_END);
}
/**
 * @brief   Saves current mac state
 */
void OTRN2483Link::save()
{
	print(MAC_START);
	print(MAC_SAVE);
	print(RN2483_END);
}

/**
 * @brief   converts a string to hex representation
 * @param   string  String to convert
 * @param   output  buffer to hold output. This should be twice the length of string
 * @param   outputLen   Length of output
 * @retval  returns true if output success
 * @note    This function only checks output bounds. If input string is too short
 *          it will overrun.
 *          Will terminate if passed a null pointer.
 * @todo    Possibly better to make part of send function and send as byte is converted
 */
bool OTRN2483Link::getHex(const uint8_t *input, uint8_t *output, uint8_t outputLen)
{
	  if((input == NULL) || (output == NULL)) return false; // check for null pointer
	    uint8_t counter = outputLen;
	    // convert to hex
	  while(counter) {
	    uint8_t highValue = *input >> 4;
	    uint8_t lowValue  = *input & 0x0f;
	    uint8_t temp;

	    temp = highValue;
	    if(temp <= 9) temp += '0';
	    else temp += ('A' - 10);
	    *output++ = temp;

	    temp = lowValue;
	    if(temp <= 9) temp += '0';
	    else temp += ('A'-10);
	    *output++ = temp;

	    input++;
	    counter -= 2;
	  }
	  return true;
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
const volatile uint8_t* OTRN2483Link::peekRXMsg() const {
    return NULL;
}

const char OTRN2483Link::SYS_START[5] = "sys ";
const char OTRN2483Link::SYS_RESET[6] = "reset"; // FIXME this can be removed on board with working reset line

const char OTRN2483Link::MAC_START[5] = "mac ";
const char OTRN2483Link::MAC_DEVADDR[9] = "devaddr ";
const char OTRN2483Link::MAC_APPSKEY[9] = "appskey ";
const char OTRN2483Link::MAC_NWKSKEY[9] = "nwkskey ";
const char OTRN2483Link::MAC_ADR_OFF[8] = "adr off";	// TODO find out what this does
const char OTRN2483Link::MAC_JOINABP[9] = "join abp";
const char OTRN2483Link::MAC_STATUS[7] = "status";
const char OTRN2483Link::MAC_SEND[12] = "tx uncnf 1 ";		// Sends an unconfirmed packet on channel 1
const char OTRN2483Link::MAC_SAVE[5] = "save";

const char OTRN2483Link::RN2483_SET[5] = "set ";
const char OTRN2483Link::RN2483_GET[5] = "get ";
const char OTRN2483Link::RN2483_END[3] = "\r\n";

} // namespace OTRN2483Link
