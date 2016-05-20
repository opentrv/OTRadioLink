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
 * @note    This currently does not support a ring buffer. The read buffer is reset after each write and
 */
template <uint8_t rxPin, uint8_t txPin, uint16_t speed>
class OTSoftSerial3 : public Stream
{
protected:
    // All these are compile time calculations and are automatically substituted as part of program code.
    static const constexpr uint16_t bitCycles = (F_CPU/4) / speed;
    static const constexpr uint8_t writeDelay = bitCycles - 5;
    static const constexpr uint8_t quarterDelay = (bitCycles / 4) - 4;  // For multisampling bits
    static const constexpr uint8_t halfDelay = (bitCycles / 2) - 7;  // Standard inter-bit delay
    static const constexpr uint8_t longDelay = halfDelay + 5;  // Longer inter-bit delay
    static const constexpr uint8_t shortDelay = halfDelay - 5;  // Shorter inter-bit delay
    static const constexpr uint8_t startDelay = (bitCycles/2)-2; // + (bitCycles / 4);  // 1 bit delay to skip start bit + 1 quarter bit delay for first read position.

    uint8_t rxBufferHead;
    volatile uint8_t rxBufferTail;
    volatile uint8_t rxBuffer[OTSOFTSERIAL3_BUFFER_SIZE];

public:
    /**
     * @brief   Constructor for OTSoftSerial2
     */
    OTSoftSerial3()
    {
        rxBufferHead = 0;
        rxBufferTail = 0;
    }
    /**
     * @brief   Initialises OTSoftSerial2 and sets up pins.
     * @param   speed: The baud to listen at.
     * @fixme   Long is excessive
     * @todo    what to do about optional stuff.
     */
    void begin(const unsigned long, const uint8_t)
    {
        // Set pins for UART
        pinMode(rxPin, INPUT_PULLUP);
        pinMode(txPin, OUTPUT);
        fastDigitalWrite(txPin, HIGH);
        memset( (void *)rxBuffer, 0, OTSOFTSERIAL3_BUFFER_SIZE);
    }
    void begin(const unsigned long) { begin(0, 0); }
    /**
     * @brief   Disables serial and releases pins.
     */
    void end() { pinMode(txPin, INPUT_PULLUP); }
    /**
     * @brief   Write a byte to serial as a binary value.
     * @param   byte: Byte to write.
     * @retval  Number of bytes written.
     */
    size_t write(const uint8_t byte)
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            uint8_t mask = 0x01;
            uint8_t c = byte;

            memset((void *)rxBuffer, 0, OTSOFTSERIAL3_BUFFER_SIZE); //for debug

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
            rxBufferHead = 0;
            rxBufferTail = 0;
        }
        return 1;
    }
    /**
     * @brief   Read next character in the input buffer without removing it.
     * @retval  Next character in input buffer.
     */
    int peek()
    {
        uint8_t head = rxBufferHead;
        uint8_t tail = rxBufferTail;
        if ((tail > 0) && (head < tail)) {
            return rxBuffer[head];
        }
        else return -1;
    }
    /**
     * @brief   Reads a byte from the serial and removes it from the buffer.
     * @retval  Next character in input buffer.
     */
    int read()
    {
        uint8_t head = rxBufferHead;
        uint8_t tail = rxBufferTail;
        if ((tail > 0) && (head < tail)) {
            rxBufferHead = head + 1;
            return rxBuffer[head];
        }
        else return -1;
    }
    /**
     * @brief   Get the number of bytes available to read in the input buffer.
     * @retval  The number of bytes available.
     */
    int available() { return rxBufferTail-rxBufferHead; }
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
    inline void handle_interrupt() __attribute__((always_inline))
    {
        // Blocking read:
        uint8_t bufptr = rxBufferTail;
        uint8_t val = 0;
        // wait for first read time (start bit + 1 quarter of 1st bit)
        _delay_x4cycles(startDelay);

        // step through bits and read value    // FIXME better way of doing this?
        // We do first 7 bits in loops and then last bit out of loop to avoid delay.
        for (uint8_t i = 0; i < 7; i++) {
            // The loop fills in the top bit and shifts down to reverse bit order
            // (UART is lsb first, we want msb first)
            uint8_t bitval = 0;
            bitval = fastDigitalRead(rxPin);
            bitval = (bitval << 1);
            _delay_x4cycles(quarterDelay);
            bitval += fastDigitalRead(rxPin);
            bitval = (bitval << 1);
            _delay_x4cycles(quarterDelay);
            bitval += fastDigitalRead(rxPin);
            // Work out if bit high and adjust delay.
            if ((bitval >= 5) || (bitval == 3)) { // True
                val += (1 << 7);
                if(bitval == 0b011) _delay_x4cycles(longDelay);  // Too fast
                else if (bitval == 0b110) _delay_x4cycles(shortDelay);  // Too slow
                else _delay_x4cycles(halfDelay);
            } else { // False
                val += (0 << 7);
                if(bitval == 0b100) _delay_x4cycles(longDelay); // Too fast
                else if ( bitval == 0b001) _delay_x4cycles(shortDelay); // Too slow
                else _delay_x4cycles(halfDelay);
            }
            val = val >> 1;  // shift down.
        }
#if 0 // Cannot exit fast enough with this.
        { // Read last bit
            uint8_t bitval = 0;
            PORTC |= (1 << 2);
            bitval = fastDigitalRead(rxPin);
            _delay_x4cycles(quarterDelay);
            bitval += fastDigitalRead(rxPin);
            _delay_x4cycles(quarterDelay);
            bitval += fastDigitalRead(rxPin);
            PORTC &= ~(1 << 2);
            if ((bitval >= 5) || (bitval == 3)) { // True
                val += (1 << 7);
            } else { // False
                val += (0 << 7);
            }
        }
#endif
        // Update Buffer.
        if (bufptr < sizeof(rxBuffer)) {
            rxBuffer[bufptr] = val;
            rxBufferTail = bufptr + 1;
        }
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
