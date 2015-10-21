/*
 * OTV0P2BASE_SoftSerial.cpp
 *
 *  Created on: 19 Oct 2015
 *      Author: denzo
 */

#include "OTV0P2BASE_SoftSerial.h"

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
void OTSoftSerial::print(uint8_t _c)
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

/**************************** Private Methods ****************************/


} // OTV0P2BASE
