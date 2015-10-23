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

#include "OTV0P2BASE_SoftSerial.h"

//#include <stdio.h>
#include <stdlib.h>
#include <util/atomic.h>
#include <OTV0p2Base.h>

namespace OTV0P2BASE
{

/**************** Public Interface ******************/
/**
 * @brief	Constructor
 * @param	rxPin	Pin to receive from
 * @param	txPin	Pin to transmit from
 */
OTSoftSerial::OTSoftSerial(uint8_t _rxPin, uint8_t _txPin) : rxPin(_rxPin), txPin(_txPin)
{
	baud = 0;
	halfDelay = 0;
	fullDelay = 0;
};

/**
 * @brief Starts serial port
 * @param baud	Baud rate to operate at. Defaults to 2400
 */
void OTSoftSerial::begin(uint16_t _baud)
{
	baud = _baud;
	uint16_t bitCycles = (F_CPU/4) / baud;	// delay function burns 4 cpu instructions per cycle
	halfDelay = bitCycles/2 - tuningVal;
	fullDelay = bitCycles - tuningVal;

	pinMode(rxPin, INPUT);
	pinMode(txPin, OUTPUT);
	fastDigitalWrite(txPin, HIGH);	// set txPin to high
}

/**
 * @brief	Closes serial port
 */
void OTSoftSerial::end()
{
	fastDigitalWrite(txPin, LOW);	// set txPin to input with no pullup
	pinMode(txPin, INPUT);
}

/**
 * @brief	Blocking read a single char
 * @retval	value received
 */
uint8_t OTSoftSerial::read()
{
	uint8_t val = 0;
	uint32_t endTime = millis() + timeOut;

	// wait for line to go low
	while(fastDigitalRead(rxPin)) {
		if(endTime < millis()) return 0;
	}

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
		// wait for mid point of bit
		_delay_x4cycles(halfDelay);

		// step through bits and read value	// FIXME better way of doing this?
		for(uint8_t i = 0; i < 8; i++) {
			_delay_x4cycles(fullDelay);
			val |= fastDigitalRead(rxPin) << i;
		}

		// wait for stop bit
		while (!fastDigitalRead(rxPin));
	}
	return val;
}



/**
 * @brief	Blocking read that times out after ?? seconds
 * @todo	How to implement timeout?
 * @param	buf	pointer to array to store read in
 * @param	len	max length of array to store read in
 * @retval	Length of read. Returns 0 if nothing received
 */
uint8_t OTSoftSerial::read(uint8_t *buf, uint8_t len)
{
	uint8_t count = 0;
	uint8_t shortDelay = 1;
	uint8_t val = 0;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		while (count < len) {
			uint16_t timer = 10000;	// attempting timeout
			// wait for line to go low
			while (fastDigitalRead(rxPin)) {
				_delay_x4cycles(shortDelay);
				if (--timer == 0) {
					return count;
				}
			}

			// wait for mid point of bit
			_delay_x4cycles(halfDelay);

			// step through bits and read value	// FIXME better way of doing this?
			for(uint8_t i = 0; i < 8; i++) {
				_delay_x4cycles(fullDelay);
				val |= fastDigitalRead(rxPin) << i;
			}

			// write val to buf and increment buf and count ready for next char
			*buf = val;
			val = 0;
			buf++;
			count++;

			// wait for stop bit
			while (!fastDigitalRead(rxPin));
		}
	}
	return count;
}

/**
 * @brief	Writes a character to serial
 * @param	c	character to write
 */
void OTSoftSerial::print(char _c)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		uint8_t mask = 0x01;
		uint8_t c = _c;

		// Send start bit
		fastDigitalWrite(txPin, LOW);
		_delay_x4cycles(fullDelay);

		// send byte. Loops until mask overflows back to 0
		while(mask != 0) {
			fastDigitalWrite(txPin, mask & c);
			_delay_x4cycles(fullDelay);
			mask = mask << 1;	// bit shift to next value
		}

		// send stop bit
		fastDigitalWrite(txPin, HIGH);
		_delay_x4cycles(fullDelay);
	}
}

/**
 * @brief	Writes an array to serial
 * @param	buf	buffer to write to serial
 * @param	len	length of buffer
 */
void OTSoftSerial::write(const char *buf, uint8_t len)
{
	for (uint8_t i = 0; i < len; i++) {
		print(*buf);
		buf++;
	}
}

/**
 * @brief	Writes a zero terminated array to serial
 * @param	buf	array to write to serial (must be \0 terminated to prevent buffer overflow)
 * @retval	size of array printed
 * @todo	implement max string length/timeout?
 */
uint8_t OTSoftSerial::print(const char *buf)
{
	uint8_t i = 0;
	while (*buf != '\0') {
		print((uint8_t)*buf);
		buf++;
		i++;
	}
	return i;
}

/**
 * @brief	Converts uint8_t to string and prints to serial
 * @param	uint8_t number to print
 * @todo	make overloaded print instead? will this confuse with print char?
 * @note	This makes use of non standard stdlib function itoa(). May break when not
 * 			compiled with avr-libc
 */
void OTSoftSerial::printNum(int8_t number)
{
	static const uint8_t maxNumLength = 3; // Double this if ever need to convert to int16_t
	// init buffer array
	char buf[maxNumLength];
	memset(buf, 0, maxNumLength);

	// convert and fill buffer
	//uint8_t numLength = (uint8_t)snprintf(buf, maxNumLength, "%d", number);
	itoa(number, buf, 10);	// convert integer to string, base 10

	// print buffer
	print(buf);
}
/**************************** Private Methods ****************************/


} // OTV0P2BASE
