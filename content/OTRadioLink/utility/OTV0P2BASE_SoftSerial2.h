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

#ifndef CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_
#define CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_

#include <stdint.h>
#include "Arduino.h"
#include <Stream.h>
#include "utility/OTV0P2BASE_FastDigitalIO.h"
#include "utility/OTV0P2BASE_Sleep.h"

namespace OTV0P2BASE
{

static const uint8_t OTSOFTSERIAL2_BUFFER_SIZE = 32;

/**
 * @class   OTSoftSerial2
 * @brief   Software serial with optional blocking read and settable interrupt pins.
 *          Extends Stream.h from the Arduino core libraries.
 */
template <uint8_t rxPin, uint8_t txPin, uint32_t baud>
class OTSoftSerial2 : public Stream
{
protected:
    static const constexpr uint16_t timeOut = 60000; // fed into loop...

    static const constexpr uint8_t bitCycles = (F_CPU/4) / baud;
    static const constexpr uint8_t writeDelay = bitCycles - 3;
    static const constexpr uint8_t readDelay = bitCycles - 8;
    static const constexpr uint8_t halfDelay = bitCycles/2;
    static const constexpr uint8_t startDelay = bitCycles + halfDelay;

public:
    /**
     * @brief   Constructor for OTSoftSerial2
     */
    OTSoftSerial2() { }
    /**
     * @brief   Initialises OTSoftSerial2 and sets up pins.
     * @param   speed: The baud to listen at.
     * @fixme   Long is excessive
     * @todo    what to do about optional stuff.
     */
    void begin(unsigned long speed, uint8_t)
    {
        // Set delays
//        uint16_t bitCycles = (F_CPU/4) / speed;
//        writeDelay = bitCycles - 3;
//        readDelay = bitCycles - 8;  // Both these need an offset. These values seem to work at 9600 baud.
//        halfDelay = bitCycles/2 - 1;
        // Set pins for UART
        pinMode(rxPin, INPUT_PULLUP);
        pinMode(txPin, OUTPUT);
        fastDigitalWrite(txPin, HIGH);
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
    int peek() { return -1; }
    /**
     * @brief   Reads a byte from the serial and removes it from the buffer.
     * @retval  Next character in input buffer.
     */
    int read() {
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
                _delay_x4cycles(readDelay);
                val |= fastDigitalRead(rxPin) << i;
            }

            timer = timeOut;
            while (!fastDigitalRead(rxPin)) {
                if (--timer == 0) return -1;
            }
        }
        return val;
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
    void sendBreak()
    {
    	fastDigitalWrite(txPin, LOW);
    	_delay_x4cycles(writeDelay * 16);
    	fastDigitalWrite(txPin, HIGH);
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

#endif /* CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_ */
