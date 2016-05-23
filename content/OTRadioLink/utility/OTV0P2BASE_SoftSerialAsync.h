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

// NOTE!!! Implementation details are in OTV0P2BASE_SoftSerialAsync_NOTES.md!!!

#ifndef CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIALASYNC_H_
#define CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIALASYNC_H_

#include <stdint.h>
#include "Arduino.h"
#include <Stream.h>
#include "utility/OTV0P2BASE_FastDigitalIO.h"
#include "utility/OTV0P2BASE_Sleep.h"

namespace OTV0P2BASE
{

static const uint8_t OTSOFTSERIALASYNC_BUFFER_SIZE = 32;  // size of buffer for holding input chars

/**
 * @class   OTSoftSerialAsync
 * @brief   Asynchronous software serial with exposed interrupt handler.
 *          Extends Stream.h from the Arduino core libraries.
 * @param   rxPin: Receive pin for software UART.
 * @param   txPin: Transmit pin for software UART.
 * @param   baud: Speed of UART in baud. Currently reliably supports up to 4800 (will usually work at 9600 with no other interrupt enabled).
 * @note    This currently does not support a ring buffer. The read buffer is reset after each write.
 * @note    This currently supports a max speed of 4800 baud when used with the ATMega pin change interrupts
 *          and with an F_CPU of 1 MHz.
 * @todo    Move everything back into source file without breaking templating.
 */
template <uint8_t rxPin, uint8_t txPin, uint16_t baud>
class OTSoftSerialAsync : public Stream
{
protected:
    // All these are compile time calculations and are automatically substituted as part of program code.
    static const constexpr uint16_t bitCycles = (F_CPU/4) / baud;  // Number of times _delay_x4cycles needs to loop for 1 bit.
    static const constexpr uint8_t writeDelay = bitCycles - 3;
    static const constexpr uint8_t quarterDelay = (bitCycles / 4) - 1;  // For multisampling within a bit.
    static const constexpr uint8_t halfDelay = (bitCycles / 2) - 6;  // Standard inter-bit delay.
    static const constexpr uint8_t longDelay = halfDelay + quarterDelay;  // Longer inter-bit delay.
    static const constexpr uint8_t shortDelay = halfDelay - quarterDelay;  // Shorter inter-bit delay.
    static const constexpr uint8_t startDelay = (3 * bitCycles / 4) - 2;  // 3/4 bit delay + isr entry for first read.

    uint8_t rxBufferHead;  // Head of buffer.
    volatile uint8_t rxBufferTail;  // Tail. Must be volatile as accessed from interrupt.
    volatile uint8_t rxBuffer[OTSOFTSERIALASYNC_BUFFER_SIZE];

public:
    /**
     * @brief   Constructor for OTSoftSerial2
     */
    OTSoftSerialAsync()
    {
        rxBufferHead = 0;
        rxBufferTail = 0;
    }
    /**
     * @brief   Initialises OTSoftSerial2 and sets up pins.
     * @param   speed: Not used. Kept for compatibility with Arduino libraries.
     */
    void begin(const unsigned long, const uint8_t)
    {
        // Set pins for UART
        pinMode(rxPin, INPUT_PULLUP);
        pinMode(txPin, OUTPUT);
        fastDigitalWrite(txPin, HIGH);
        // init buffer
        memset( (void *)rxBuffer, 0, OTSOFTSERIALASYNC_BUFFER_SIZE);
    }
    void begin(const unsigned long) { begin(0, 0); }

    /**
     * @brief   Disables serial and releases pins.
     */
    void end() { pinMode(txPin, INPUT_PULLUP); }

    /**
     * @brief   Write a byte to serial as a binary value.
     * @param   byte: Byte to write.
     * @retval  Number of bytes written (always 1).
     */
    size_t write(const uint8_t byte)
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            uint8_t mask = 0x01;
            uint8_t c = byte;

            memset((void *)rxBuffer, 0, OTSOFTSERIALASYNC_BUFFER_SIZE); //for debug

            // Send start bit
            fastDigitalWrite(txPin, LOW);
            _softserial_delay(writeDelay);

            // send byte. Loops until mask overflows back to 0
            while(mask != 0) {
                if (mask & c) fastDigitalWrite(txPin, HIGH);
                else fastDigitalWrite(txPin, LOW);
                _softserial_delay(writeDelay);
                mask = mask << 1;    // bit shift to next value
            }

            // send stop bit
            fastDigitalWrite(txPin, HIGH);
            _softserial_delay(writeDelay);
            // reset rx buffer.
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
     * @retval  Next character in input buffer. -1 if empty.
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
     * @brief   Inline delay.
     * @param   n: Number of loops to delay for. Each loop takes 4 clock cycles.
     * @note    This is a copy of OTV0P2BASE::_delay_x4cycles in OTV0P2BASE_Sleep.h
     *          It was reproduced here to force GCC to inline it.
     */
    inline void _softserial_delay(uint8_t n) __attribute__((always_inline))
    {
        __asm__ volatile // Similar to _delay_loop_1() from util/delay_basic.h but multiples of 4 cycles are easier.
           (
            "1: dec  %0" "\n\t"
            "   breq 2f" "\n\t"
            "2: brne 1b"
            : "=r" (n)
            : "0" (n)
          );
    }

    /**
     * @brief   Interrupt handler containing read routine.
     * @fixme   Does not read the most significant bit.
     * @fixme   Cannot enter and exit fast enough for this to be reliable at 9600 baud.
     */
    inline void handle_interrupt() __attribute__((always_inline))
    {
        // Blocking read:
        uint8_t bufptr = rxBufferTail;
        uint8_t val = 0;
        // wait for first read time (start bit + 1 quarter of 1st bit)
        _softserial_delay(startDelay);

        // Step through bits and read value.
        for (uint8_t i = 0; i < 7; i++) {
            // The loop fills in the top bit of and shifts down to reverse bit order
            // (UART is sent least significant bit(lsb) first, we need most significant bit (msb) first)
            uint8_t bitval = 0;

            // Put 3 samples, each a quarter of a bit apart, into bitval.
            // fixme this could be put in a loop.
            bitval = fastDigitalRead(rxPin);
            bitval = (bitval << 1);
            _softserial_delay(quarterDelay);
            bitval += fastDigitalRead(rxPin);
            bitval = (bitval << 1);
            _softserial_delay(quarterDelay);
            bitval += fastDigitalRead(rxPin);

            // Work out if bit high and adjust delay.
            // If bitval == 111, 110 or 011, the bit was high.
            if ((bitval >= 5) || (bitval == 3)) { // True
                val += (1 << 7);  // Set msb in val high.
                // If 011, we sampled too early so we wait longer.
                if(bitval == 0b011) _softserial_delay(longDelay);
                // If 110, we sampled too late so we wait less.
                else if (bitval == 0b110) _softserial_delay(shortDelay);
                // If 111 or 101, we don't adjust the delay.
                else _softserial_delay(halfDelay);
            } else { // False
                val += (0 << 7);  // set lsb in val low.
                if(bitval == 0b100) _softserial_delay(longDelay); // Too fast
                else if ( bitval == 0b001) _softserial_delay(shortDelay); // Too slow
                else _softserial_delay(halfDelay);
            }
            val = val >> 1;  // shift val down.
        }
#if 0 // Cannot exit fast enough with this.
        // This reads the last bit. No need for a delay as we exit the interrupt afterwards.
        {
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
        // Put val in the read buffer and increment the tail.
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
     * @note    This is not used for OTSoftSerialAsync as all writes are synchronous
     */
    virtual void flush() {}
    /**
     * @brief   Returns the number of elements in the Tx buffer.
     * @retval  0 as no Tx buffer implemented.
     * @note    This is not used for OTSoftSerialAsync as all writes are synchronous.
     */
    int availableForWrite() { return 0; }  //

};


}

#endif /* CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIALASYNC_H_ */
