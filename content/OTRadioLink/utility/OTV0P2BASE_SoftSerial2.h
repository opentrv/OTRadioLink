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
                           Damon Hart-Davis 2016
*/

/*
 * Software-based serial/UART V2.
 *
 * V0p2/AVR only.
 */

// Implementation details are in OTV0P2BASE_SoftSerial2_NOTES.txt.

#ifndef CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_
#define CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_

#include <stdint.h>

#ifdef ARDUINO
#include "Arduino.h"
#endif

#include "utility/OTV0P2BASE_FastDigitalIO.h"
#include "utility/OTV0P2BASE_Sleep.h"

namespace OTV0P2BASE
{


#ifdef ARDUINO_ARCH_AVR
/**
 * @class   OTSoftSerial2
 * @brief   Blocking software serial library.
 *          Extends Stream.h from the Arduino core libraries.
 * @param   rxPin: Receive pin for software UART.
 * @param   txPin: Transmit pin for software UART.
 * @param   baud: Speed of UART in baud. Currently reliably supports up to 9600.
 * @note    This currently supports a max speed of 9600 baud with an F_CPU of 1 MHz.
 */
#define OTSoftSerial2_DEFINED
template <uint8_t rxPin, uint8_t txPin, uint32_t baud>
class OTSoftSerial2 final : public Stream
{
protected:
    // All these are compile time calculations and are automatically substituted as part of program code.
    static constexpr uint16_t timeOut = 60000; // fed into loop...
    static constexpr uint8_t bitCycles = (F_CPU/4) / baud;  // Number of times _delay_x4cycles needs to loop for 1 bit.
    static constexpr uint8_t writeDelay = bitCycles - 3;  // Delay needed to write 1 bit.
    static constexpr uint8_t readDelay = bitCycles - 8;  // Delay needed to read 1 bit.
    static constexpr uint8_t halfDelay = bitCycles/2;

public:
    /**
     * @brief   Initialises OTSoftSerial2 and sets up pins.
     * @param   speed: Not used. Kept for compatibility with Arduino libraries.
     */
    void begin(unsigned long , uint8_t)
    {
        // Set pins for UART
        pinMode(rxPin, INPUT_PULLUP);
        pinMode(txPin, OUTPUT);
        fastDigitalWrite(txPin, HIGH);
    }
    void begin(unsigned long) { begin(0, 0); }

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
            _delay_x4cycles(writeDelay);

            // send byte. Loops until mask overflows back to 0
            // NOTE: This requires an 8 bit data type! Change loop type?
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
     * @brief   Reads a byte from the serial and removes it from the buffer.
     * @retval  Next character in input buffer; -1 on timeout or error.
     * @note    This routine blocks interrupts until it receives a byte or times out.
     */
    int read() {
        // Blocking read:
        uint8_t val = 0;
        volatile uint16_t timer = timeOut;

        // Wait for start bit, ie wait for RX to go low.
        // NOTE: As this is outside of an interrupt, we may be late in
        //       responding to a pin change.
        while (fastDigitalRead(rxPin)) { if (--timer == 0) { return (-1); } }

        // The bit that actually does the read.
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            // Wait for mid point of bit, ie 0.5 bit time,
            // to centre the following reads in bit times.
            _delay_x4cycles(halfDelay);

            // Step through bits and assemble bits into byte.
            for(uint8_t i = 0; i < 8; ++i) {
                _delay_x4cycles(readDelay);
                val |= fastDigitalRead(rxPin) << i;
            }
        }

        // Wait for stop bit, ie wait for RX to go high.
        // This is no longer time sensitive so we can reenable interrupts.
        timer = timeOut;
        while (!fastDigitalRead(rxPin)) { if (--timer == 0) { return (-1); } }
        return val;
    }

    /**
     * @brief   Check if serial port is ready for use.
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
    /**************************************************************************
     * ------------------------ Unimplemented ------------------------------- *
     *************************************************************************/
    /**
     * @brief   Destructor for OTSoftSerial.
     * @note    Not implemented to reduce code size.
     */
    //    ~OTSoftSerial2() {};
    /**
     * @brief   Read next character in the input buffer without removing it.
     * @note    Not used. Kept for compatibility with Arduino libraries.
     */
    int peek() { return -1; }
    /**
     * @brief   Get the number of bytes available to read in the input buffer.
     * @note    Not used. Kept for compatibility with Arduino libraries.
     */
    int available() { return -1; }
    /**
     * @brief   Waits for transmission of outgoing serial data to complete.
     * @note    This is not used for OTSoftSerial2 as all writes are synchronous.
     */
    virtual void flush() {}
//    /**
//     * @brief   Returns the number of elements in the Tx buffer.
//     * @retval  0 as no Tx buffer implemented.
//     * @note    This is not used for OTSoftSerial2 as all writes are synchronous.
//     */
//    int availableForWrite() { return 0; }
};
#endif // ARDUINO_ARCH_AVR


}

#endif /* CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_ */
