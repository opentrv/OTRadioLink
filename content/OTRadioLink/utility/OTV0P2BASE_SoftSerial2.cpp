/*
 * OTV0P2BASE_SoftSerial2.cpp
 *
 *  Created on: 15 Apr 2016
 *      Author: denzo
 */

#include "OTV0P2BASE_SoftSerial2.h"

namespace OTV0P2BASE
{
/**
 * @brief   Constructor for OTSoftSerial2
 * @param   rxPin: Pin to receive from.
 * @param   txPin: Pin to send from.
 */
OTSoftSerial2::OTSoftSerial2(uint8_t _rxPin, uint8_t _txPin): rxPin(_rxPin), txPin(_txPin)
{
    halfDelay = 0;
    fullDelay = 0;
//    rxBufferHead = 0;
//    rxBufferTail = 0;
//    memset(rxBuffer, 0, sizeof(rxBuffer));
}
/**
 * @brief   Initialises OTSoftSerial2 and sets up pins.
 * @param   speed: The baud to listen at.
 * @fixme   Long is excessive
 */
void OTSoftSerial2::begin(unsigned long speed, uint8_t) {
    // Set delays
    uint16_t bitCycles = (F_CPU/4) / speed;
    fullDelay = bitCycles - 25; // fixme -22 only temporary. will be removed when templating.
    halfDelay = bitCycles/2 - 25;

    // Set pins for UART
    pinMode(rxPin, INPUT_PULLUP);
    pinMode(txPin, OUTPUT);
    fastDigitalWrite(txPin, HIGH);
}
/**
 * @brief   Disables serial and releases pins.
 * @todo    What state do we want to leave these in?
 * @todo    Reset read buffer?
 */
void OTSoftSerial2::end() {
    pinMode(txPin, INPUT_PULLUP);
}
/**
 * @brief   Write a byte to serial as a binary value.
 * @param   byte: Byte to write.
 * @retval  Number of bytes written.
 */
size_t OTSoftSerial2::write(uint8_t byte) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        uint8_t mask = 0x01;
        uint8_t c = byte;

        // Send start bit
        fastDigitalWrite(txPin, LOW);
        _delay_x4cycles(fullDelay); // fixme delete -5s

        // send byte. Loops until mask overflows back to 0
        while(mask != 0) {
            fastDigitalWrite(txPin, mask & c);
            _delay_x4cycles(fullDelay);
            mask = mask << 1;    // bit shift to next value
        }

        // send stop bit
        fastDigitalWrite(txPin, HIGH);
        _delay_x4cycles(fullDelay);
    }
    return 1;
}
/**
 * @brief   Read next character in the input buffer without removing it.
 * @retval  Next character in input buffer.
 * @todo    include when asynchronous read implemented.
 */
int OTSoftSerial2::peek() {
//    if (rxBufferHead == rxBufferTail) return -1;
//    else return rxBuffer[rxBufferTail];
    return -1;
}
/**
 * @brief   Reads a byte from the serial and removes it from the buffer.
 * @retval  Next character in input buffer.
 * @fixme   Currently blocking.
 */
int OTSoftSerial2::read() {
    // Blocking read:
    uint8_t val = 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        volatile uint16_t timer = timeOut;  // TODO find out if using an attribute will be useful.
        // wait for line to go low
        while (fastDigitalRead(rxPin)) {
            if (--timer == 0) return -1;
        }

        // wait for mid point of bit
        _delay_x4cycles(halfDelay);

        // step through bits and read value    // FIXME better way of doing this?
        for(uint8_t i = 0; i < 8; i++) {
            _delay_x4cycles(fullDelay);
            val |= fastDigitalRead(rxPin) << i;
        }

        timer = timeOut;
        while (!fastDigitalRead(rxPin)) {
            if (--timer == 0) return -1;
        }
    }
    return val;
    // This is the asyc code (I got carried away).
//    if (rxBuffer == rxBufferTail) return -1;
//    else {
//        uint8_t c = rxBuffer[rxBufferTail];
//        rxBufferTail = (rxBufferTail + 1) % sizeof(rxBuffer);
//        return c;
//    }
}
/**
 * @brief   Get the number of bytes available to read in the input buffer.
 * @retval  The number of bytes available.
 */
int OTSoftSerial2::available() {
//    uint8_t available = sizeof(rxBuffer) + rxBufferHead - rxBufferTail; // Extra addition keeps calculation positive. Probably unnecessary?
//    return (available % sizeof(rxBuffer));
    return -1;
}
/**
 * @TODO!!!!!!!!!!
 */


}
