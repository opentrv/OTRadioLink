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

// NOTE!!! Implementation details are in OTV0P2BASE_SoftSerial_NOTES.txt!!!

#ifndef CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL3_H_
#define CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL3_H_

#include <stdint.h>
#include "Arduino.h"
#include <Stream.h>
#include "utility/OTV0P2BASE_FastDigitalIO.h"
#include "utility/OTV0P2BASE_Sleep.h"

namespace OTV0P2BASE
{

static const uint8_t OTSOFTSERIAL3_BUFFER_SIZE = 32;

/**
 * @class   OTSoftSerial3
 * @brief   Software serial with optional blocking read and settable interrupt pins.
 *          Extends Stream.h from the Arduino core libraries.
 */
template <uint8_t rxPin, uint8_t txPin>
class OTSoftSerial3 : public Stream
{
protected:
    uint8_t writeDelay;
    uint8_t readDelay;
    uint8_t halfDelay;
    volatile uint8_t rxBufferHead;
    volatile uint8_t rxBufferTail;
    uint8_t rxBuffer[OTSOFTSERIAL3_BUFFER_SIZE];

public:
    /**
     * @brief   Constructor for OTSoftSerial2
     */
    OTSoftSerial3()
    {
        writeDelay = 0;
        readDelay = 0;
        halfDelay = 0;
        rxBufferHead = 0;
        rxBufferTail = 0;
    }
    /**
     * @brief   Initialises OTSoftSerial2 and sets up pins.
     * @param   speed: The baud to listen at.
     * @fixme   Long is excessive
     * @todo    what to do about optional stuff.
     */
    void begin(unsigned long speed, uint8_t)
    {
        // Set delays
        uint16_t bitCycles = (F_CPU/4) / speed;
        writeDelay = bitCycles - 3;
        readDelay = bitCycles;  // Both these need an offset. These values seem to work at 9600 baud. 8
        halfDelay = bitCycles/2;
        // Set pins for UART
        pinMode(rxPin, INPUT_PULLUP);
        pinMode(txPin, OUTPUT);
        fastDigitalWrite(txPin, HIGH);
        memset(rxBuffer, 0, OTSOFTSERIAL3_BUFFER_SIZE);
    }
    void begin(unsigned long speed) { begin(speed, 0); }
    /**
     * @brief   Disables serial and releases pins.
     */
    void end() { pinMode(txPin, INPUT_PULLUP); }
    /**
     * @brief   Write a byte to serial as a binary value.
     * @param   byte: Byte to write.
     * @retval  Number of bytes written.
     */
    size_t write(uint8_t byte)
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            uint8_t mask = 0x01;
            uint8_t c = byte;

            // Send start bit
            fastDigitalWrite(txPin, LOW);
            _delay_x4cycles(writeDelay); // fixme delete -5s

            // send byte. Loops until mask overflows back to 0
            while(mask != 0) {
                if (mask & c) fastDigitalWrite(txPin, HIGH);
                else fastDigitalWrite(txPin, LOW);
                _delay_x4cycles(writeDelay);
                mask = mask << 1;    // bit shift to next value
            }

            // send stop bit
            fastDigitalWrite(txPin, HIGH);
            _delay_x4cycles(writeDelay);
        }
        return 1;
    }
    /**
     * @brief   Read next character in the input buffer without removing it.
     * @retval  Next character in input buffer.
     */
    int peek()
    {
        return rxBuffer[0];
    }
    /**
     * @brief   Reads a byte from the serial and removes it from the buffer.
     * @retval  Next character in input buffer.
     */
    int read()
    {
        return -1;
    }
    /**
     * @brief   Get the number of bytes available to read in the input buffer.
     * @retval  The number of bytes available.
     */
    int available() { return -1; }
    /**
     * @brief   Check if serial port is ready for use.
     * @todo    Implement the time checks using this?
     */
    operator bool() { return true; }
    using Print::write; // write(str) and write(buf, size) from Print

    /**************************************************************************
     * -------------------------- Non Standard ------------------------------ *
     *************************************************************************/
    /**
     * @brief   Sends a break condition (tx line held low for longer than the
     *          time it takes to send a character.
     */
    void sendBreak()
    {
        fastDigitalWrite(txPin, LOW);
        _delay_x4cycles(writeDelay * 16);
        fastDigitalWrite(txPin, HIGH);
    }
    /**
     * @brief   Handle interrupts
     */
    inline void handle_interrupt()
    {
        // Blocking read:
        uint8_t next = rxBufferTail;
        uint8_t val = 0;
        // wait for mid point of bit
//        _delay_x4cycles(halfDelay);
        _delay_x4cycles(3);

        // step through bits and read value    // FIXME better way of doing this?
        for(uint8_t i = 0; i < 8; i++) {
//            _delay_x4cycles(readDelay);
            _delay_x4cycles(25);
            val |= fastDigitalRead(rxPin) << i;
        }
        // Writing to the buffer:
        //  - Increment tail pointer
        //  - Check for ovf
        //  - Check tail has not overtaken head.
        //  - Write char to tail.
        rxBuffer[0] = val;
//        next++;
//        if (next > OTSOFTSERIAL3_BUFFER_SIZE) {
//            next = 0;
//        }
//        if (next != rxBufferHead) {
//            rxBufferTail = next;
//            rxBuffer[next] = val;
//        }
    }
    /**************************************************************************
     * ------------------------ Unimplemented ------------------------------- *
     *************************************************************************/
    /**
     * @brief   Destuctor for OTSoftSerial.
     * @note    Not implemented to reduce code size.
     */
    //    ~OTSoftSerial2() {};    // TODO Does this actually work?
    /**
     * @brief   Waits for transmission of outgoing serial data to complete.
     * @note    This is not used for OTSoftSerial2 as all writes are synchronous.
     */
    virtual void flush() {}
    /**
     * @brief   Returns the number of elements in the Tx buffer.
     * @retval  0 as no Tx buffer implemented.
     * @note    This is not used for OTSoftSerial2 as all writes are synchronous.
     */
    int availableForWrite() { return 0; }  //

};


}

#endif /* CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL3_H_ */
